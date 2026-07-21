"""Compile and execute the standalone notification core tests."""

from __future__ import annotations

import os
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


def find_visual_studio_environment() -> Path | None:
    if os.name != "nt":
        return None
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    result = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
         "installationPath"],
        check=True, capture_output=True, text=True,
    )
    installation = result.stdout.strip()
    if not installation:
        return None
    environment = Path(installation) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    return environment if environment.is_file() else None


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
    for candidate in ["cc", "gcc", "clang", "cl"]:
        executable = shutil.which(candidate)
        if executable:
            name = Path(candidate).name.lower()
            return executable, "msvc" if name in {"cl", "cl.exe"} else "unix", None
    environment = find_visual_studio_environment()
    if environment is not None:
        return "cl", "msvc", environment
    raise RuntimeError("no C compiler found; install GCC/Clang/MSVC or set CC")


def compile_and_run() -> None:
    compiler, family, compiler_environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-notification-") as temp:
        build_dir = Path(temp)
        executable = build_dir / (
            "notification_core_test.exe" if os.name == "nt" else
            "notification_core_test"
        )
        sources = [str(TEST_SOURCE), *(str(source) for source in PRODUCTION_SOURCES)]
        if family == "msvc":
            command = [
                compiler, "/nologo", "/std:c11", "/W4", "/WX",
                f"/I{INCLUDE_DIR}", *sources, f"/Fo{temp}{os.sep}",
                f"/Fe:{executable}",
            ]
        else:
            command = [
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-pedantic", f"-I{INCLUDE_DIR}", *sources, "-o", str(executable),
            ]
        print("compiling notification core:", " ".join(command), flush=True)
        if compiler_environment is None:
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            batch = build_dir / "compile_notification_core_test.bat"
            batch.write_text(
                "@echo off\r\n"
                f'call "{compiler_environment}" >nul || exit /b 1\r\n'
                f"{subprocess.list2cmdline(command)}\r\n",
                encoding="utf-8",
            )
            subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT, check=True)
        subprocess.run([str(executable)], cwd=ROOT, check=True)


def main() -> None:
    for source in [TEST_SOURCE, *PRODUCTION_SOURCES]:
        if not source.is_file():
            raise RuntimeError(f"notification source is missing: {source}")
    compile_and_run()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"notification core test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
