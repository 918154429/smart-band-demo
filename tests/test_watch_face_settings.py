"""Compile, run, and cover the watch-face settings persistence slice."""

from __future__ import annotations

import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from test_ui_compile import find_compiler


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "openvela_app" / "smart_band"
INCLUDE_DIR = APP_DIR / "include"
TEST_SOURCE = Path(__file__).with_name("watch_face_settings_test.c")
PRODUCTION_SOURCES = [
    APP_DIR / "logic" / "watch_face_settings.c",
    APP_DIR / "services" / "storage_codec.c",
    APP_DIR / "services" / "store.c",
    APP_DIR / "platform" / "storage" / "storage_memory.c",
    APP_DIR / "platform" / "storage" / "storage_fault.c",
]
MINIMUM_LINE_COVERAGE = 90.0


def run_compile(command: list[str], environment: Path | None,
                build_root: Path) -> None:
    print("compiling watch-face settings tests:", " ".join(command), flush=True)
    if environment is None:
        subprocess.run(command, cwd=ROOT, check=True)
        return
    batch = build_root / "compile_watch_face_settings.bat"
    batch.write_text(
        "@echo off\r\n"
        f'call "{environment}" >nul || exit /b 1\r\n'
        f"{subprocess.list2cmdline(command)}\r\n",
        encoding="utf-8",
    )
    subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT, check=True)


def strict_build_and_run(build_root: Path) -> tuple[Path, str]:
    compiler, family, environment = find_compiler()
    output = build_root / (
        "watch_face_settings_test.exe" if os.name == "nt"
        else "watch_face_settings_test"
    )
    sources = [str(TEST_SOURCE), *(str(source) for source in PRODUCTION_SOURCES)]
    if family == "msvc":
        command = [
            compiler, "/nologo", "/std:c11", "/W4", "/WX",
            "/Od", "/Zi",
            f"/I{INCLUDE_DIR}", *sources, f"/Fo{build_root}{os.sep}",
            f"/Fd:{build_root / 'watch_face_settings.pdb'}", f"/Fe:{output}",
            "/link", "/DEBUG",
        ]
    else:
        command = [
            compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-pedantic", f"-I{INCLUDE_DIR}", *sources, "-o", str(output),
        ]
    run_compile(command, environment, build_root)
    subprocess.run([str(output)], cwd=ROOT, check=True)
    return output, family


def find_open_cpp_coverage() -> Path | None:
    configured = os.environ.get("OPENCPPCOVERAGE")
    candidates = [
        Path(configured) if configured else None,
        Path(shutil.which("OpenCppCoverage") or "")
        if shutil.which("OpenCppCoverage") else None,
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "OpenCppCoverage" / "OpenCppCoverage.exe",
        Path(tempfile.gettempdir()) / "smart-band-tools" / "OpenCppCoverage"
        / "OpenCppCoverage.exe",
    ]
    return next((path for path in candidates
                 if path is not None and path.is_file()), None)


def msvc_coverage(output: Path, build_root: Path) -> float:
    coverage_tool = find_open_cpp_coverage()
    if coverage_tool is None:
        raise RuntimeError(
            "MSVC settings coverage requires OpenCppCoverage or GCC/gcov"
        )
    report = build_root / "coverage.xml"
    subprocess.run(
        [
            str(coverage_tool), "--quiet", "--sources",
            str(PRODUCTION_SOURCES[0].parent), "--export_type",
            f"cobertura:{report}", "--", str(output),
        ],
        cwd=ROOT,
        check=True,
    )
    return float(ET.parse(report).getroot().attrib["line-rate"]) * 100.0


def gcc_coverage(build_root: Path) -> float:
    gcc = shutil.which("gcc")
    gcov = shutil.which("gcov")
    if gcc is None or gcov is None:
        raise RuntimeError("watch-face settings coverage requires GCC and gcov")
    coverage_dir = build_root / "coverage"
    coverage_dir.mkdir()
    output = coverage_dir / (
        "watch_face_settings_coverage.exe" if os.name == "nt"
        else "watch_face_settings_coverage"
    )
    command = [
        gcc, "-std=c11", "-O0", "-g", "--coverage", "-fprofile-abs-path",
        "-Wall", "-Wextra", "-Werror", "-pedantic", f"-I{INCLUDE_DIR}",
        str(TEST_SOURCE), *(str(source) for source in PRODUCTION_SOURCES),
        "-o", str(output),
    ]
    subprocess.run(command, cwd=coverage_dir, check=True)
    subprocess.run([str(output)], cwd=coverage_dir, check=True)
    summary = coverage_dir / "summary.json"
    relative = PRODUCTION_SOURCES[0].relative_to(ROOT).as_posix()
    subprocess.run(
        [
            sys.executable, "-m", "gcovr", "--root", str(ROOT),
            "--object-directory", str(coverage_dir), "--gcov-executable", gcov,
            "--filter", f"^{re.escape(relative)}$", "--exclude", "^tests/",
            "--json-summary", str(summary), "--fail-under-line",
            str(MINIMUM_LINE_COVERAGE),
        ],
        cwd=ROOT,
        check=True,
    )
    return float(json.loads(summary.read_text(encoding="utf-8"))["line_percent"])


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="smart-band-watch-face-settings-") as temp:
        build_root = Path(temp)
        output, family = strict_build_and_run(build_root)
        if (shutil.which("gcc") and shutil.which("gcov") and
                importlib.util.find_spec("gcovr") is not None):
            coverage = gcc_coverage(build_root)
        elif family == "msvc" and find_open_cpp_coverage() is not None:
            coverage = msvc_coverage(output, build_root)
        else:
            print(
                "watch_face_settings.c coverage deferred to the unified GCC "
                "coverage gate",
                flush=True,
            )
            return
        print(f"watch_face_settings.c line coverage: {coverage:.2f}%", flush=True)
        if coverage + 1e-9 < MINIMUM_LINE_COVERAGE:
            raise RuntimeError(
                f"watch_face_settings.c line coverage {coverage:.2f}% is below "
                f"{MINIMUM_LINE_COVERAGE:.2f}%"
            )


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"watch-face settings test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
