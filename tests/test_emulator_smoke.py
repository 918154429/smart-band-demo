#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SMOKE_SCRIPT = ROOT / "scripts" / "smoke_openvela_emulator.py"
SPEC = importlib.util.spec_from_file_location("emulator_smoke", SMOKE_SCRIPT)
assert SPEC is not None and SPEC.loader is not None
EMULATOR_SMOKE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EMULATOR_SMOKE)


class EmulatorSmokeHelpersTest(unittest.TestCase):
    def test_extract_pidof_ignores_echoed_command(self) -> None:
        output = (
            "pidof smart_band\r\n"
            "smart_band: temperature sensor 29C\r\n"
            "42 77 \r\n"
            "goldfish-armv8a-ap> "
        )
        self.assertEqual(EMULATOR_SMOKE.extract_pidof(output), [42, 77])

    def test_failure_marker_is_case_insensitive(self) -> None:
        self.assertEqual(
            EMULATOR_SMOKE.find_app_failure("fatal: Assertion Failed at ui.c:10"),
            "Assertion failed",
        )
        self.assertIsNone(EMULATOR_SMOKE.find_app_failure("smart_band: UI ready"))

    def test_config_value_decodes_quoted_prompt(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / ".config"
            config.write_text(
                'CONFIG_NSH_PROMPT_STRING="goldfish-armv8a-ap> "\n',
                encoding="utf-8",
            )
            self.assertEqual(
                EMULATOR_SMOKE.config_value(config, "CONFIG_NSH_PROMPT_STRING"),
                "goldfish-armv8a-ap> ",
            )

    def test_runtime_inputs_require_goldfish_partition_images(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            emulator_script = base / "emulator.sh"
            output = base / "output"
            output.mkdir()
            emulator_script.write_text("#!/bin/sh\n", encoding="utf-8")
            for name in (".config", "nuttx", "vela_system.bin"):
                output.joinpath(name).write_bytes(b"test")

            with self.assertRaisesRegex(
                EMULATOR_SMOKE.SmokeFailure, "vela_data.bin"
            ):
                EMULATOR_SMOKE.validate_runtime_inputs(emulator_script, output)

    def test_png_dimensions_reads_the_ihdr_size(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "screen.png"
            image.write_bytes(
                b"\x89PNG\r\n\x1a\n"
                + struct.pack(">I", 13)
                + b"IHDR"
                + struct.pack(">II", 336, 480)
            )
            self.assertEqual(EMULATOR_SMOKE.png_dimensions(image), (336, 480))


@unittest.skipUnless(
    os.name == "posix" and hasattr(os, "openpty"),
    "the emulator smoke harness requires POSIX PTYs",
)
class EmulatorSmokeHarnessTest(unittest.TestCase):
    def test_fake_emulator_exercises_full_runtime_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            openvela = base / "openvela"
            output = openvela / "cmake_out/vela_goldfish-arm64-v8a-ap"
            emulator_root = openvela / "prebuilts/emulator/linux-x86_64"
            renderer_root = emulator_root / "lib64"
            qemu_root = emulator_root / "qemu/linux-x86_64"
            skin_root = openvela / "prebuilts/emulator/skins"
            evidence = base / "evidence"
            home = base / "home"
            for path in (
                output,
                renderer_root,
                qemu_root,
                skin_root,
                evidence,
                home,
            ):
                path.mkdir(parents=True, exist_ok=True)

            output.joinpath(".config").write_text(
                'CONFIG_NSH_PROMPT_STRING="goldfish-armv8a-ap> "\n',
                encoding="utf-8",
            )
            for name in ("nuttx", "vela_system.bin", "vela_data.bin"):
                output.joinpath(name).write_bytes(b"fake runtime input")
            shutil.copy2("/bin/true", emulator_root / "emulator")
            shutil.copy2("/bin/true", renderer_root / "libOpenglRender.so")
            shutil.copy2(
                "/bin/true", qemu_root / "qemu-system-aarch64-headless"
            )

            fake_emulator = openvela / "fake_emulator.py"
            fake_emulator.write_text(
                textwrap.dedent(
                    r'''
                    #!/usr/bin/env python3
                    import os
                    import signal
                    import socket
                    import sys
                    import threading
                    import time

                    args = sys.argv[1:]
                    port = int(args[args.index("-port") + 1])
                    prompt = "goldfish-armv8a-ap> "
                    running = False

                    def console_server():
                        with socket.socket() as server:
                            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                            server.bind(("127.0.0.1", port))
                            server.listen(1)
                            conn, _ = server.accept()
                            with conn:
                                conn.sendall(b"Android Console fake\r\nOK\r\n")
                                buffer = b""
                                while True:
                                    chunk = conn.recv(1024)
                                    if not chunk:
                                        return
                                    buffer += chunk
                                    while b"\n" in buffer:
                                        line, buffer = buffer.split(b"\n", 1)
                                        command = line.strip().decode()
                                        if command == "ping":
                                            conn.sendall(b"I am alive!\r\nOK\r\n")
                                        elif command == "kill":
                                            conn.sendall(b"OK\r\n")
                                            time.sleep(0.05)
                                            os.kill(os.getpid(), signal.SIGTERM)
                                            return
                                        else:
                                            conn.sendall(b"KO: unknown command\r\n")

                    threading.Thread(target=console_server, daemon=True).start()
                    print("NuttShell (NSH)")
                    print(prompt, end="", flush=True)
                    for raw_line in sys.stdin:
                        command = raw_line.strip()
                        if command.startswith("echo "):
                            print(command[5:])
                        elif command == "smart_band &":
                            running = True
                            time.sleep(0.05)
                            print("smart_band: UI ready")
                        elif command == "pidof smart_band":
                            print("42 " if running else "pidof: task smart_band not found")
                        elif command == "ps":
                            if running:
                                print("  42 100 32768 RUNNING smart_band")
                        print(prompt, end="", flush=True)
                    '''
                ).lstrip(),
                encoding="utf-8",
            )
            fake_emulator.chmod(0o755)

            emulator_script = openvela / "emulator.sh"
            emulator_script.write_text(
                '#!/usr/bin/env bash\nexec python3 "$(dirname "$0")/fake_emulator.py" "$@"\n',
                encoding="utf-8",
            )
            emulator_script.chmod(0o755)

            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                console_port = probe.getsockname()[1]

            environment = os.environ.copy()
            environment["HOME"] = str(home)
            result = subprocess.run(
                [
                    sys.executable,
                    str(SMOKE_SCRIPT),
                    "--openvela-root",
                    str(openvela),
                    "--evidence-dir",
                    str(evidence),
                    "--console-port",
                    str(console_port),
                    "--boot-timeout",
                    "5",
                    "--command-timeout",
                    "5",
                    "--settle-seconds",
                    "0.1",
                    "--stability-seconds",
                    "0.1",
                ],
                cwd=ROOT,
                env=environment,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=20,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            runtime = json.loads(
                evidence.joinpath("runtime-smoke.json").read_text(encoding="utf-8")
            )
            self.assertEqual(runtime["status"], "passed")
            self.assertGreater(runtime["app_ui_ready_seconds"], 0.0)
            self.assertLess(runtime["app_ui_ready_seconds"], 1.0)
            self.assertEqual(runtime["stable_pids"], [42])
            self.assertIn(
                "smart_band: UI ready",
                evidence.joinpath("emulator-headless.log").read_text(
                    encoding="utf-8", errors="replace"
                ),
            )
            self.assertTrue(evidence.joinpath("emulator-console-ping.txt").is_file())
            self.assertTrue(evidence.joinpath("emulator-cleanup.txt").is_file())
            self.assertTrue(evidence.joinpath("opengl-renderer-ldd.txt").is_file())


if __name__ == "__main__":
    unittest.main()
