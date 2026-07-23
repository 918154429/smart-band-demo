#!/usr/bin/env python3
"""Run the four-boot native Q4 notification journey and collect evidence."""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import importlib.util
import json
import os
import re
import secrets
import signal
import time
import traceback
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_DIR = "cmake_out/vela_goldfish-arm64-v8a-ap"
SCENARIOS = ("ordinary", "center", "calls", "workout")
Q4_STATE_MARKER = "smart_band:q4:v1"
Q4_INJECT_MARKER = "smart_band:q4:inject:v1"
Q4_HAPTIC_MARKER = "smart_band:q4:haptic:v1"
Q4_WAKE_MARKER = "smart_band:q4:wake:v1"
MIN_STACK_MARGIN_PERCENT = 25.0
Q4_FIXTURE_IDS = {
    "ordinary": (701,),
    "center": (711, 712, 713, 714, 715),
    "calls": (721, 722),
    "workout": (731,),
}
INJECT_PHASES = {
    "ordinary": {"initial", "updated"},
    "center": {"ready"},
    "calls": {"ready"},
    "workout": {"armed", "active"},
}

# Touch coordinates are expressed in the same 330x626 logical input space as
# run_q3_native_e2e.py. Keep every Q4-specific point here so a reviewed native
# run can adjust one table without changing scenario logic.
NOTIFICATION_LAUNCHER_POINT = (90, 210)
OVERLAY_BODY_POINT = (90, 90)
OVERLAY_DRAG_START = (250, 90)
OVERLAY_DRAG_END = (60, 90)
OVERLAY_DISMISS_POINT = (265, 137)
OVERLAY_REJECT_POINT = (177, 137)
CENTER_FIRST_READ_POINT = (228, 183)
CENTER_FIRST_DELETE_POINT = (295, 183)
CALL_REJECT_POINT = (90, 560)
CALL_ACCEPT_POINT = (240, 560)

Q4_STATE_FIELDS = {
    "elapsed_ms",
    "notifications",
    "dnd",
    "active_id",
    "active_generation",
    "presentation",
    "haptic_events",
    "wake_requests",
    "haptic_retries",
    "haptic_log_dropped",
    "wake_log_dropped",
    "pending_effects",
    "inbox_dropped",
}
ANSI_ESCAPE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


class Q4NativeFailure(RuntimeError):
    """Raised when a native Q4 assertion is not proved."""


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise Q4NativeFailure(f"cannot load helper module: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


Q3 = load_module("q4_native_q3", ROOT / "scripts" / "run_q3_native_e2e.py")
NATIVE = Q3.NATIVE
SMOKE = NATIVE.SMOKE


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run four isolated native Q4 notification scenarios"
    )
    parser.add_argument("--openvela-root", required=True, type=Path)
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--console-port", type=int, default=5575)
    parser.add_argument("--boot-timeout", type=float, default=120.0)
    parser.add_argument("--command-timeout", type=float, default=20.0)
    parser.add_argument("--ui-settle-seconds", type=float, default=0.5)
    parser.add_argument("--marker-timeout", type=float, default=12.0)
    return parser.parse_args()


def validate_settings(args: argparse.Namespace) -> None:
    if not 1 <= args.console_port <= 65535:
        raise Q4NativeFailure("console port must be between 1 and 65535")
    for name in (
        "boot_timeout",
        "command_timeout",
        "ui_settle_seconds",
        "marker_timeout",
    ):
        if getattr(args, name) <= 0:
            raise Q4NativeFailure(f"{name.replace('_', '-')} must be positive")


def local_point(point: tuple[int, int]) -> tuple[int, int]:
    return Q3.local_point(*point)


def app_launch_command(scenario: str) -> str:
    if scenario not in SCENARIOS:
        raise Q4NativeFailure(f"unsupported Q4 native scenario: {scenario}")
    return f"smart_band --q4-native-scenario={scenario} &"


def _marker_tokens(
    line: str, marker: str, expected_keys: set[str]
) -> dict[str, str] | None:
    clean = ANSI_ESCAPE.sub("", line).replace("\r", "")
    start = clean.find(marker)
    if start < 0:
        return None
    suffix = clean[start + len(marker) :]
    if not suffix.startswith(" "):
        raise Q4NativeFailure(f"malformed {marker} marker boundary: {clean!r}")
    fields: dict[str, str] = {}
    for token in suffix.strip().split():
        if token.count("=") != 1:
            raise Q4NativeFailure(f"malformed {marker} token: {token!r}")
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            raise Q4NativeFailure(f"invalid or duplicate {marker} field: {token!r}")
        fields[key] = value
    if set(fields) != expected_keys:
        missing = sorted(expected_keys - set(fields))
        extra = sorted(set(fields) - expected_keys)
        raise Q4NativeFailure(
            f"{marker} fields do not match contract; missing={missing} extra={extra}"
        )
    return fields


def _nonnegative_int(value: str, marker: str, field: str) -> int:
    if not re.fullmatch(r"[0-9]+", value):
        raise Q4NativeFailure(f"{marker} {field} is not a nonnegative integer")
    return int(value)


def parse_q4_state(line: str) -> dict[str, int] | None:
    raw = _marker_tokens(line, Q4_STATE_MARKER, Q4_STATE_FIELDS)
    if raw is None:
        return None
    state = {
        key: _nonnegative_int(value, Q4_STATE_MARKER, key)
        for key, value in raw.items()
    }
    if state["dnd"] not in (0, 1) or state["presentation"] not in (0, 1, 2):
        raise Q4NativeFailure(f"invalid Q4 state enum values: {state}")
    if state["active_id"] == 0 and (
        state["active_generation"] != 0 or state["presentation"] != 0
    ):
        raise Q4NativeFailure(f"inactive Q4 state retains presentation data: {state}")
    if state["active_id"] != 0 and (
        state["active_generation"] == 0 or state["presentation"] == 0
    ):
        raise Q4NativeFailure(f"active Q4 state lacks presentation data: {state}")
    return state


def parse_inject_marker(line: str) -> dict[str, Any] | None:
    raw = _marker_tokens(
        line,
        Q4_INJECT_MARKER,
        {"scenario", "phase", "accepted", "requested"},
    )
    if raw is None:
        return None
    scenario = raw["scenario"]
    phase = raw["phase"]
    if scenario not in SCENARIOS or phase not in INJECT_PHASES[scenario]:
        raise Q4NativeFailure(
            f"invalid Q4 inject scenario/phase: {scenario}/{phase}"
        )
    accepted = _nonnegative_int(raw["accepted"], Q4_INJECT_MARKER, "accepted")
    requested = _nonnegative_int(raw["requested"], Q4_INJECT_MARKER, "requested")
    if accepted > requested:
        raise Q4NativeFailure(
            f"Q4 inject accepted exceeds requested: {accepted}>{requested}"
        )
    return {
        "scenario": scenario,
        "phase": phase,
        "accepted": accepted,
        "requested": requested,
    }


def parse_haptic_marker(line: str) -> dict[str, Any] | None:
    raw = _marker_tokens(
        line,
        Q4_HAPTIC_MARKER,
        {"notification_id", "generation", "pattern", "simulated"},
    )
    if raw is None:
        return None
    notification_id = _nonnegative_int(
        raw["notification_id"], Q4_HAPTIC_MARKER, "notification_id"
    )
    generation = _nonnegative_int(
        raw["generation"], Q4_HAPTIC_MARKER, "generation"
    )
    if notification_id == 0 or generation == 0:
        raise Q4NativeFailure("Q4 haptic marker has a zero identity")
    if raw["pattern"] not in {"subtle", "normal", "urgent"}:
        raise Q4NativeFailure(f"invalid Q4 haptic pattern: {raw['pattern']}")
    if raw["simulated"] != "1":
        raise Q4NativeFailure("Q4 haptic marker must be explicitly simulated")
    return {
        "notification_id": notification_id,
        "generation": generation,
        "pattern": raw["pattern"],
        "simulated": 1,
    }


def parse_wake_marker(line: str) -> dict[str, Any] | None:
    raw = _marker_tokens(
        line,
        Q4_WAKE_MARKER,
        {
            "notification_id",
            "generation",
            "reason",
            "synthetic",
            "power_transition",
        },
    )
    if raw is None:
        return None
    notification_id = _nonnegative_int(
        raw["notification_id"], Q4_WAKE_MARKER, "notification_id"
    )
    generation = _nonnegative_int(
        raw["generation"], Q4_WAKE_MARKER, "generation"
    )
    if notification_id == 0 or generation == 0:
        raise Q4NativeFailure("Q4 wake marker has a zero identity")
    if (
        raw["reason"] != "notification"
        or raw["synthetic"] != "1"
        or raw["power_transition"] != "0"
    ):
        raise Q4NativeFailure(f"invalid synthetic Q4 wake contract: {raw}")
    return {
        "notification_id": notification_id,
        "generation": generation,
        "reason": "notification",
        "synthetic": 1,
        "power_transition": 0,
    }


def marker_records(
    transcript: bytes | bytearray,
    marker: str,
    parser: Callable[[str], dict[str, Any] | None],
) -> list[dict[str, Any]]:
    text = bytes(transcript).decode("utf-8", errors="replace").replace("\r", "")
    records: list[dict[str, Any]] = []
    for line in text.splitlines():
        if marker not in line:
            continue
        parsed = parser(line)
        if parsed is None:
            raise Q4NativeFailure(f"marker parser ignored matching line: {line!r}")
        records.append(parsed)
    return records


def wait_for_marker(
    child: Any,
    marker: str,
    parser: Callable[[str], dict[str, Any] | None],
    predicate: Callable[[dict[str, Any]], bool],
    timeout: float,
    description: str,
    start: int = 0,
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    seen = 0
    while time.monotonic() < deadline:
        records = marker_records(child.transcript[start:], marker, parser)
        for record in records[seen:]:
            if predicate(record):
                return record
        seen = len(records)
        if child.poll() is not None:
            raise Q4NativeFailure(
                f"app/emulator exited while waiting for {description}"
            )
        child.pump(min(0.25, max(0.0, deadline - time.monotonic())))
    records = marker_records(child.transcript[start:], marker, parser)
    raise Q4NativeFailure(
        f"timed out waiting for {description}; latest={records[-1] if records else None}"
    )


def wait_for_q3_state(
    child: Any,
    predicate: Callable[[dict[str, int]], bool],
    timeout: float,
    description: str,
    start: int = 0,
) -> dict[str, int]:
    deadline = time.monotonic() + timeout
    seen = 0
    while time.monotonic() < deadline:
        states = Q3.marker_states(child.transcript[start:])
        for state in states[seen:]:
            if predicate(state):
                return state
        seen = len(states)
        if child.poll() is not None:
            raise Q4NativeFailure(
                f"app/emulator exited while waiting for {description}"
            )
        child.pump(min(0.25, max(0.0, deadline - time.monotonic())))
    states = Q3.marker_states(child.transcript[start:])
    raise Q4NativeFailure(
        f"timed out waiting for {description}; latest={states[-1] if states else None}"
    )


def parse_ps_stack(output: str) -> dict[str, Any]:
    clean = ANSI_ESCAPE.sub("", output).replace("\r", "")
    header = next(
        (
            line
            for line in clean.splitlines()
            if re.search(r"\bPID\b", line)
            and re.search(r"\bSTACK\b", line)
            and re.search(r"\bUSED\b", line)
            and re.search(r"\bFILLED\b", line)
            and re.search(r"\bCOMMAND\b", line)
        ),
        None,
    )
    if header is None:
        raise Q4NativeFailure("NSH ps output lacks stack coloration columns")
    rows: list[dict[str, Any]] = []
    row_pattern = re.compile(
        r"^\s*(?P<pid>[0-9]+)\s+(?P<group>[0-9]+)\s+.*\s+"
        r"(?P<stack>[0-9]+)\s+(?P<used>[0-9]+)\s+"
        r"(?P<filled>[0-9]+(?:\.[0-9]+)?)%\s+"
        r"smart_band(?:\s.*)?$"
    )
    for line in clean.splitlines():
        match = row_pattern.match(line)
        if match is None:
            continue
        row: dict[str, Any] = {
            "pid": int(match.group("pid")),
            "group": int(match.group("group")),
            "stack_size_bytes": int(match.group("stack")),
            "stack_used_bytes": int(match.group("used")),
            "filled_percent": float(match.group("filled")),
        }
        rows.append(row)
    if len(rows) != 1:
        raise Q4NativeFailure(
            f"expected exactly one smart_band stack row, got {len(rows)}"
        )
    row = rows[0]
    if (
        row["stack_size_bytes"] <= 0
        or row["stack_used_bytes"] <= 0
        or row["stack_used_bytes"] > row["stack_size_bytes"]
    ):
        raise Q4NativeFailure(f"invalid smart_band stack values: {row}")
    calculated = row["stack_used_bytes"] * 100.0 / row["stack_size_bytes"]
    if abs(calculated - row["filled_percent"]) > 0.2:
        raise Q4NativeFailure(
            f"smart_band FILLED does not match STACK/USED: {row}"
        )
    row["margin_bytes"] = row["stack_size_bytes"] - row["stack_used_bytes"]
    row["margin_percent"] = (
        row["margin_bytes"] * 100.0 / row["stack_size_bytes"]
    )
    if row["margin_percent"] < MIN_STACK_MARGIN_PERCENT:
        raise Q4NativeFailure(
            "smart_band stack margin is below "
            f"{MIN_STACK_MARGIN_PERCENT:.1f}%: {row}"
        )
    return row


def summarize_stack_samples(samples: list[dict[str, Any]]) -> dict[str, Any]:
    if not samples:
        raise Q4NativeFailure("no smart_band stack samples were collected")
    if {sample["scenario"] for sample in samples} != set(SCENARIOS):
        raise Q4NativeFailure("stack evidence does not cover all Q4 scenarios")
    peak_used = max(samples, key=lambda sample: sample["stack_used_bytes"])
    peak_filled = max(samples, key=lambda sample: sample["filled_percent"])
    return {
        "min_effective_stack_size_bytes": min(
            sample["stack_size_bytes"] for sample in samples
        ),
        "max_effective_stack_size_bytes": max(
            sample["stack_size_bytes"] for sample in samples
        ),
        "max_stack_used_bytes": peak_used["stack_used_bytes"],
        "minimum_margin_bytes": min(sample["margin_bytes"] for sample in samples),
        "minimum_margin_percent": min(
            sample["margin_bytes"] * 100.0 / sample["stack_size_bytes"]
            for sample in samples
        ),
        "max_filled_percent": peak_filled["filled_percent"],
        "peak_used_scenario": peak_used["scenario"],
        "peak_used_checkpoint": peak_used["checkpoint"],
        "peak_filled_scenario": peak_filled["scenario"],
        "peak_filled_checkpoint": peak_filled["checkpoint"],
        "sample_count": len(samples),
        "scenario_count": len({sample["scenario"] for sample in samples}),
    }


def require_quiescent_effect_pipeline(state: dict[str, int]) -> None:
    for key in (
        "haptic_retries",
        "haptic_log_dropped",
        "wake_log_dropped",
        "pending_effects",
        "inbox_dropped",
    ):
        if state[key] != 0:
            raise Q4NativeFailure(f"Q4 effect/input invariant failed: {key}={state[key]}")


def effect_pairs(
    transcript: bytes | bytearray,
    expected_haptic_ids: list[int],
    expected_wake_ids: list[int] | None = None,
    require_identity_pairs: bool = True,
) -> dict[str, Any]:
    if expected_wake_ids is None:
        expected_wake_ids = expected_haptic_ids
    haptics = marker_records(
        transcript, Q4_HAPTIC_MARKER, parse_haptic_marker
    )
    wakes = marker_records(transcript, Q4_WAKE_MARKER, parse_wake_marker)
    haptic_keys = [
        (record["notification_id"], record["generation"]) for record in haptics
    ]
    wake_keys = [
        (record["notification_id"], record["generation"]) for record in wakes
    ]
    if len(set(haptic_keys)) != len(haptic_keys):
        raise Q4NativeFailure(f"duplicate Q4 haptic identities: {haptic_keys}")
    if len(set(wake_keys)) != len(wake_keys):
        raise Q4NativeFailure(f"duplicate Q4 wake identities: {wake_keys}")
    if require_identity_pairs and set(haptic_keys) != set(wake_keys):
        raise Q4NativeFailure(
            f"Q4 haptic/wake identities differ: {haptic_keys} != {wake_keys}"
        )
    if collections.Counter(
        record["notification_id"] for record in haptics
    ) != collections.Counter(expected_haptic_ids):
        raise Q4NativeFailure(
            f"unexpected Q4 haptic notification IDs: {haptic_keys}"
        )
    if collections.Counter(
        record["notification_id"] for record in wakes
    ) != collections.Counter(expected_wake_ids):
        raise Q4NativeFailure(
            f"unexpected Q4 wake notification IDs: {wake_keys}"
        )
    return {
        "haptics": haptics,
        "wakes": wakes,
        "paired_identities": [
            {"notification_id": item[0], "generation": item[1]}
            for item in sorted(haptic_keys)
        ],
    }


def send_drag(
    boot: "Boot",
    name: str,
    start: tuple[int, int],
    end: tuple[int, int],
) -> None:
    if boot.console is None or boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    start_x, start_y = local_point(start)
    end_x, end_y = local_point(end)
    steps = 5
    commands = []
    for index in range(steps + 1):
        x = start_x + (end_x - start_x) * index // steps
        y = start_y + (end_y - start_y) * index // steps
        pressed = 0 if index == steps else 1
        commands.append(f"event mouse {x} {y} 0 {pressed}")
    for index, command in enumerate(commands, 1):
        response = boot.console.command(
            command, f"console-{name}-{index:02d}.txt"
        )
        if not NATIVE.console_response_ok(response):
            raise Q4NativeFailure(f"console rejected {name}: {response!r}")
        boot.child.pump(0.03)


class Boot:
    def __init__(
        self,
        scenario: str,
        args: argparse.Namespace,
        openvela_root: Path,
        runtime_output: Path,
        evidence_dir: Path,
        run_id: str,
    ) -> None:
        self.scenario = scenario
        self.args = args
        self.openvela_root = openvela_root
        self.runtime_output = runtime_output
        self.evidence_dir = evidence_dir
        self.run_id = run_id
        self.child = None
        self.console = None
        self.launch_offset = 0
        self.stack_samples: list[dict[str, Any]] = []
        self.cleanup_log: list[str] = []
        self.process_cleanup: dict[str, Any] | None = None
        self.prompt = SMOKE.config_value(
            runtime_output / ".config", "CONFIG_NSH_PROMPT_STRING"
        ).encode("utf-8")
        emulator_root = openvela_root / "prebuilts/emulator/linux-x86_64"
        self.executables = (
            emulator_root / "emulator",
            emulator_root / "qemu/linux-x86_64/qemu-system-aarch64-headless",
        )
        self.process_baseline = NATIVE.attributed_process_snapshot(
            run_id, runtime_output, self.executables
        )

    def write_process_metadata(
        self, pid: int | None = None, pgid: int | None = None
    ) -> None:
        process_file = self.evidence_dir / "emulator-process.json"
        temporary = process_file.with_suffix(".json.tmp")
        metadata: dict[str, Any] = {
            "run_id": self.run_id,
            "runtime_output": str(self.runtime_output.resolve()),
            "executables": [str(path.resolve()) for path in self.executables],
            "baseline": self.process_baseline,
        }
        if pid is not None and pgid is not None:
            metadata.update({"pid": pid, "pgid": pgid})
        NATIVE.write_json(temporary, metadata)
        os.replace(temporary, process_file)

    def start(self) -> None:
        skin_dir = self.openvela_root / "prebuilts/emulator/skins"
        command = [
            str(self.openvela_root / "emulator.sh"),
            str(self.runtime_output),
            "-no-window",
            "-no-audio",
            "-accel",
            "off",
            "-port",
            str(self.args.console_port),
            "-skin",
            "xiaomi_smart_screen_10",
            "-skindir",
            str(skin_dir),
        ]
        NATIVE.write_text(
            self.evidence_dir / "emulator-command.txt", " ".join(command) + "\n"
        )
        previous = os.environ.get(NATIVE.RUN_ID_ENV)
        os.environ[NATIVE.RUN_ID_ENV] = self.run_id
        self.write_process_metadata()
        self.child = SMOKE.PtyChild(
            command, self.openvela_root, self.evidence_dir / "emulator.log"
        )
        try:
            self.child.start()
        finally:
            if previous is None:
                os.environ.pop(NATIVE.RUN_ID_ENV, None)
            else:
                os.environ[NATIVE.RUN_ID_ENV] = previous
        if self.child.pid is None:
            raise Q4NativeFailure("emulator PTY did not return a child PID")
        self.write_process_metadata(
            self.child.pid, os.getpgid(self.child.pid)
        )
        self.child.wait_for(self.prompt, 0, self.args.boot_timeout)
        ready = f"Q4_{self.scenario.upper()}_BOOT_READY"
        response = self.child.send_command(
            f"echo {ready}",
            self.prompt,
            self.args.command_timeout,
            self.evidence_dir / "nsh-ready.txt",
        )
        if ready not in [
            line.strip() for line in response.replace("\r", "").splitlines()
        ]:
            raise Q4NativeFailure(f"{self.scenario} NSH readiness failed")
        self.console = SMOKE.connect_console(
            self.args.console_port, self.evidence_dir
        )
        ping = self.console.command("ping", "console-ping.txt")
        if "I am alive!" not in ping or not NATIVE.console_response_ok(ping):
            raise Q4NativeFailure(f"{self.scenario} emulator console ping failed")
        self.launch_offset = len(self.child.transcript)
        self.child.send_command(
            app_launch_command(self.scenario),
            self.prompt,
            self.args.command_timeout,
            self.evidence_dir / "nsh-launch.txt",
        )
        self.child.wait_for(
            b"smart_band: UI ready",
            self.launch_offset,
            self.args.command_timeout,
        )
        self.child.pump(self.args.ui_settle_seconds)

    def click(self, name: str, point: tuple[int, int]) -> None:
        if self.console is None or self.child is None:
            raise Q4NativeFailure("emulator is not running")
        Q3.click(
            self.console,
            self.child,
            self.evidence_dir,
            name,
            local_point(point),
        )

    def screenshot(self, name: str) -> dict[str, Any]:
        if self.console is None:
            raise Q4NativeFailure("emulator console is not connected")
        _image, record = NATIVE.capture_screenshot(
            self.console, self.evidence_dir, name
        )
        if not record["console_ok"] or not record["nonblank"]:
            raise Q4NativeFailure(f"native screenshot failed validation: {name}")
        return record

    def sample_stack(self, checkpoint: str) -> dict[str, Any]:
        if self.child is None:
            raise Q4NativeFailure("emulator is not running")
        output = self.child.send_command(
            "ps",
            self.prompt,
            self.args.command_timeout,
            self.evidence_dir / f"nsh-ps-{len(self.stack_samples) + 1:02d}-{checkpoint}.txt",
        )
        sample = parse_ps_stack(output)
        sample.update({"scenario": self.scenario, "checkpoint": checkpoint})
        self.stack_samples.append(sample)
        return sample

    def markers(self) -> dict[str, Any]:
        if self.child is None:
            raise Q4NativeFailure("emulator is not running")
        transcript = self.child.transcript[self.launch_offset :]
        return {
            "states": marker_records(
                transcript, Q4_STATE_MARKER, parse_q4_state
            ),
            "inject": marker_records(
                transcript, Q4_INJECT_MARKER, parse_inject_marker
            ),
            "haptic": marker_records(
                transcript, Q4_HAPTIC_MARKER, parse_haptic_marker
            ),
            "wake": marker_records(
                transcript, Q4_WAKE_MARKER, parse_wake_marker
            ),
        }

    def stop(self) -> None:
        if self.console is not None:
            try:
                self.console.command("kill", "console-kill.txt", allow_eof=True)
            except Exception as exc:
                self.cleanup_log.append(f"console kill failed: {exc}")
            self.console.close()
            self.console = None
        if self.child is not None:
            try:
                self.child.stop_process_group(self.cleanup_log)
            finally:
                self.child.close()
                self.child = None
        self.process_cleanup = NATIVE.cleanup_attributed_processes(
            self.run_id,
            self.runtime_output,
            self.executables,
            self.process_baseline,
        )
        if not self.process_cleanup["absent"]:
            raise Q4NativeFailure(
                f"{self.scenario} attributed emulator processes survived cleanup"
            )
        if not NATIVE.wait_for_port_available(self.args.console_port):
            raise Q4NativeFailure(
                f"console port remained occupied after {self.scenario}"
            )
        NATIVE.write_text(
            self.evidence_dir / "cleanup.txt",
            "\n".join(self.cleanup_log) + "\n",
        )


def wait_inject(
    boot: Boot, phase: str, accepted: int, requested: int
) -> dict[str, Any]:
    if boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    return wait_for_marker(
        boot.child,
        Q4_INJECT_MARKER,
        parse_inject_marker,
        lambda item: item
        == {
            "scenario": boot.scenario,
            "phase": phase,
            "accepted": accepted,
            "requested": requested,
        },
        boot.args.marker_timeout,
        f"{boot.scenario}/{phase} inject marker",
        boot.launch_offset,
    )


def wait_q4_state(
    boot: Boot,
    predicate: Callable[[dict[str, int]], bool],
    description: str,
    start: int | None = None,
) -> dict[str, int]:
    if boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    record = wait_for_marker(
        boot.child,
        Q4_STATE_MARKER,
        parse_q4_state,
        predicate,
        boot.args.marker_timeout,
        description,
        boot.launch_offset if start is None else start,
    )
    require_quiescent_effect_pipeline(record)
    return record


def run_ordinary(boot: Boot) -> dict[str, Any]:
    if boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    record: dict[str, Any] = {"scenario": "ordinary", "screenshots": {}}
    record["initial_inject"] = wait_inject(boot, "initial", 1, 1)
    initial = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 1
        and state["active_id"] == 701
        and state["presentation"] == 1
        and state["haptic_events"] >= 1
        and state["wake_requests"] == 0,
        "ordinary initial overlay",
    )
    record["initial_state"] = initial
    record["screenshots"]["initial"] = boot.screenshot("ordinary-initial")
    boot.sample_stack("initial")

    q3_before = wait_for_q3_state(
        boot.child,
        lambda state: True,
        boot.args.marker_timeout,
        "ordinary pre-isolation Q3 state",
        boot.launch_offset,
    )
    interaction_start = len(boot.child.transcript)
    boot.click("ordinary-overlay-body", OVERLAY_BODY_POINT)
    send_drag(
        boot,
        "ordinary-overlay-drag",
        OVERLAY_DRAG_START,
        OVERLAY_DRAG_END,
    )
    boot.child.pump(1.1)
    q3_after = wait_for_q3_state(
        boot.child,
        lambda state: state["page"] == q3_before["page"]
        and state["view"] == q3_before["view"],
        boot.args.marker_timeout,
        "ordinary input isolation Q3 state",
        interaction_start,
    )
    isolated = wait_q4_state(
        boot,
        lambda state: state["active_id"] == 701
        and state["presentation"] == 1,
        "ordinary overlay remained active after blocked input",
        interaction_start,
    )
    record["input_isolation"] = {
        "before": q3_before,
        "after": q3_after,
        "overlay_state": isolated,
        "passed": True,
    }
    boot.sample_stack("input-isolation")

    record["updated_inject"] = wait_inject(boot, "updated", 1, 1)
    updated = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 1
        and state["active_id"] == 701
        and state["active_generation"] != initial["active_generation"]
        and state["presentation"] == 1
        and state["haptic_events"] >= 2
        and state["wake_requests"] == 1,
        "ordinary same-ID updated long UTF-8 overlay",
    )
    record["updated_state"] = updated
    record["screenshots"]["updated_long_utf8"] = boot.screenshot(
        "ordinary-updated-long-utf8"
    )
    ordinary_effects = effect_pairs(
        boot.child.transcript[boot.launch_offset :],
        [701, 701],
        [701],
        require_identity_pairs=False,
    )
    if len(ordinary_effects["haptics"]) != 2 or len(
        ordinary_effects["wakes"]
    ) != 1:
        raise Q4NativeFailure("ordinary effect marker counts are not 2 haptic / 1 wake")
    if (
        ordinary_effects["haptics"][-1]["generation"]
        != ordinary_effects["wakes"][0]["generation"]
    ):
        raise Q4NativeFailure(
            "ordinary wake generation does not match updated haptic generation"
        )
    record["effects"] = ordinary_effects
    boot.sample_stack("updated-long-utf8")

    action_start = len(boot.child.transcript)
    boot.click("ordinary-dismiss", OVERLAY_DISMISS_POINT)
    dismissed = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 1
        and state["active_id"] == 0,
        "ordinary Dismiss action",
        action_start,
    )
    record["dismissed_state"] = dismissed
    boot.sample_stack("dismissed")
    return record


def open_notification_center(boot: Boot) -> dict[str, int]:
    if boot.console is None or boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    Q3.swipe_to_apps(boot.console, boot.child, boot.evidence_dir)
    start = len(boot.child.transcript)
    boot.click("open-notifications", NOTIFICATION_LAUNCHER_POINT)
    boot.child.pump(1.0)
    return wait_for_q3_state(
        boot.child,
        lambda state: state["page"] == Q3.PAGE_APPS and state["view"] == 3,
        boot.args.marker_timeout,
        "Notification Center view",
        start,
    )


def run_center(boot: Boot) -> dict[str, Any]:
    if boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    record: dict[str, Any] = {"scenario": "center", "screenshots": {}}
    record["inject"] = wait_inject(boot, "ready", 5, 5)
    ready = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 5
        and state["dnd"] == 1
        and state["active_id"] == 0
        and state["haptic_events"] == 0
        and state["wake_requests"] == 0,
        "DND-retained Notification Center fixtures",
    )
    record["ready_state"] = ready
    record["center_view"] = open_notification_center(boot)
    record["screenshots"]["dnd_center"] = boot.screenshot("center-dnd")
    boot.sample_stack("center-open")
    record["effects_before_actions"] = effect_pairs(
        boot.child.transcript[boot.launch_offset :], []
    )

    boot.click("center-mark-read", CENTER_FIRST_READ_POINT)
    boot.child.pump(1.1)
    record["screenshots"]["marked_read"] = boot.screenshot(
        "center-marked-read"
    )
    after_read = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 5
        and state["dnd"] == 1
        and state["active_id"] == 0,
        "Notification Center Mark read action",
    )
    record["after_mark_read"] = after_read

    action_start = len(boot.child.transcript)
    boot.click("center-delete", CENTER_FIRST_DELETE_POINT)
    after_delete = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 4
        and state["dnd"] == 1
        and state["active_id"] == 0,
        "Notification Center Delete action",
        action_start,
    )
    record["after_delete"] = after_delete
    record["effects_after_actions"] = effect_pairs(
        boot.child.transcript[boot.launch_offset :], []
    )
    boot.sample_stack("center-actions")
    return record


def run_calls(boot: Boot) -> dict[str, Any]:
    if boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    record: dict[str, Any] = {"scenario": "calls", "screenshots": {}}
    record["inject"] = wait_inject(boot, "ready", 2, 2)
    alice = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 2
        and state["active_id"] == 721
        and state["presentation"] == 2
        and state["haptic_events"] >= 2
        and state["wake_requests"] >= 2,
        "Alice full-screen call",
    )
    record["alice_state"] = alice
    record["screenshots"]["alice"] = boot.screenshot("calls-alice")
    boot.sample_stack("alice")

    q3_before = wait_for_q3_state(
        boot.child,
        lambda state: True,
        boot.args.marker_timeout,
        "calls pre-isolation Q3 state",
        boot.launch_offset,
    )
    interaction_start = len(boot.child.transcript)
    send_drag(
        boot,
        "calls-fullscreen-drag",
        OVERLAY_DRAG_START,
        OVERLAY_DRAG_END,
    )
    boot.child.pump(1.1)
    q3_after = wait_for_q3_state(
        boot.child,
        lambda state: state["page"] == q3_before["page"]
        and state["view"] == q3_before["view"],
        boot.args.marker_timeout,
        "full-screen call input isolation",
        interaction_start,
    )
    still_alice = wait_q4_state(
        boot,
        lambda state: state["active_id"] == 721
        and state["presentation"] == 2,
        "Alice retained after blocked page swipe",
        interaction_start,
    )
    record["input_isolation"] = {
        "before": q3_before,
        "after": q3_after,
        "call_state": still_alice,
        "passed": True,
    }

    action_start = len(boot.child.transcript)
    boot.click("calls-accept-alice", CALL_ACCEPT_POINT)
    bob = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 1
        and state["active_id"] == 722
        and state["presentation"] == 2,
        "Bob promoted after Alice Accept",
        action_start,
    )
    record["bob_promoted_state"] = bob
    record["screenshots"]["bob_promoted"] = boot.screenshot(
        "calls-bob-promoted"
    )
    boot.sample_stack("bob-promoted")

    action_start = len(boot.child.transcript)
    boot.click("calls-reject-bob", CALL_REJECT_POINT)
    cleared = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 0
        and state["active_id"] == 0,
        "Bob Reject action",
        action_start,
    )
    record["cleared_state"] = cleared
    record["effects"] = effect_pairs(
        boot.child.transcript[boot.launch_offset :], [721, 722]
    )
    boot.sample_stack("calls-cleared")
    return record


def run_workout(boot: Boot) -> dict[str, Any]:
    if boot.console is None or boot.child is None:
        raise Q4NativeFailure("emulator is not running")
    record: dict[str, Any] = {"scenario": "workout", "screenshots": {}}
    record["armed_inject"] = wait_inject(boot, "armed", 0, 1)
    armed = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 0
        and state["active_id"] == 0
        and state["haptic_events"] == 0
        and state["wake_requests"] == 0,
        "workout call armed before active workout",
    )
    record["armed_state"] = armed
    boot.sample_stack("armed")

    Q3.open_workout(
        boot.console, boot.child, boot.evidence_dir
    )
    active_start = len(boot.child.transcript)
    boot.click("workout-start-walk", Q3.START_WALK_POINT)
    active_workout = wait_for_q3_state(
        boot.child,
        lambda state: state["state"] == Q3.STATE_ACTIVE
        and state["view"] == Q3.VIEW_WORKOUT,
        boot.args.marker_timeout,
        "active workout before call injection",
        active_start,
    )
    record["active_workout"] = active_workout
    record["active_inject"] = wait_inject(boot, "active", 1, 1)
    coach = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 1
        and state["active_id"] == 731
        and state["presentation"] == 1
        and state["haptic_events"] == 1
        and state["wake_requests"] == 1,
        "Coach workout call overlay",
    )
    record["coach_state"] = coach
    record["screenshots"]["workout_call"] = boot.screenshot("workout-call")
    boot.sample_stack("workout-call")

    pause_start = len(boot.child.transcript)
    boot.click("workout-pause-through-overlay", Q3.SESSION_PRIMARY_POINT)
    paused = wait_for_q3_state(
        boot.child,
        lambda state: state["state"] == Q3.STATE_PAUSED
        and state["view"] == Q3.VIEW_WORKOUT,
        boot.args.marker_timeout,
        "Pause button beneath workout notification overlay",
        pause_start,
    )
    overlay_after_pause = wait_q4_state(
        boot,
        lambda state: state["active_id"] == 731
        and state["presentation"] == 1,
        "workout overlay retained after Pause",
        pause_start,
    )
    record["pause_through_overlay"] = {
        "workout_state": paused,
        "notification_state": overlay_after_pause,
        "passed": True,
    }
    record["screenshots"]["paused_with_call"] = boot.screenshot(
        "workout-paused-with-call"
    )
    boot.sample_stack("paused-with-call")

    action_start = len(boot.child.transcript)
    boot.click("workout-reject-call", OVERLAY_REJECT_POINT)
    cleared = wait_q4_state(
        boot,
        lambda state: state["notifications"] == 0
        and state["active_id"] == 0,
        "workout call Reject action",
        action_start,
    )
    record["cleared_state"] = cleared
    record["effects"] = effect_pairs(
        boot.child.transcript[boot.launch_offset :], [731]
    )
    boot.sample_stack("workout-call-cleared")
    return record


SCENARIO_RUNNERS: dict[str, Callable[[Boot], dict[str, Any]]] = {
    "ordinary": run_ordinary,
    "center": run_center,
    "calls": run_calls,
    "workout": run_workout,
}


def run(args: argparse.Namespace) -> int:
    validate_settings(args)
    openvela_root = args.openvela_root.resolve()
    source_output = SMOKE.resolve_under(openvela_root, args.output_dir).resolve()
    evidence_dir = args.evidence_dir.resolve()
    NATIVE.require_disjoint_paths(source_output, evidence_dir)
    NATIVE.ensure_empty_evidence_dir(evidence_dir)
    run_id = secrets.token_hex(16)
    result: dict[str, Any] = {
        "status": "running",
        "started_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "run_id": run_id,
        "source_output": str(source_output),
        "scenarios": [],
        "checks": {},
        "stack_samples": [],
    }
    failure: dict[str, str] | None = None
    active_boot: Boot | None = None
    active_runtime: dict[str, Any] | None = None
    active_runtime_output: Path | None = None
    previous_handlers: dict[signal.Signals, Any] = {}

    def check(name: str, condition: bool, message: str) -> None:
        result["checks"][name] = bool(condition)
        if not condition:
            raise Q4NativeFailure(message)

    def handle_signal(signum: int, _frame: Any) -> None:
        raise Q4NativeFailure(f"received {signal.Signals(signum).name}")

    try:
        for handled_signal in (signal.SIGTERM, signal.SIGINT):
            previous_handlers[handled_signal] = signal.getsignal(handled_signal)
            signal.signal(handled_signal, handle_signal)
        check(
            "port_preflight",
            NATIVE.port_available(args.console_port),
            f"console port is already in use: {args.console_port}",
        )
        emulator_script = openvela_root / "emulator.sh"
        SMOKE.validate_runtime_inputs(emulator_script, source_output)
        config = source_output / ".config"
        for key in (
            "CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS",
            "CONFIG_STACK_COLORATION",
            "CONFIG_FS_PROCFS",
        ):
            check(
                f"config_{key.lower()}",
                SMOKE.config_value(config, key) == "y",
                f"required native evidence config is not enabled: {key}",
            )
        configured_stack = SMOKE.config_value(
            config, "CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE"
        )
        check(
            "configured_stack_size",
            bool(re.fullmatch(r"[1-9][0-9]*", configured_stack)),
            "configured smart_band stack size is not a positive integer",
        )
        result["configured_stack_size_bytes"] = int(configured_stack)

        emulator_root = openvela_root / "prebuilts/emulator/linux-x86_64"
        emulator_library_path = emulator_root / "lib64"
        SMOKE.verify_elf(emulator_root / "emulator", evidence_dir, "emulator")
        SMOKE.verify_elf(
            emulator_root / "qemu/linux-x86_64/qemu-system-aarch64-headless",
            evidence_dir,
            "qemu-headless",
            emulator_library_path,
        )

        for scenario in SCENARIOS:
            scenario_dir = evidence_dir / "scenarios" / scenario
            scenario_dir.mkdir(parents=True)
            active_runtime_output = scenario_dir / "runtime-output"
            active_runtime = NATIVE.stage_runtime_output(
                source_output, active_runtime_output
            )
            isolated = all(
                active_runtime["files"][name]["isolated"]
                and active_runtime["files"][name]["initial_hash_matches"]
                and active_runtime["files"][name]["method"] == "copy2"
                for name in NATIVE.RUNTIME_INPUTS
            )
            check(
                f"{scenario}_runtime_isolated",
                isolated,
                f"{scenario} runtime inputs were not isolated copy2 files",
            )
            SMOKE.validate_runtime_inputs(emulator_script, active_runtime_output)
            active_boot = Boot(
                scenario,
                args,
                openvela_root,
                active_runtime_output,
                scenario_dir,
                f"{run_id}-{scenario}",
            )
            active_boot.start()
            scenario_record = SCENARIO_RUNNERS[scenario](active_boot)
            scenario_record["markers"] = active_boot.markers()
            scenario_record["stack_samples"] = list(active_boot.stack_samples)
            result["stack_samples"].extend(active_boot.stack_samples)
            active_boot.stop()
            scenario_record["process_cleanup"] = active_boot.process_cleanup
            active_boot = None
            check(
                f"{scenario}_source_unchanged",
                NATIVE.verify_fixed_output_unchanged(active_runtime),
                f"fixed source output changed during {scenario}",
            )
            scenario_record["runtime"] = active_runtime
            cleanup = NATIVE.cleanup_staged_runtime_inputs(active_runtime_output)
            scenario_record["runtime_cleanup"] = cleanup
            check(
                f"{scenario}_runtime_cleanup",
                cleanup["passed"],
                f"{scenario} staged runtime inputs were not removed cleanly",
            )
            active_runtime = None
            active_runtime_output = None
            result["scenarios"].append(scenario_record)

        result["stack_summary"] = summarize_stack_samples(
            result["stack_samples"]
        )
        check(
            "stack_margin_at_least_25_percent",
            result["stack_summary"]["minimum_margin_percent"]
            >= MIN_STACK_MARGIN_PERCENT,
            "Q4 worst-path stack margin is below 25 percent",
        )
        check(
            "four_scenarios_completed",
            [item["scenario"] for item in result["scenarios"]]
            == list(SCENARIOS),
            "not all isolated Q4 scenarios completed",
        )
    except Exception as exc:
        failure = {
            "type": type(exc).__name__,
            "message": str(exc),
            "traceback": "failure.txt",
        }
        NATIVE.write_text(evidence_dir / "failure.txt", traceback.format_exc())
    finally:
        if active_boot is not None:
            try:
                active_boot.stop()
            except Exception as exc:
                result["checks"]["emergency_process_cleanup"] = False
                if failure is None:
                    failure = {
                        "type": type(exc).__name__,
                        "message": str(exc),
                        "traceback": "failure.txt",
                    }
                    NATIVE.write_text(
                        evidence_dir / "failure.txt", traceback.format_exc()
                    )
        result["checks"]["port_cleanup"] = NATIVE.wait_for_port_available(
            args.console_port
        )
        if active_runtime is not None:
            try:
                result["checks"]["failed_source_unchanged"] = (
                    NATIVE.verify_fixed_output_unchanged(active_runtime)
                )
            except Exception:
                result["checks"]["failed_source_unchanged"] = False
        if active_runtime_output is not None:
            try:
                cleanup = NATIVE.cleanup_staged_runtime_inputs(
                    active_runtime_output
                )
                result["checks"]["failed_runtime_cleanup"] = cleanup["passed"]
                result["failed_runtime_cleanup"] = cleanup
            except Exception:
                result["checks"]["failed_runtime_cleanup"] = False
        for handled_signal, previous in previous_handlers.items():
            signal.signal(handled_signal, previous)
        result["failure"] = failure
        result["status"] = (
            "passed"
            if failure is None and all(result["checks"].values())
            else "failed"
        )
        result["completed_at_utc"] = dt.datetime.now(
            dt.timezone.utc
        ).isoformat()
        NATIVE.write_json(evidence_dir / "q4-native-journey.json", result)
        NATIVE.write_evidence_manifest(evidence_dir)

    print(
        json.dumps(
            {"status": result["status"], "evidence_dir": str(evidence_dir)},
            sort_keys=True,
        )
    )
    return 0 if result["status"] == "passed" else 1


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
