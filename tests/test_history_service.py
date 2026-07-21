"""Compile and execute the standalone history service tests."""

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
TEST_SOURCE = Path(__file__).with_name("history_service_test.c")
PRODUCTION_SOURCES = [
    APP_DIR / "services" / "history_service.c",
    APP_DIR / "services" / "storage_codec.c",
    APP_DIR / "services" / "store.c",
    APP_DIR / "services" / "storage_transaction.c",
    APP_DIR / "platform" / "storage" / "storage_fault.c",
    APP_DIR / "platform" / "storage" / "storage_memory.c",
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
        if shutil.which(requested):
            name = Path(requested).name.lower()
            return requested, "msvc" if name in {"cl", "cl.exe"} else "unix", None
        if Path(requested).name.lower() in {"cl", "cl.exe"}:
            environment = find_visual_studio_environment()
            if environment is not None:
                return "cl", "msvc", environment
        raise RuntimeError(f"requested C compiler is unavailable: {requested}")
    for candidate in ["cc", "gcc", "clang", "cl"]:
        if shutil.which(candidate):
            name = Path(candidate).name.lower()
            return candidate, "msvc" if name in {"cl", "cl.exe"} else "unix", None
    environment = find_visual_studio_environment()
    if environment is not None:
        return "cl", "msvc", environment
    raise RuntimeError("no C compiler found; install GCC/Clang/MSVC or set CC")


def compile_and_run() -> None:
    compiler, family, compiler_environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-history-service-") as temp:
        output = Path(temp) / ("history_service_test.exe" if os.name == "nt" else "history_service_test")
        sources = [str(TEST_SOURCE), *(str(source) for source in PRODUCTION_SOURCES)]
        if family == "msvc":
            command = [
                compiler, "/nologo", "/std:c11", "/W4", "/WX",
                "/D_CRT_SECURE_NO_WARNINGS", f"/I{INCLUDE_DIR}", *sources,
                f"/Fo{temp}{os.sep}", f"/Fe:{output}",
            ]
        else:
            command = [
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-pedantic", f"-I{INCLUDE_DIR}", *sources, "-o", str(output),
            ]

        print("compiling history service:", " ".join(command), flush=True)
        if compiler_environment is None:
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            batch = Path(temp) / "compile_history_service_test.bat"
            batch.write_text(
                "@echo off\r\n"
                f'call "{compiler_environment}" >nul || exit /b 1\r\n'
                f"{subprocess.list2cmdline(command)}\r\n",
                encoding="utf-8",
            )
            subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)


if __name__ == "__main__":
    try:
        compile_and_run()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"history service host test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
