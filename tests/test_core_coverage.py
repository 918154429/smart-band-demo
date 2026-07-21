"""Measure line coverage of the host-testable production C core with GCC."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "openvela_app" / "smart_band"
INCLUDE_DIR = APP_DIR / "include"
FAKE_LVGL_DIR = Path(__file__).with_name("fake_lvgl")

# This is the architectural production-core boundary, not a list selected from
# coverage results. It includes every host-testable model/provider/controller and
# time-state implementation introduced by the reliability refactor. LVGL page and
# app view adapters remain outside the C core and are covered by runtime/UI gates.
CORE_SOURCES = [
    APP_DIR / "watch_model.c",
    APP_DIR / "sensor_bridge.c",
    APP_DIR / "smart_band_apps.c",
    APP_DIR / "services" / "event_queue.c",
    APP_DIR / "services" / "event_inbox.c",
    APP_DIR / "services" / "clock.c",
    APP_DIR / "services" / "capabilities.c",
    APP_DIR / "services" / "runtime.c",
    APP_DIR / "services" / "storage_codec.c",
    APP_DIR / "services" / "store.c",
    APP_DIR / "platform" / "platform_noop.c",
    APP_DIR / "platform" / "loopback" / "sync_loopback.c",
    APP_DIR / "platform" / "storage" / "storage_fault.c",
    APP_DIR / "platform" / "storage" / "storage_memory.c",
    APP_DIR / "platform" / "storage" / "storage_file.c",
    APP_DIR / "logic" / "calculator_model.c",
    APP_DIR / "logic" / "game_2048_model.c",
    APP_DIR / "logic" / "mines_model.c",
    APP_DIR / "logic" / "step_normalizer.c",
    APP_DIR / "logic" / "workout_model.c",
    APP_DIR / "logic" / "notification_model.c",
    APP_DIR / "logic" / "notification_demo.c",
    APP_DIR / "logic" / "power_policy.c",
    APP_DIR / "logic" / "watch_face_settings.c",
    APP_DIR / "services" / "sync_protocol.c",
    APP_DIR / "apps" / "timer_app.c",
    APP_DIR / "apps" / "stopwatch_app.c",
]

NEW_RUNTIME_SOURCES = [
    APP_DIR / "services" / "event_queue.c",
    APP_DIR / "services" / "event_inbox.c",
    APP_DIR / "services" / "clock.c",
    APP_DIR / "services" / "capabilities.c",
    APP_DIR / "services" / "runtime.c",
    APP_DIR / "platform" / "platform_noop.c",
    APP_DIR / "platform" / "loopback" / "sync_loopback.c",
]

NEW_STORAGE_SOURCES = [
    APP_DIR / "services" / "storage_codec.c",
    APP_DIR / "services" / "store.c",
    APP_DIR / "platform" / "storage" / "storage_fault.c",
    APP_DIR / "platform" / "storage" / "storage_memory.c",
    APP_DIR / "platform" / "storage" / "storage_file.c",
]

NEW_W1_SOURCES = [
    APP_DIR / "logic" / "step_normalizer.c",
    APP_DIR / "logic" / "workout_model.c",
    APP_DIR / "logic" / "notification_model.c",
    APP_DIR / "logic" / "notification_demo.c",
    APP_DIR / "logic" / "power_policy.c",
    APP_DIR / "services" / "sync_protocol.c",
]

NEW_WATCH_FACE_SOURCES = [
    APP_DIR / "logic" / "watch_face_settings.c",
]


@dataclass(frozen=True)
class CoverageTarget:
    name: str
    test_source: Path
    production_sources: tuple[Path, ...]
    include_dirs: tuple[Path, ...] = ()
    needs_temp_directory: bool = False


TARGETS = [
    CoverageTarget(
        "storage_core",
        Path(__file__).with_name("storage_core_test.c"),
        tuple(NEW_STORAGE_SOURCES),
        needs_temp_directory=True,
    ),
    CoverageTarget(
        "runtime_core",
        Path(__file__).with_name("runtime_core_test.c"),
        (
            APP_DIR / "watch_model.c",
            APP_DIR / "sensor_bridge.c",
            APP_DIR / "smart_band_apps.c",
            *NEW_RUNTIME_SOURCES,
            APP_DIR / "services" / "storage_codec.c",
            APP_DIR / "services" / "store.c",
            APP_DIR / "platform" / "storage" / "storage_fault.c",
            APP_DIR / "platform" / "storage" / "storage_memory.c",
        ),
        (FAKE_LVGL_DIR,),
    ),
    CoverageTarget(
        "watch_model",
        Path(__file__).with_name("watch_model_test.c"),
        (APP_DIR / "watch_model.c", APP_DIR / "sensor_bridge.c"),
    ),
    CoverageTarget(
        "app_logic",
        Path(__file__).with_name("app_logic_test.c"),
        (
            APP_DIR / "logic" / "calculator_model.c",
            APP_DIR / "logic" / "game_2048_model.c",
            APP_DIR / "logic" / "mines_model.c",
        ),
    ),
    CoverageTarget(
        "app_runtime",
        Path(__file__).with_name("app_runtime_test.c"),
        (APP_DIR / "smart_band_apps.c",),
        (FAKE_LVGL_DIR,),
    ),
    CoverageTarget(
        "time_apps",
        Path(__file__).with_name("time_apps_test.c"),
        (
            APP_DIR / "smart_band_apps.c",
            APP_DIR / "apps" / "timer_app.c",
            APP_DIR / "apps" / "stopwatch_app.c",
        ),
        (FAKE_LVGL_DIR,),
    ),
    CoverageTarget(
        "workout_core",
        Path(__file__).with_name("workout_core_test.c"),
        (
            APP_DIR / "logic" / "step_normalizer.c",
            APP_DIR / "logic" / "workout_model.c",
        ),
    ),
    CoverageTarget(
        "notification_core",
        Path(__file__).with_name("notification_core_test.c"),
        (
            APP_DIR / "logic" / "notification_model.c",
            APP_DIR / "logic" / "notification_demo.c",
        ),
    ),
    CoverageTarget(
        "power_policy",
        Path(__file__).with_name("power_policy_test.c"),
        (APP_DIR / "logic" / "power_policy.c",),
    ),
    CoverageTarget(
        "watch_face_settings",
        Path(__file__).with_name("watch_face_settings_test.c"),
        (
            APP_DIR / "logic" / "watch_face_settings.c",
            APP_DIR / "services" / "storage_codec.c",
            APP_DIR / "services" / "store.c",
            APP_DIR / "platform" / "storage" / "storage_fault.c",
            APP_DIR / "platform" / "storage" / "storage_memory.c",
        ),
    ),
    CoverageTarget(
        "sync_protocol",
        Path(__file__).with_name("sync_protocol_test.c"),
        (APP_DIR / "services" / "sync_protocol.c",),
    ),
]


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required coverage tool is unavailable: {name}")
    return path


def validate_scope() -> None:
    measured = {source.resolve() for target in TARGETS for source in target.production_sources}
    expected = {source.resolve() for source in CORE_SOURCES}
    if measured != expected:
        missing = sorted(str(path) for path in expected - measured)
        unexpected = sorted(str(path) for path in measured - expected)
        raise RuntimeError(
            f"coverage target/source drift; missing={missing}, unexpected={unexpected}"
        )
    for source in [*CORE_SOURCES, *(target.test_source for target in TARGETS)]:
        if not source.is_file():
            raise RuntimeError(f"coverage source is missing: {source}")


def compile_and_run(gcc: str, build_root: Path, target: CoverageTarget) -> None:
    target_dir = build_root / target.name
    target_dir.mkdir()
    output = target_dir / (f"{target.name}.exe" if os.name == "nt" else target.name)
    sources = [target.test_source, *target.production_sources]
    include_dirs = [INCLUDE_DIR, *target.include_dirs]
    command = [
        gcc,
        "-std=c11",
        "-O0",
        "-g",
        "--coverage",
        "-fprofile-abs-path",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        *(f"-I{directory}" for directory in include_dirs),
        *(str(source) for source in sources),
        "-o",
        str(output),
    ]
    print(f"coverage compile [{target.name}]:", " ".join(command), flush=True)
    subprocess.run(command, cwd=target_dir, check=True)
    run_command = [str(output)]
    if target.needs_temp_directory:
        runtime_directory = target_dir / "runtime-data"
        runtime_directory.mkdir()
        run_command.append(str(runtime_directory))
    subprocess.run(run_command, cwd=target_dir, check=True)


def run_gcovr(build_root: Path) -> None:
    filters = []
    for source in CORE_SOURCES:
        relative = source.relative_to(ROOT).as_posix()
        filters.extend(["--filter", f"^{re.escape(relative)}$"])

    command = [
        sys.executable,
        "-m",
        "gcovr",
        "--root",
        str(ROOT),
        "--object-directory",
        str(build_root),
        "--gcov-executable",
        require_tool("gcov"),
        *filters,
        "--exclude",
        "^tests/",
        "--txt",
        "--print-summary",
        "--fail-under-line",
        "85",
    ]
    print("coverage report:", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)

    runtime_base_command = [
        sys.executable,
        "-m",
        "gcovr",
        "--root",
        str(ROOT),
        "--object-directory",
        str(build_root),
        "--gcov-executable",
        require_tool("gcov"),
    ]
    for source in [
        *NEW_RUNTIME_SOURCES,
        *NEW_STORAGE_SOURCES,
        *NEW_W1_SOURCES,
        *NEW_WATCH_FACE_SOURCES,
    ]:
        relative = source.relative_to(ROOT).as_posix()
        runtime_command = [
            *runtime_base_command,
            "--filter",
            f"^{re.escape(relative)}$",
            "--exclude",
            "^tests/",
            "--txt",
            "--print-summary",
            "--fail-under-line",
            "90",
        ]
        print(
            f"new runtime coverage report [{relative}]:",
            " ".join(runtime_command),
            flush=True,
        )
        subprocess.run(runtime_command, cwd=ROOT, check=True)


def main() -> None:
    validate_scope()
    gcc = require_tool(os.environ.get("CC", "gcc"))
    with tempfile.TemporaryDirectory(prefix="smart-band-core-coverage-") as temp:
        build_root = Path(temp)
        for target in TARGETS:
            compile_and_run(gcc, build_root, target)
        run_gcovr(build_root)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"production C core coverage failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
