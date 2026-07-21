#!/usr/bin/env python3
"""Boot the pinned openvela emulator and prove smart_band stays alive.

The fixed goldfish configuration exposes NSH on the emulator terminal but does
not enable the ADB shell service.  This smoke test therefore drives the real
NSH terminal through a pseudo-terminal, and uses the Android emulator console
only as an independent liveness and shutdown channel.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import re
import select
import signal
import shutil
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path


APP_FAILURE_MARKERS = (
    "smart_band: LVGL display initialization failed",
    "smart_band: failed to create LVGL UI",
    "smart_band: LVGL already initialized",
    "Assertion failed",
    "Segmentation fault",
    "PANIC",
)


class SmokeFailure(RuntimeError):
    """Raised when a runtime-smoke assertion fails."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the native smart-band app in a headless openvela emulator."
    )
    parser.add_argument("--openvela-root", required=True, type=Path)
    parser.add_argument(
        "--output-dir",
        default="cmake_out/vela_goldfish-arm64-v8a-ap",
        help="openvela output directory, relative to --openvela-root by default",
    )
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--boot-timeout", type=float, default=240.0)
    parser.add_argument("--command-timeout", type=float, default=30.0)
    parser.add_argument("--settle-seconds", type=float, default=10.0)
    parser.add_argument("--stability-seconds", type=float, default=5.0)
    parser.add_argument("--console-port", type=int, default=5554)
    parser.add_argument("--skin", default="xiaomi_smart_screen_10")
    parser.add_argument("--screenshot-width", type=int)
    parser.add_argument("--screenshot-height", type=int)
    return parser.parse_args()


def config_value(config_path: Path, key: str) -> str:
    prefix = f"{key}="
    for line in config_path.read_text(encoding="utf-8").splitlines():
        if not line.startswith(prefix):
            continue
        value = line[len(prefix) :]
        if value.startswith('"'):
            try:
                parsed = ast.literal_eval(value)
            except (SyntaxError, ValueError) as exc:
                raise SmokeFailure(f"invalid {key} in {config_path}: {value}") from exc
            if not isinstance(parsed, str):
                raise SmokeFailure(f"non-string {key} in {config_path}: {value}")
            return parsed
        return value
    raise SmokeFailure(f"missing {key} in {config_path}")


def resolve_under(root: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else root / path


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", errors="replace")


def run_diagnostic(
    command: list[str],
    destination: Path,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=environment,
    )
    write_text(destination, result.stdout)
    return result


def verify_elf(
    binary: Path,
    evidence_dir: Path,
    label: str,
    library_path: Path | None = None,
    require_executable: bool = True,
) -> None:
    if not binary.is_file():
        raise SmokeFailure(f"{label} is missing: {binary}")
    if require_executable and not os.access(binary, os.X_OK):
        raise SmokeFailure(f"{label} is not executable: {binary}")

    file_result = run_diagnostic(
        ["file", str(binary)], evidence_dir / f"{label}-file.txt"
    )
    if file_result.returncode != 0 or "ELF" not in file_result.stdout:
        raise SmokeFailure(f"{label} is not a readable ELF executable")

    environment = None
    if library_path is not None:
        environment = os.environ.copy()
        existing = environment.get("LD_LIBRARY_PATH")
        environment["LD_LIBRARY_PATH"] = (
            f"{library_path}:{existing}" if existing else str(library_path)
        )
    ldd_result = run_diagnostic(
        ["ldd", str(binary)],
        evidence_dir / f"{label}-ldd.txt",
        environment,
    )
    if ldd_result.returncode != 0 or re.search(r"=>\s+not found(?:\s|$)", ldd_result.stdout):
        raise SmokeFailure(f"{label} has unresolved host libraries")


def validate_runtime_inputs(emulator_script: Path, output_dir: Path) -> None:
    required_inputs = (
        emulator_script,
        output_dir / ".config",
        output_dir / "nuttx",
        output_dir / "vela_system.bin",
        output_dir / "vela_data.bin",
    )
    for required in required_inputs:
        if not required.is_file() or required.stat().st_size == 0:
            raise SmokeFailure(f"required emulator input is missing or empty: {required}")


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SmokeFailure(f"screenshot is not a PNG: {path}")
    if header[12:16] != b"IHDR":
        raise SmokeFailure(f"screenshot has no leading IHDR chunk: {path}")
    return struct.unpack(">II", header[16:24])


def capture_screenshot(
    console: EmulatorConsole,
    evidence_dir: Path,
    expected_width: int,
    expected_height: int,
) -> dict[str, object]:
    capture_dir = evidence_dir / "raw-watch-face"
    capture_dir.mkdir()
    response = console.command(
        f"screenrecord screenshot {capture_dir}",
        "emulator-console-screenshot.txt",
    )
    candidates = sorted(capture_dir.rglob("*.png"))
    if len(candidates) != 1:
        raise SmokeFailure(
            f"expected one watch-face screenshot, found {len(candidates)}"
        )
    destination = evidence_dir / "watch-face.png"
    shutil.copy2(candidates[0], destination)
    width, height = png_dimensions(destination)
    if (width, height) != (expected_width, expected_height):
        raise SmokeFailure(
            f"unexpected screenshot size {width}x{height}; "
            f"expected {expected_width}x{expected_height}"
        )
    return {
        "path": str(destination),
        "bytes": destination.stat().st_size,
        "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "width": width,
        "height": height,
        "console_ok": re.search(r"(?:^|\r?\n)OK\r?\n?$", response) is not None,
    }


class PtyChild:
    def __init__(self, command: list[str], cwd: Path, log_path: Path):
        if os.name != "posix" or not hasattr(os, "openpty"):
            raise SmokeFailure("the emulator smoke test requires a POSIX pseudo-terminal")

        self.command = command
        self.cwd = cwd
        self.log_path = log_path
        self.pid: int | None = None
        self.returncode: int | None = None
        self.master_fd: int | None = None
        self.transcript = bytearray()
        self.log_file = log_path.open("wb")

    def start(self) -> None:
        import pty

        pid, master_fd = pty.fork()
        if pid == 0:
            try:
                os.chdir(self.cwd)
                os.execvpe(self.command[0], self.command, os.environ.copy())
            except BaseException as exc:  # pragma: no cover - only runs in child
                os.write(2, f"failed to exec emulator: {exc}\n".encode())
                os._exit(127)

        self.pid = pid
        self.master_fd = master_fd
        os.set_blocking(master_fd, False)

    def poll(self) -> int | None:
        if self.returncode is not None:
            return self.returncode
        if self.pid is None:
            return None

        waited_pid, status = os.waitpid(self.pid, os.WNOHANG)
        if waited_pid == 0:
            return None
        self.returncode = os.waitstatus_to_exitcode(status)
        return self.returncode

    def _read_once(self, timeout: float) -> bool:
        if self.master_fd is None:
            raise SmokeFailure("emulator PTY was not started")

        readable, _, _ = select.select([self.master_fd], [], [], max(0.0, timeout))
        if not readable:
            return False
        try:
            chunk = os.read(self.master_fd, 65536)
        except BlockingIOError:
            return False
        except OSError as exc:
            if self.poll() is not None:
                return False
            raise SmokeFailure(f"failed reading emulator PTY: {exc}") from exc

        if not chunk:
            return False
        self.transcript.extend(chunk)
        self.log_file.write(chunk)
        self.log_file.flush()
        return True

    def wait_for(self, needle: bytes, start: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while True:
            index = self.transcript.find(needle, start)
            if index >= 0:
                return bytes(self.transcript[start : index + len(needle)])

            returncode = self.poll()
            if returncode is not None:
                self._read_once(0.0)
                raise SmokeFailure(
                    f"emulator exited with status {returncode} while waiting for {needle!r}"
                )

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise SmokeFailure(f"timed out waiting for emulator output {needle!r}")
            self._read_once(min(0.25, remaining))

    def pump(self, seconds: float) -> None:
        deadline = time.monotonic() + max(0.0, seconds)
        while time.monotonic() < deadline:
            returncode = self.poll()
            if returncode is not None:
                self._read_once(0.0)
                raise SmokeFailure(f"emulator exited unexpectedly with status {returncode}")
            self._read_once(min(0.25, deadline - time.monotonic()))

    def send_command(
        self,
        command: str,
        prompt: bytes,
        timeout: float,
        destination: Path,
    ) -> str:
        if self.master_fd is None:
            raise SmokeFailure("emulator PTY was not started")
        start = len(self.transcript)
        os.write(self.master_fd, command.encode("utf-8") + b"\n")
        segment = self.wait_for(prompt, start, timeout)
        decoded = segment.decode("utf-8", errors="replace")
        write_text(destination, decoded)
        return decoded

    def stop_process_group(self, cleanup_log: list[str]) -> None:
        if self.pid is None:
            return

        def group_exists() -> bool:
            try:
                os.killpg(self.pid, 0)
            except ProcessLookupError:
                return False
            except PermissionError:
                return True
            return True

        self.poll()
        if group_exists():
            cleanup_log.append(f"sending SIGTERM to emulator process group {self.pid}")
            try:
                os.killpg(self.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass

            deadline = time.monotonic() + 5.0
            while group_exists() and time.monotonic() < deadline:
                self.poll()
                try:
                    self._read_once(0.1)
                except SmokeFailure as exc:
                    cleanup_log.append(f"PTY read during SIGTERM failed: {exc}")
                    time.sleep(0.1)

        if group_exists():
            cleanup_log.append(f"sending SIGKILL to emulator process group {self.pid}")
            try:
                os.killpg(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            deadline = time.monotonic() + 2.0
            while group_exists() and time.monotonic() < deadline:
                self.poll()
                try:
                    self._read_once(0.1)
                except SmokeFailure as exc:
                    cleanup_log.append(f"PTY read during SIGKILL failed: {exc}")
                    time.sleep(0.1)

        self.poll()
        if group_exists():
            cleanup_log.append(f"emulator process group {self.pid} survived SIGKILL")
        cleanup_log.append(f"emulator exit status: {self.poll()}")

    def close(self) -> None:
        if self.master_fd is not None:
            try:
                os.close(self.master_fd)
            except OSError:
                pass
            self.master_fd = None
        self.log_file.close()


def read_console_response(sock: socket.socket, timeout: float = 5.0) -> bytes:
    deadline = time.monotonic() + timeout
    response = bytearray()
    terminator = re.compile(rb"(?:^|\r?\n)(?:OK|KO(?::[^\r\n]*)?)\r?\n$")
    while time.monotonic() < deadline:
        sock.settimeout(max(0.1, deadline - time.monotonic()))
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            break
        response.extend(chunk)
        if terminator.search(bytes(response)):
            break
    return bytes(response)


class EmulatorConsole:
    def __init__(self, sock: socket.socket, evidence_dir: Path):
        self.sock = sock
        self.evidence_dir = evidence_dir

    def command(self, command: str, evidence_name: str, allow_eof: bool = False) -> str:
        self.sock.sendall(command.encode("utf-8") + b"\n")
        response = read_console_response(self.sock)
        text = response.decode("utf-8", errors="replace")
        write_text(self.evidence_dir / evidence_name, text)
        if not allow_eof and not re.search(r"(?:^|\r?\n)OK\r?\n?$", text):
            raise SmokeFailure(f"emulator console rejected {command!r}: {text.strip()}")
        return text

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass


def connect_console(
    port: int, evidence_dir: Path, timeout: float = 30.0
) -> EmulatorConsole:
    deadline = time.monotonic() + timeout
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=2.0)
            greeting = read_console_response(sock)
            greeting_text = greeting.decode("utf-8", errors="replace")
            write_text(evidence_dir / "emulator-console-greeting.txt", greeting_text)

            console = EmulatorConsole(sock, evidence_dir)
            if "Authentication required" in greeting_text:
                token_path = Path.home() / ".emulator_console_auth_token"
                token_deadline = time.monotonic() + 10.0
                while not token_path.is_file() and time.monotonic() < token_deadline:
                    time.sleep(0.1)
                if not token_path.is_file():
                    raise SmokeFailure(f"emulator console token was not created: {token_path}")
                token = token_path.read_text(encoding="utf-8").strip()
                if not token:
                    raise SmokeFailure(f"emulator console token is empty: {token_path}")
                console.command(f"auth {token}", "emulator-console-auth.txt")
            return console
        except (OSError, SmokeFailure) as exc:
            last_error = exc
            try:
                sock.close()
            except (OSError, UnboundLocalError):
                pass
            time.sleep(0.5)
    raise SmokeFailure(f"emulator console did not become ready on port {port}: {last_error}")


def extract_pidof(output: str) -> list[int]:
    clean = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", output).replace("\r", "")
    for line in clean.splitlines():
        candidate = line.strip()
        if re.fullmatch(r"\d+(?:\s+\d+)*", candidate):
            return [int(value) for value in candidate.split()]
    return []


def find_app_failure(output: str) -> str | None:
    lowered = output.lower()
    for marker in APP_FAILURE_MARKERS:
        if marker.lower() in lowered:
            return marker
    return None


def log_tail(path: Path, line_count: int = 80) -> str:
    if not path.is_file():
        return ""
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return "\n".join(lines[-line_count:])


def main() -> int:
    args = parse_args()
    if (args.screenshot_width is None) != (args.screenshot_height is None):
        raise SmokeFailure(
            "--screenshot-width and --screenshot-height must be provided together"
        )
    if args.screenshot_width is not None and (
        args.screenshot_width <= 0 or args.screenshot_height <= 0
    ):
        raise SmokeFailure("screenshot dimensions must be positive")
    root = args.openvela_root.resolve()
    output_dir = resolve_under(root, args.output_dir).resolve()
    evidence_dir = args.evidence_dir.resolve()
    evidence_dir.mkdir(parents=True, exist_ok=True)

    emulator_script = root / "emulator.sh"
    emulator_binary = root / "prebuilts/emulator/linux-x86_64/emulator"
    emulator_library_path = emulator_binary.parent / "lib64"
    opengl_renderer = emulator_library_path / "libOpenglRender.so"
    qemu_headless = (
        root
        / "prebuilts/emulator/linux-x86_64/qemu/linux-x86_64/"
        "qemu-system-aarch64-headless"
    )
    skin_dir = root / "prebuilts/emulator/skins"
    config_path = output_dir / ".config"
    log_path = evidence_dir / "emulator-headless.log"

    validate_runtime_inputs(emulator_script, output_dir)
    if not skin_dir.is_dir():
        raise SmokeFailure(f"emulator skin directory is missing: {skin_dir}")
    if not (skin_dir / args.skin).is_dir():
        raise SmokeFailure(f"emulator skin is missing: {skin_dir / args.skin}")
    if not qemu_headless.is_file() or not os.access(qemu_headless, os.X_OK):
        raise SmokeFailure(f"headless aarch64 QEMU binary is missing: {qemu_headless}")

    verify_elf(emulator_binary, evidence_dir, "emulator")
    verify_elf(
        qemu_headless,
        evidence_dir,
        "qemu-headless",
        emulator_library_path,
    )
    verify_elf(
        opengl_renderer,
        evidence_dir,
        "opengl-renderer",
        emulator_library_path,
        require_executable=False,
    )
    prompt_text = config_value(config_path, "CONFIG_NSH_PROMPT_STRING")
    prompt = prompt_text.encode("utf-8")

    command = [
        str(emulator_script),
        str(output_dir),
        "-no-window",
        "-no-audio",
        "-accel",
        "off",
        "-port",
        str(args.console_port),
        "-skin",
        args.skin,
        "-skindir",
        str(skin_dir),
    ]
    write_text(evidence_dir / "emulator-command.txt", " ".join(command) + "\n")

    child = PtyChild(command, root, log_path)
    console: EmulatorConsole | None = None
    cleanup_log: list[str] = []
    started_at = time.monotonic()
    app_transcript_start = 0
    handled_signals = (signal.SIGTERM, signal.SIGINT)
    previous_handlers = {sig: signal.getsignal(sig) for sig in handled_signals}
    interrupted = False

    def handle_signal(signum: int, _frame: object) -> None:
        nonlocal interrupted
        if interrupted:
            return
        interrupted = True
        raise SmokeFailure(f"received {signal.Signals(signum).name}")

    try:
        for handled_signal in handled_signals:
            signal.signal(handled_signal, handle_signal)
        child.start()
        if child.pid is None:
            raise SmokeFailure("emulator PTY did not return a child PID")
        write_text(
            evidence_dir / "emulator-process.json",
            json.dumps(
                {"pid": child.pid, "pgid": os.getpgid(child.pid)},
                indent=2,
                sort_keys=True,
            )
            + "\n",
        )
        child.wait_for(prompt, 0, args.boot_timeout)
        boot_seconds = time.monotonic() - started_at

        ready_marker = "SMART_BAND_NSH_READY"
        ready_output = child.send_command(
            f"echo {ready_marker}",
            prompt,
            args.command_timeout,
            evidence_dir / "nsh-ready.txt",
        )
        if ready_marker not in [line.strip() for line in ready_output.replace("\r", "").splitlines()]:
            raise SmokeFailure("NSH did not execute the readiness command")

        console = connect_console(args.console_port, evidence_dir)
        ping_output = console.command("ping", "emulator-console-ping.txt")
        if "I am alive!" not in ping_output:
            raise SmokeFailure("emulator console ping did not report liveness")

        app_transcript_start = len(child.transcript)
        app_started_at = time.monotonic()
        child.send_command(
            "smart_band &",
            prompt,
            args.command_timeout,
            evidence_dir / "smart-band-launch.txt",
        )
        settle_started_at = time.monotonic()
        child.wait_for(
            b"smart_band: UI ready",
            app_transcript_start,
            args.command_timeout,
        )
        app_ui_ready_seconds = time.monotonic() - app_started_at
        settle_elapsed = time.monotonic() - settle_started_at
        child.pump(max(0.0, args.settle_seconds - settle_elapsed))

        app_output = bytes(child.transcript[app_transcript_start:]).decode(
            "utf-8", errors="replace"
        )
        if "smart_band: UI ready" not in app_output:
            raise SmokeFailure("smart_band did not report successful LVGL UI creation")

        screenshot = None
        if args.screenshot_width is not None:
            screenshot = capture_screenshot(
                console,
                evidence_dir,
                args.screenshot_width,
                args.screenshot_height,
            )

        pid_output_1 = child.send_command(
            "pidof smart_band",
            prompt,
            args.command_timeout,
            evidence_dir / "smart-band-pidof-1.txt",
        )
        pids_1 = extract_pidof(pid_output_1)
        if not pids_1:
            raise SmokeFailure("smart_band was not running after the initial settle period")

        child.pump(args.stability_seconds)
        pid_output_2 = child.send_command(
            "pidof smart_band",
            prompt,
            args.command_timeout,
            evidence_dir / "smart-band-pidof-2.txt",
        )
        pids_2 = extract_pidof(pid_output_2)
        if not pids_2:
            raise SmokeFailure("smart_band did not remain alive for the stability interval")

        ps_output = child.send_command(
            "ps",
            prompt,
            args.command_timeout,
            evidence_dir / "smart-band-ps.txt",
        )
        if not re.search(r"(?<![A-Za-z0-9_])smart_band(?![A-Za-z0-9_])", ps_output):
            raise SmokeFailure("NSH ps output did not contain the smart_band task")

        app_output = bytes(child.transcript[app_transcript_start:]).decode(
            "utf-8", errors="replace"
        )
        failure_marker = find_app_failure(app_output)
        if failure_marker:
            raise SmokeFailure(f"application emitted fatal marker: {failure_marker}")
        if child.poll() is not None:
            raise SmokeFailure("emulator exited before runtime smoke completed")

        result = {
            "status": "passed",
            "boot_seconds": round(boot_seconds, 3),
            "app_ui_ready_seconds": round(app_ui_ready_seconds, 3),
            "console_port": args.console_port,
            "skin": args.skin,
            "screenshot": screenshot,
            "nsh_prompt": prompt_text,
            "initial_pids": pids_1,
            "stable_pids": pids_2,
            "settle_seconds": args.settle_seconds,
            "stability_seconds": args.stability_seconds,
        }
        write_text(
            evidence_dir / "runtime-smoke.json",
            json.dumps(result, indent=2, sort_keys=True) + "\n",
        )
        print(json.dumps(result, sort_keys=True))
        return 0
    except SmokeFailure as exc:
        print(f"runtime smoke failed: {exc}", file=sys.stderr)
        tail = log_tail(log_path)
        if tail:
            print("--- emulator log tail ---", file=sys.stderr)
            print(tail, file=sys.stderr)
        return 1
    finally:
        for handled_signal in handled_signals:
            signal.signal(handled_signal, signal.SIG_IGN)
        if console is not None:
            try:
                console.command("kill", "emulator-console-kill.txt", allow_eof=True)
                cleanup_log.append("requested emulator shutdown through console")
            except (OSError, SmokeFailure) as exc:
                cleanup_log.append(f"console shutdown failed: {exc}")
            console.close()
        child.stop_process_group(cleanup_log)
        child.close()
        write_text(evidence_dir / "emulator-cleanup.txt", "\n".join(cleanup_log) + "\n")
        for handled_signal, previous_handler in previous_handlers.items():
            signal.signal(handled_signal, previous_handler)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SmokeFailure as exc:
        print(f"runtime smoke failed before launch: {exc}", file=sys.stderr)
        raise SystemExit(1)
