"""Compile, execute, and measure the Q6 sync envelope codec tests."""

from __future__ import annotations

import binascii
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "openvela_app" / "smart_band"
INCLUDE_DIR = APP_DIR / "include"
TEST_SOURCE = Path(__file__).with_name("sync_protocol_test.c")
PRODUCTION_SOURCE = APP_DIR / "services" / "sync_protocol.c"
VECTOR_FILE = Path(__file__).with_name("vectors") / "sync-v1-envelope.json"
COVERAGE_MINIMUM = 90.0


def find_visual_studio_environment() -> Path | None:
    if os.name != "nt":
        return None
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.is_file():
        result = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
             "installationPath"],
            check=True, capture_output=True, text=True,
        )
        installation = result.stdout.strip()
        if installation:
            environment = Path(installation) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
            if environment.is_file():
                return environment
    editions = ["Community", "Professional", "Enterprise", "BuildTools"]
    for year in ["2022", "2019"]:
        for edition in editions:
            environment = (
                Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
                / "Microsoft Visual Studio" / year / edition
                / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
            )
            if environment.is_file():
                return environment
    return None


def find_compiler() -> tuple[str, str, Path | None]:
    requested = os.environ.get("CC")
    if requested:
        executable = shutil.which(requested)
        name = Path(requested).name.lower()
        if executable:
            return executable, "msvc" if name in {"cl", "cl.exe"} else "unix", None
        if name in {"cl", "cl.exe"}:
            environment = find_visual_studio_environment()
            if environment is not None:
                return "cl", "msvc", environment
        raise RuntimeError(f"requested C compiler is unavailable: {requested}")
    for candidate in ["gcc", "clang", "cc"]:
        executable = shutil.which(candidate)
        if executable:
            return executable, "unix", None
    executable = shutil.which("cl")
    if executable:
        return executable, "msvc", None
    environment = find_visual_studio_environment()
    if environment is not None:
        return "cl", "msvc", environment
    raise RuntimeError("no C11 compiler found; install GCC/Clang/MSVC or set CC")


def validate_vector() -> None:
    vector = json.loads(VECTOR_FILE.read_text(encoding="utf-8"))
    frame = bytes.fromhex(vector["frame_hex"])
    expected = bytes.fromhex(
        "53 42 01 00 01 03 00 00 04 00 78 56 34 12 01 02 03 04 "
        "de ad be ef c8 a6"
    )
    if frame != expected:
        raise RuntimeError("golden vector bytes drifted from the frozen v1 example")
    calculated_crc = binascii.crc_hqx(frame[:-2], 0xFFFF)
    stored_crc = int.from_bytes(frame[-2:], "little")
    if calculated_crc != stored_crc or stored_crc != int(vector["crc16"], 16):
        raise RuntimeError("golden vector CRC is inconsistent")
    check = vector["standard_crc_check"]
    check_crc = binascii.crc_hqx(check["ascii"].encode("ascii"), 0xFFFF)
    if check_crc != int(check["crc16"], 16) or check_crc != 0x29B1:
        raise RuntimeError("standard CRC-16/CCITT-FALSE check failed")


def run_command(command: list[str], *, cwd: Path, environment: Path | None,
                batch_path: Path | None = None) -> None:
    print("running:", subprocess.list2cmdline(command), flush=True)
    if environment is None:
        subprocess.run(command, cwd=cwd, check=True)
        return
    if batch_path is None:
        raise RuntimeError("MSVC environment requires a batch path")
    batch_path.write_text(
        "@echo off\r\n"
        f'call "{environment}" >nul || exit /b 1\r\n'
        f"{subprocess.list2cmdline(command)}\r\n",
        encoding="utf-8",
    )
    subprocess.run(["cmd.exe", "/d", "/c", str(batch_path)], cwd=cwd, check=True)


def compile_test(compiler: str, family: str, environment: Path | None,
                 build_root: Path) -> Path:
    output = build_root / ("sync_protocol_test.exe" if os.name == "nt" else "sync_protocol_test")
    sources = [str(TEST_SOURCE), str(PRODUCTION_SOURCE)]
    if family == "msvc":
        command = [
            compiler, "/nologo", "/std:c11", "/W4", "/WX", "/permissive-",
            "/TC", "/Od", "/Zi", f"/I{INCLUDE_DIR}", *sources,
            f"/Fo{build_root}{os.sep}", f"/Fd:{build_root / 'compiler.pdb'}",
            f"/Fe:{output}", "/link", "/DEBUG",
        ]
    else:
        command = [
            compiler, "-std=c11", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
            "-pedantic", f"-I{INCLUDE_DIR}", *sources, "-o", str(output),
        ]
    run_command(command, cwd=ROOT, environment=environment,
                batch_path=build_root / "compile.bat")
    subprocess.run([str(output)], cwd=ROOT, check=True)
    return output


def find_open_cpp_coverage() -> Path | None:
    configured = os.environ.get("OPENCPPCOVERAGE")
    candidates = [
        Path(configured) if configured else None,
        Path(shutil.which("OpenCppCoverage") or "") if shutil.which("OpenCppCoverage") else None,
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "OpenCppCoverage" / "OpenCppCoverage.exe",
        Path(tempfile.gettempdir()) / "smart-band-tools" / "OpenCppCoverage"
        / "OpenCppCoverage.exe",
    ]
    return next((path for path in candidates if path is not None and path.is_file()), None)


def run_msvc_coverage(output: Path, build_root: Path) -> float:
    coverage_tool = find_open_cpp_coverage()
    if coverage_tool is None:
        raise RuntimeError(
            "MSVC tests passed, but line coverage requires OpenCppCoverage; "
            "set OPENCPPCOVERAGE or run with GCC/gcov"
        )
    report = build_root / "coverage.xml"
    command = [
        str(coverage_tool), "--quiet", "--sources", str(PRODUCTION_SOURCE.parent),
        "--export_type", f"cobertura:{report}", "--", str(output),
    ]
    subprocess.run(command, cwd=ROOT, check=True)
    root = ET.parse(report).getroot()
    percent = float(root.attrib["line-rate"]) * 100.0
    return percent


def run_gcc_coverage(compiler: str, build_root: Path) -> float:
    if Path(compiler).name.lower() not in {"gcc", "gcc.exe"}:
        gcc = shutil.which("gcc")
        if gcc is None:
            raise RuntimeError(
                "strict compile passed, but line coverage requires GCC/gcov or "
                "OpenCppCoverage with MSVC"
            )
    else:
        gcc = compiler
    if shutil.which("gcov") is None:
        raise RuntimeError("GCC coverage requires gcov")
    coverage_dir = build_root / "gcc-coverage"
    coverage_dir.mkdir()
    output = coverage_dir / ("sync_protocol_coverage.exe" if os.name == "nt" else "sync_protocol_coverage")
    command = [
        gcc, "-std=c11", "-O0", "-g", "--coverage", "-fprofile-abs-path",
        "-Wall", "-Wextra", "-Werror", "-pedantic", f"-I{INCLUDE_DIR}",
        str(TEST_SOURCE), str(PRODUCTION_SOURCE), "-o", str(output),
    ]
    subprocess.run(command, cwd=coverage_dir, check=True)
    subprocess.run([str(output)], cwd=coverage_dir, check=True)
    report = coverage_dir / "summary.json"
    relative_source = PRODUCTION_SOURCE.relative_to(ROOT).as_posix()
    gcovr_command = [
        sys.executable, "-m", "gcovr", "--root", str(ROOT),
        "--object-directory", str(coverage_dir), "--filter",
        f"^{re.escape(relative_source)}$", "--exclude", "^tests/",
        "--json-summary", str(report), "--fail-under-line", str(COVERAGE_MINIMUM),
    ]
    subprocess.run(gcovr_command, cwd=ROOT, check=True)
    return float(json.loads(report.read_text(encoding="utf-8"))["line_percent"])


def main() -> None:
    validate_vector()
    compiler, family, environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-sync-protocol-") as temp:
        build_root = Path(temp)
        output = compile_test(compiler, family, environment, build_root)
        if family == "msvc":
            coverage_tool = find_open_cpp_coverage()
            if coverage_tool is None:
                print(
                    "sync_protocol.c coverage deferred to the unified GCC "
                    "coverage gate",
                    flush=True,
                )
                return
            coverage = run_msvc_coverage(output, build_root)
        else:
            coverage = run_gcc_coverage(compiler, build_root)
        print(f"sync_protocol.c line coverage: {coverage:.2f}%", flush=True)
        if coverage + 1e-9 < COVERAGE_MINIMUM:
            raise RuntimeError(
                f"sync_protocol.c line coverage {coverage:.2f}% is below "
                f"{COVERAGE_MINIMUM:.2f}%"
            )


if __name__ == "__main__":
    try:
        main()
    except (KeyError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Q6 sync protocol test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
