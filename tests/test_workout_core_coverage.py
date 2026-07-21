"""Enforce per-file line coverage for the standalone workout core."""

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
TEST_SOURCE = Path(__file__).with_name("workout_core_test.c")
PRODUCTION_SOURCES = [
    APP_DIR / "logic" / "step_normalizer.c",
    APP_DIR / "logic" / "workout_model.c",
]


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required coverage tool is unavailable: {name}")
    return path


def main() -> None:
    gcc = require_tool(os.environ.get("CC", "gcc"))
    gcov = require_tool("gcov")
    for source in [TEST_SOURCE, *PRODUCTION_SOURCES]:
        if not source.is_file():
            raise RuntimeError(f"coverage source is missing: {source}")

    with tempfile.TemporaryDirectory(prefix="smart-band-workout-coverage-") as temp:
        build_root = Path(temp)
        output = build_root / ("workout_core_test.exe" if os.name == "nt" else "workout_core_test")
        command = [
            gcc, "-std=c11", "-O0", "-g", "--coverage", "-fprofile-abs-path",
            "-Wall", "-Wextra", "-Werror", "-pedantic", f"-I{INCLUDE_DIR}",
            str(TEST_SOURCE), *(str(source) for source in PRODUCTION_SOURCES),
            "-o", str(output),
        ]
        print("coverage compile:", " ".join(command), flush=True)
        subprocess.run(command, cwd=build_root, check=True)
        subprocess.run([str(output)], cwd=build_root, check=True)

        for source in PRODUCTION_SOURCES:
            relative = source.relative_to(ROOT).as_posix()
            report = [
                sys.executable, "-m", "gcovr", "--root", str(ROOT),
                "--object-directory", str(build_root), "--gcov-executable", gcov,
                "--filter", f"^{re.escape(relative)}$", "--exclude", "^tests/",
                "--txt", "--print-summary", "--fail-under-line", "90",
            ]
            print(f"coverage report [{relative}]:", " ".join(report), flush=True)
            subprocess.run(report, cwd=ROOT, check=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"workout core coverage failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
