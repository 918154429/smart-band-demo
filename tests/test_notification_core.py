"""Compile, execute, and measure the notification core with GCC."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "openvela_app" / "smart_band"
INCLUDE_DIR = APP_DIR / "include"
TEST_SOURCE = Path(__file__).with_name("notification_core_test.c")
PRODUCTION_SOURCES = [
    APP_DIR / "logic" / "notification_model.c",
    APP_DIR / "logic" / "notification_demo.c",
]
MINIMUM_LINE_COVERAGE = 90.0


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required tool is unavailable: {name}")
    return path


def compile_and_run(gcc: str, build_dir: Path) -> None:
    objects: list[Path] = []
    common = [
        gcc,
        "-std=c11",
        "-O0",
        "-g",
        "--coverage",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        f"-I{INCLUDE_DIR}",
    ]
    for source in [TEST_SOURCE, *PRODUCTION_SOURCES]:
        output = build_dir / f"{source.stem}.o"
        command = [*common, "-c", str(source), "-o", str(output)]
        print("notification compile:", " ".join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)
        objects.append(output)

    executable = build_dir / (
        "notification_core_test.exe" if os.name == "nt" else
        "notification_core_test"
    )
    link = [gcc, "--coverage", *(str(path) for path in objects), "-o", str(executable)]
    print("notification link:", " ".join(link), flush=True)
    subprocess.run(link, cwd=ROOT, check=True)
    subprocess.run([str(executable)], cwd=ROOT, check=True)


def measure_coverage(gcov: str, build_dir: Path) -> None:
    for source in PRODUCTION_SOURCES:
        command = [gcov, "-b", "-c", "-o", str(build_dir), str(source)]
        print("notification coverage:", " ".join(command), flush=True)
        result = subprocess.run(
            command, cwd=build_dir, check=True, capture_output=True, text=True
        )
        print(result.stdout, end="")
        match = re.search(r"Lines executed:([0-9.]+)%", result.stdout)
        if match is None:
            raise RuntimeError(f"could not parse line coverage for {source.name}")
        coverage = float(match.group(1))
        if coverage < MINIMUM_LINE_COVERAGE:
            raise RuntimeError(
                f"{source.name} line coverage {coverage:.2f}% is below "
                f"{MINIMUM_LINE_COVERAGE:.2f}%"
            )


def main() -> None:
    for source in [TEST_SOURCE, *PRODUCTION_SOURCES]:
        if not source.is_file():
            raise RuntimeError(f"notification source is missing: {source}")
    gcc = require_tool(os.environ.get("CC", "gcc"))
    gcov = require_tool(os.environ.get("GCOV", "gcov"))
    with tempfile.TemporaryDirectory(prefix="smart-band-notification-") as temp:
        build_dir = Path(temp)
        compile_and_run(gcc, build_dir)
        measure_coverage(gcov, build_dir)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"notification core test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
