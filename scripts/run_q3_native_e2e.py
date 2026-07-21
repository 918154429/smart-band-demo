#!/usr/bin/env python3
"""Run the storage-backed native Workout recovery and History journey."""

from __future__ import annotations

import argparse
import datetime as dt
import importlib.util
import json
import os
import re
import secrets
import signal
import sys
import time
import traceback
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_DIR = "cmake_out/vela_goldfish-arm64-v8a-ap"
DEFAULT_STORAGE_PATH = "/data/smart-band-q3"
LAYOUT_SIZE = (330, 626)
WORKOUT_LAUNCHER_POINT = (90, 177)
HISTORY_LAUNCHER_POINT = (246, 177)
START_WALK_POINT = (90, 388)
SESSION_PRIMARY_POINT = (62, 596)
SESSION_FINISH_POINT = (168, 596)
RECOVERY_RESUME_POINT = (90, 579)
CONFIRM_ACCEPT_POINT = (246, 573)
SUMMARY_DONE_POINT = (168, 594)
MAX_APPS_SWIPE_ATTEMPTS = 5
SWIPE_STEP_DELAY_SECONDS = 0.03
POST_SWIPE_SECONDS = 1.2
CLICK_HOLD_SECONDS = 0.1
Q3_MARKER = "smart_band:q3:v1"
STATE_IDLE = 0
STATE_ACTIVE = 2
STATE_PAUSED = 3
STATE_FINISHED = 4
STATE_RECOVERY = 6
PAGE_APPS = 3
VIEW_NONE = 0
VIEW_WORKOUT = 1
VIEW_HISTORY = 2


class Q3NativeFailure(RuntimeError):
    """Raised when a native Q3 assertion is not proved."""


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise Q3NativeFailure(f"cannot load helper module: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


NATIVE = load_module("q3_native_base", ROOT / "scripts" / "run_native_e2e.py")
SMOKE = NATIVE.SMOKE


def framed_screen_geometry(
    width: int, height: int
) -> tuple[tuple[int, int], tuple[int, int]]:
    if width <= 0 or height <= 0:
        raise Q3NativeFailure("display dimensions must be positive")
    if width <= 540 and height <= 540:
        return (0, 0), (width, height)

    watch_height = max(min(height - 48, 720), 360)
    watch_width = (watch_height * 194) // 368
    if watch_width > width - 48:
        watch_width = width - 48
        watch_height = (watch_width * 368) // 194
    return (
        ((width - watch_width) // 2 + 3, (height - watch_height) // 2 + 3),
        (watch_width - 6, watch_height - 6),
    )


SCREEN_ORIGIN, SCREEN_SIZE = framed_screen_geometry(
    NATIVE.EXPECTED_WIDTH, NATIVE.EXPECTED_HEIGHT
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run three-boot native Workout recovery and History E2E"
    )
    parser.add_argument("--openvela-root", required=True, type=Path)
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--console-port", type=int, default=5565)
    parser.add_argument("--storage-path", default=DEFAULT_STORAGE_PATH)
    parser.add_argument("--boot-timeout", type=float, default=120.0)
    parser.add_argument("--command-timeout", type=float, default=20.0)
    parser.add_argument("--ui-settle-seconds", type=float, default=3.0)
    parser.add_argument("--warmup-seconds", type=int, default=0)
    parser.add_argument("--soak-seconds", type=int, default=0)
    parser.add_argument("--sample-interval-seconds", type=int, default=10)
    return parser.parse_args()


def validate_settings(args: argparse.Namespace) -> None:
    if not 1 <= args.console_port <= 65535:
        raise Q3NativeFailure("console port must be between 1 and 65535")
    if not re.fullmatch(r"/[A-Za-z0-9._/-]+", args.storage_path):
        raise Q3NativeFailure("storage path must be a simple absolute guest path")
    if "/../" in f"{args.storage_path}/" or "/./" in f"{args.storage_path}/":
        raise Q3NativeFailure("storage path cannot contain dot segments")
    for name in ("boot_timeout", "command_timeout", "ui_settle_seconds"):
        if getattr(args, name) <= 0:
            raise Q3NativeFailure(f"{name.replace('_', '-')} must be positive")
    if args.warmup_seconds < 0 or args.soak_seconds < 0:
        raise Q3NativeFailure("warmup and soak durations must not be negative")
    if args.sample_interval_seconds <= 0:
        raise Q3NativeFailure("sample interval must be positive")
    if args.soak_seconds and args.soak_seconds < args.sample_interval_seconds:
        raise Q3NativeFailure("soak duration must include at least one sample")


def local_point(x: int, y: int) -> tuple[int, int]:
    if (
        not 0 <= x < LAYOUT_SIZE[0]
        or not 0 <= y < LAYOUT_SIZE[1]
    ):
        raise Q3NativeFailure(f"local touch point is outside screen: {(x, y)}")
    return (
        SCREEN_ORIGIN[0] + (x * SCREEN_SIZE[0]) // LAYOUT_SIZE[0],
        SCREEN_ORIGIN[1] + (y * SCREEN_SIZE[1]) // LAYOUT_SIZE[1],
    )


def parse_q3_marker(line: str) -> dict[str, int] | None:
    marker = f"{Q3_MARKER} "
    start = line.find(marker)
    if start < 0:
        return None
    fields: dict[str, int] = {}
    for token in line[start + len(marker) :].strip().split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if not re.fullmatch(r"-?[0-9]+", value):
            return None
        fields[key] = int(value)
    required = {
        "elapsed_ms", "page", "view", "state", "mode", "active_ms",
        "steps", "recovery", "phase", "checkpoint", "daily",
        "sessions", "daily_store", "session_store", "queue", "dropped",
        "evicted", "coalesced", "inbox_dropped", "objects",
        "tick_gap_max_ms",
    }
    return fields if required <= fields.keys() else None


def marker_states(transcript: bytes | bytearray) -> list[dict[str, int]]:
    text = bytes(transcript).decode("utf-8", errors="replace").replace("\r", "")
    states = []
    for line in text.splitlines():
        parsed = parse_q3_marker(line)
        if parsed is not None:
            states.append(parsed)
    return states


def wait_for_state(
    child: Any,
    predicate: Callable[[dict[str, int]], bool],
    timeout: float,
    description: str,
) -> dict[str, int]:
    deadline = time.monotonic() + timeout
    seen = 0
    while time.monotonic() < deadline:
        states = marker_states(child.transcript)
        for state in states[seen:]:
            if predicate(state):
                return state
        seen = len(states)
        child.pump(min(0.5, max(0.0, deadline - time.monotonic())))
    states = marker_states(child.transcript)
    raise Q3NativeFailure(
        f"timed out waiting for {description}; latest={states[-1] if states else None}"
    )


def console_ok(response: str) -> bool:
    return NATIVE.console_response_ok(response)


def click(
    console: Any,
    child: Any,
    evidence_dir: Path,
    name: str,
    point: tuple[int, int],
) -> None:
    x, y = point
    for suffix, pressed in (("down", 1), ("up", 0)):
        response = console.command(
            f"event mouse {x} {y} 0 {pressed}", f"console-{name}-{suffix}.txt"
        )
        if not console_ok(response):
            raise Q3NativeFailure(f"console rejected {name} {suffix}: {response!r}")
        if pressed:
            child.pump(CLICK_HOLD_SECONDS)


def swipe_to_apps(console: Any, child: Any, evidence_dir: Path) -> None:
    for swipe in range(MAX_APPS_SWIPE_ATTEMPTS):
        for index, command in enumerate(NATIVE.build_swipe_commands(), 1):
            response = console.command(
                command, f"console-apps-swipe-{swipe + 1:02d}-{index:02d}.txt"
            )
            if not console_ok(response):
                raise Q3NativeFailure(f"apps swipe command failed: {response!r}")
            time.sleep(SWIPE_STEP_DELAY_SECONDS)
        child.pump(POST_SWIPE_SECONDS)
        states = marker_states(child.transcript)
        if (
            states
            and states[-1]["page"] == PAGE_APPS
            and states[-1]["view"] == VIEW_NONE
        ):
            return
    wait_for_state(
        child,
        lambda state: state["page"] == PAGE_APPS and state["view"] == VIEW_NONE,
        POST_SWIPE_SECONDS,
        "Apps page",
    )


def open_workout(console: Any, child: Any, evidence_dir: Path) -> None:
    swipe_to_apps(console, child, evidence_dir)
    click(
        console, child, evidence_dir, "open-workout",
        local_point(*WORKOUT_LAUNCHER_POINT),
    )
    child.pump(1.0)
    wait_for_state(
        child,
        lambda state: state["page"] == PAGE_APPS
        and state["view"] == VIEW_WORKOUT,
        4.0,
        "Workout view",
    )


def open_history(console: Any, child: Any, evidence_dir: Path) -> None:
    swipe_to_apps(console, child, evidence_dir)
    click(
        console, child, evidence_dir, "open-history",
        local_point(*HISTORY_LAUNCHER_POINT),
    )
    child.pump(1.0)
    wait_for_state(
        child,
        lambda state: state["page"] == PAGE_APPS
        and state["view"] == VIEW_HISTORY,
        4.0,
        "History view",
    )


def inject_motion(console: Any, evidence_dir: Path, index: int) -> None:
    acceleration = "0:9.81:0" if index % 2 == 0 else "4.4:6.8:1.7"
    heart = 96 + index % 12
    commands = {
        f"heart-{index:04d}": f"sensor set heart-rate {heart}",
        f"accel-{index:04d}": f"sensor set acceleration {acceleration}",
    }
    for name, command in commands.items():
        response = console.command(command, f"console-{name}.txt")
        if not console_ok(response):
            raise Q3NativeFailure(f"sensor injection failed: {command}: {response!r}")


def require_stable_state(state: dict[str, int]) -> None:
    for key in ("queue", "dropped", "evicted", "inbox_dropped"):
        if state[key] != 0:
            raise Q3NativeFailure(f"native queue invariant failed: {key}={state[key]}")
    if state["objects"] <= 0:
        raise Q3NativeFailure("native LVGL object count is empty")
    if state["tick_gap_max_ms"] > 2500:
        raise Q3NativeFailure(
            f"native timer gap exceeded 2500ms: {state['tick_gap_max_ms']}"
        )


def run_soak(
    child: Any,
    console: Any,
    evidence_dir: Path,
    warmup_seconds: int,
    soak_seconds: int,
    interval: int,
) -> dict[str, Any]:
    total = warmup_seconds + soak_seconds
    if total == 0:
        return {"enabled": False, "warmup_seconds": 0, "soak_seconds": 0}

    started = time.monotonic()
    formal_samples: list[dict[str, int]] = []
    all_objects: list[int] = []
    sample_index = 0
    next_pause = 300
    while time.monotonic() - started < total:
        inject_motion(console, evidence_dir, sample_index)
        child.pump(float(interval))
        state = marker_states(child.transcript)[-1]
        require_stable_state(state)
        if state["state"] not in (STATE_ACTIVE, STATE_PAUSED):
            raise Q3NativeFailure(f"workout left live state during soak: {state}")
        elapsed = int(time.monotonic() - started)
        all_objects.append(state["objects"])
        if elapsed >= warmup_seconds:
            formal_samples.append(state)
        if elapsed >= next_pause and elapsed + 5 < total:
            click(console, child, evidence_dir, f"soak-pause-{next_pause}",
                  local_point(*SESSION_PRIMARY_POINT))
            wait_for_state(child, lambda value: value["state"] == STATE_PAUSED,
                           5.0, "soak pause")
            click(console, child, evidence_dir, f"soak-resume-{next_pause}",
                  local_point(*SESSION_PRIMARY_POINT))
            wait_for_state(child, lambda value: value["state"] == STATE_ACTIVE,
                           5.0, "soak resume")
            next_pause += 300
        sample_index += 1

    expected = soak_seconds // interval
    if len(formal_samples) < expected:
        raise Q3NativeFailure(
            f"soak produced {len(formal_samples)} formal samples, expected {expected}"
        )
    if all_objects and min(all_objects) != max(all_objects):
        raise Q3NativeFailure(
            f"LVGL object count changed during soak: {min(all_objects)}..{max(all_objects)}"
        )
    samples_path = evidence_dir / "soak-samples.jsonl"
    samples_path.write_text(
        "".join(json.dumps(item, sort_keys=True) + "\n" for item in formal_samples),
        encoding="utf-8",
    )
    return {
        "enabled": True,
        "warmup_seconds": warmup_seconds,
        "soak_seconds": soak_seconds,
        "sample_interval_seconds": interval,
        "formal_samples": len(formal_samples),
        "object_count": all_objects[0] if all_objects else None,
        "max_tick_gap_ms": max(item["tick_gap_max_ms"] for item in formal_samples),
        "heap_verified": False,
        "fd_verified": False,
        "samples": samples_path.name,
    }


class Boot:
    def __init__(
        self,
        index: int,
        args: argparse.Namespace,
        openvela_root: Path,
        runtime_output: Path,
        evidence_dir: Path,
        run_id: str,
    ) -> None:
        self.index = index
        self.args = args
        self.openvela_root = openvela_root
        self.runtime_output = runtime_output
        self.evidence_dir = evidence_dir
        self.run_id = run_id
        self.child = None
        self.console = None
        self.prompt = SMOKE.config_value(
            runtime_output / ".config", "CONFIG_NSH_PROMPT_STRING"
        ).encode("utf-8")
        self.cleanup: list[str] = []

    def start(self, create_storage: bool) -> None:
        emulator_root = self.openvela_root / "prebuilts/emulator/linux-x86_64"
        skin_dir = self.openvela_root / "prebuilts/emulator/skins"
        command = [
            str(self.openvela_root / "emulator.sh"), str(self.runtime_output),
            "-no-window", "-no-audio", "-accel", "off", "-port",
            str(self.args.console_port), "-skin", "xiaomi_smart_screen_10",
            "-skindir", str(skin_dir),
        ]
        env_key = NATIVE.RUN_ID_ENV
        previous = os.environ.get(env_key)
        os.environ[env_key] = self.run_id
        self.child = SMOKE.PtyChild(
            command, self.openvela_root, self.evidence_dir / f"boot-{self.index}.log"
        )
        try:
            self.child.start()
        finally:
            if previous is None:
                os.environ.pop(env_key, None)
            else:
                os.environ[env_key] = previous
        self.child.wait_for(self.prompt, 0, self.args.boot_timeout)
        ready = self.child.send_command(
            f"echo Q3_BOOT_{self.index}_READY", self.prompt,
            self.args.command_timeout, self.evidence_dir / f"boot-{self.index}-ready.txt",
        )
        if f"Q3_BOOT_{self.index}_READY" not in ready:
            raise Q3NativeFailure(f"boot {self.index} NSH readiness failed")
        if create_storage:
            self.child.send_command(
                f"mkdir {self.args.storage_path}", self.prompt,
                self.args.command_timeout,
                self.evidence_dir / f"boot-{self.index}-mkdir-storage.txt",
            )
        storage = self.child.send_command(
            f"ls -ld {self.args.storage_path}", self.prompt,
            self.args.command_timeout,
            self.evidence_dir / f"boot-{self.index}-storage-preflight.txt",
        )
        if self.args.storage_path not in storage:
            raise Q3NativeFailure(f"boot {self.index} storage directory is unavailable")
        self.console = SMOKE.connect_console(self.args.console_port, self.evidence_dir)
        ping = self.console.command("ping", f"boot-{self.index}-console-ping.txt")
        if "I am alive!" not in ping or not console_ok(ping):
            raise Q3NativeFailure(f"boot {self.index} emulator console ping failed")
        transcript_start = len(self.child.transcript)
        self.child.send_command(
            "smart_band &", self.prompt, self.args.command_timeout,
            self.evidence_dir / f"boot-{self.index}-launch.txt",
        )
        self.child.wait_for(
            b"smart_band: UI ready", transcript_start, self.args.command_timeout
        )
        self.child.pump(self.args.ui_settle_seconds)
        wait_for_state(
            self.child,
            lambda state: state["view"] == VIEW_NONE,
            4.0,
            f"boot {self.index} diagnostics",
        )

    def storage_listing(self, suffix: str) -> str:
        if self.child is None:
            raise Q3NativeFailure("boot is not running")
        return self.child.send_command(
            f"ls -l {self.args.storage_path}", self.prompt,
            self.args.command_timeout,
            self.evidence_dir / f"boot-{self.index}-storage-{suffix}.txt",
        )

    def app_pid(self) -> list[int]:
        if self.child is None:
            return []
        output = self.child.send_command(
            "pidof smart_band", self.prompt, self.args.command_timeout,
            self.evidence_dir / f"boot-{self.index}-pidof.txt",
        )
        return SMOKE.extract_pidof(output)

    def stop(self) -> None:
        if self.console is not None:
            try:
                self.console.command(
                    "kill", f"boot-{self.index}-console-kill.txt", allow_eof=True
                )
            except Exception as exc:
                self.cleanup.append(f"console kill failed: {exc}")
            self.console.close()
            self.console = None
        if self.child is not None:
            try:
                self.child.stop_process_group(self.cleanup)
            finally:
                self.child.close()
                self.child = None
        if not NATIVE.wait_for_port_available(self.args.console_port):
            raise Q3NativeFailure(f"console port remained occupied after boot {self.index}")
        NATIVE.write_text(
            self.evidence_dir / f"boot-{self.index}-cleanup.txt",
            "\n".join(self.cleanup) + "\n",
        )


def screenshot(boot: Boot, name: str) -> dict[str, Any]:
    if boot.console is None:
        raise Q3NativeFailure("emulator console is not connected")
    _image, record = NATIVE.capture_screenshot(boot.console, boot.evidence_dir, name)
    if not record["console_ok"] or not record["nonblank"]:
        raise Q3NativeFailure(f"native screenshot failed validation: {name}")
    return record


def run(args: argparse.Namespace) -> int:
    validate_settings(args)
    openvela_root = args.openvela_root.resolve()
    source_output = SMOKE.resolve_under(openvela_root, args.output_dir).resolve()
    evidence_dir = args.evidence_dir.resolve()
    NATIVE.require_disjoint_paths(source_output, evidence_dir)
    NATIVE.ensure_empty_evidence_dir(evidence_dir)
    runtime_output = evidence_dir / "runtime-output"
    run_id = secrets.token_hex(16)
    result: dict[str, Any] = {
        "status": "running",
        "started_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "run_id": run_id,
        "source_output": str(source_output),
        "runtime_output": str(runtime_output),
        "storage_path": args.storage_path,
        "boots": [],
        "screenshots": {},
        "checks": {},
    }
    failure: dict[str, str] | None = None
    runtime: dict[str, Any] | None = None
    active_boot: Boot | None = None

    def check(name: str, condition: bool, message: str) -> None:
        result["checks"][name] = bool(condition)
        if not condition:
            raise Q3NativeFailure(message)

    try:
        check("port_preflight", NATIVE.port_available(args.console_port),
              f"console port is already in use: {args.console_port}")
        runtime = NATIVE.stage_runtime_output(source_output, runtime_output)
        result["runtime"] = runtime
        check(
            "runtime_staged_isolated",
            all(
                runtime["files"][name]["isolated"]
                and runtime["files"][name]["initial_hash_matches"]
                and runtime["files"][name]["method"] == "copy2"
                for name in NATIVE.RUNTIME_INPUTS
            ),
            "runtime inputs were not staged with isolated copy2 files",
        )
        SMOKE.validate_runtime_inputs(openvela_root / "emulator.sh", runtime_output)
        configured_path = SMOKE.config_value(
            runtime_output / ".config", "CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH"
        )
        check("storage_config", configured_path == args.storage_path,
              f"configured storage path is {configured_path!r}")
        diagnostic = SMOKE.config_value(
            runtime_output / ".config",
            "CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS",
        )
        check("diagnostics_config", diagnostic == "y",
              "Q3 E2E diagnostics are not enabled")
        result["data_image_before"] = NATIVE.file_fingerprint(
            runtime_output / "vela_data.bin"
        )

        active_boot = Boot(1, args, openvela_root, runtime_output, evidence_dir, run_id)
        active_boot.start(create_storage=True)
        check("boot1_pid", bool(active_boot.app_pid()), "boot1 app is not alive")
        open_workout(active_boot.console, active_boot.child, evidence_dir)
        click(active_boot.console, active_boot.child, evidence_dir, "start-walk",
              local_point(*START_WALK_POINT))
        wait_for_state(active_boot.child, lambda state: state["state"] == STATE_ACTIVE,
                       8.0, "boot1 active workout")
        for index in range(8):
            inject_motion(active_boot.console, evidence_dir, index)
            active_boot.child.pump(1.0)
        click(active_boot.console, active_boot.child, evidence_dir, "pause-workout",
              local_point(*SESSION_PRIMARY_POINT))
        paused = wait_for_state(
            active_boot.child,
            lambda state: state["state"] == STATE_PAUSED and state["checkpoint"] == 0,
            8.0,
            "boot1 durable paused checkpoint",
        )
        require_stable_state(paused)
        check("boot1_steps", paused["steps"] > 0, "boot1 produced no normalized steps")
        result["screenshots"]["paused"] = screenshot(active_boot, "q3-paused")
        listing = active_boot.storage_listing("paused")
        check("boot1_storage_objects", "object-" in listing,
              "boot1 storage directory has no persisted objects")
        result["boots"].append({"boot": 1, "state": paused})
        active_boot.stop()
        active_boot = None

        active_boot = Boot(2, args, openvela_root, runtime_output, evidence_dir, run_id)
        active_boot.start(create_storage=False)
        check("boot2_pid", bool(active_boot.app_pid()), "boot2 app is not alive")
        open_workout(active_boot.console, active_boot.child, evidence_dir)
        recovered = wait_for_state(
            active_boot.child,
            lambda state: state["state"] == STATE_RECOVERY and state["recovery"] == 1,
            8.0,
            "boot2 recovery confirmation",
        )
        check("recovery_steps_match", recovered["steps"] == paused["steps"],
              "recovered step count differs from paused checkpoint")
        result["screenshots"]["recovery"] = screenshot(active_boot, "q3-recovery")
        click(active_boot.console, active_boot.child, evidence_dir, "resume-recovery",
              local_point(*RECOVERY_RESUME_POINT))
        wait_for_state(active_boot.child, lambda state: state["state"] == STATE_ACTIVE,
                       8.0, "boot2 resumed workout")
        result["soak"] = run_soak(
            active_boot.child, active_boot.console, evidence_dir,
            args.warmup_seconds, args.soak_seconds, args.sample_interval_seconds,
        )
        for index in range(8, 12):
            inject_motion(active_boot.console, evidence_dir, index)
            active_boot.child.pump(1.0)
        click(active_boot.console, active_boot.child, evidence_dir, "finish-request",
              local_point(*SESSION_FINISH_POINT))
        active_boot.child.pump(1.0)
        result["screenshots"]["confirmation"] = screenshot(
            active_boot, "q3-finish-confirmation"
        )
        click(active_boot.console, active_boot.child, evidence_dir, "finish-confirm",
              local_point(*CONFIRM_ACCEPT_POINT))
        finished = wait_for_state(
            active_boot.child,
            lambda state: state["state"] == STATE_FINISHED
            and state["phase"] == 0 and state["sessions"] == 1,
            10.0,
            "boot2 finalized summary",
        )
        require_stable_state(finished)
        check("finished_steps_not_less", finished["steps"] >= recovered["steps"],
              "finished session lost recovered steps")
        result["screenshots"]["summary"] = screenshot(active_boot, "q3-summary")
        click(active_boot.console, active_boot.child, evidence_dir, "summary-done",
              local_point(*SUMMARY_DONE_POINT))
        active_boot.child.pump(1.0)
        click(active_boot.console, active_boot.child, evidence_dir,
              "open-history-after-finish",
              local_point(*HISTORY_LAUNCHER_POINT))
        history = wait_for_state(
            active_boot.child,
            lambda state: state["view"] == VIEW_HISTORY and state["sessions"] == 1,
            8.0,
            "boot2 history view",
        )
        result["screenshots"]["history"] = screenshot(active_boot, "q3-history")
        active_boot.storage_listing("finished")
        result["boots"].append({"boot": 2, "recovery": recovered,
                                "finished": finished, "history": history})
        active_boot.stop()
        active_boot = None

        active_boot = Boot(3, args, openvela_root, runtime_output, evidence_dir, run_id)
        active_boot.start(create_storage=False)
        check("boot3_pid", bool(active_boot.app_pid()), "boot3 app is not alive")
        open_history(active_boot.console, active_boot.child, evidence_dir)
        reloaded = wait_for_state(
            active_boot.child,
            lambda state: state["view"] == VIEW_HISTORY
            and state["state"] == STATE_IDLE and state["sessions"] == 1,
            8.0,
            "boot3 persisted history",
        )
        check("boot3_daily", reloaded["daily"] >= 1,
              "boot3 did not reload a daily summary")
        result["screenshots"]["history_reloaded"] = screenshot(
            active_boot, "q3-history-reloaded"
        )
        result["boots"].append({"boot": 3, "state": reloaded})
        active_boot.storage_listing("reloaded")
        active_boot.stop()
        active_boot = None

        result["data_image_after"] = NATIVE.file_fingerprint(
            runtime_output / "vela_data.bin"
        )
        check(
            "data_image_changed",
            result["data_image_after"]["sha256"] !=
            result["data_image_before"]["sha256"],
            "staged vela_data.bin did not change across persistent boots",
        )
        check("fixed_output_unchanged", NATIVE.verify_fixed_output_unchanged(runtime),
              "fixed source output changed during Q3 journey")
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
                result["checks"]["emergency_cleanup"] = False
                if failure is None:
                    failure = {"type": type(exc).__name__, "message": str(exc)}
        result["checks"]["port_cleanup"] = NATIVE.wait_for_port_available(
            args.console_port
        )
        if runtime is not None:
            try:
                cleanup = NATIVE.cleanup_staged_runtime_inputs(runtime_output)
                result["runtime_cleanup"] = cleanup
                result["checks"]["runtime_input_cleanup"] = cleanup["passed"]
            except Exception as exc:
                result["checks"]["runtime_input_cleanup"] = False
                if failure is None:
                    failure = {"type": type(exc).__name__, "message": str(exc)}
        result["failure"] = failure
        result["status"] = "passed" if failure is None and all(
            result["checks"].values()
        ) else "failed"
        result["completed_at_utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
        NATIVE.write_json(evidence_dir / "q3-native-journey.json", result)
        NATIVE.write_evidence_manifest(evidence_dir)

    print(json.dumps({"status": result["status"],
                      "evidence_dir": str(evidence_dir)}, sort_keys=True))
    return 0 if result["status"] == "passed" else 1


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
