"""Compile and execute the production watch model host test."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_SOURCE = ROOT / "openvela_app" / "smart_band" / "watch_model.c"
INCLUDE_DIR = ROOT / "openvela_app" / "smart_band" / "include"
TEST_SOURCE = Path(__file__).with_name("watch_model_test.c")


def find_visual_studio_environment() -> Path | None:
    if os.name != "nt":
        return None

    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None

    result = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    installation = result.stdout.strip()
    if not installation:
        return None

    environment = Path(installation) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    return environment if environment.is_file() else None


def find_compiler() -> tuple[str, str, Path | None]:
    requested = os.environ.get("CC")
    candidates = [requested] if requested else []
    candidates.extend(["cc", "gcc", "clang", "cl"])

    for candidate in candidates:
        if candidate and shutil.which(candidate):
            name = Path(candidate).name.lower()
            family = "msvc" if name in {"cl", "cl.exe"} else "unix"
            return candidate, family, None

    visual_studio_environment = find_visual_studio_environment()
    if visual_studio_environment is not None:
        return "cl", "msvc", visual_studio_environment

    raise RuntimeError(
        "no C compiler found; install GCC/Clang/MSVC or set CC to its executable"
    )


def compile_and_run() -> None:
    compiler, family, compiler_environment = find_compiler()

    with tempfile.TemporaryDirectory(prefix="smart-band-watch-model-") as temp:
        output = Path(temp) / ("watch_model_test.exe" if os.name == "nt" else "watch_model_test")
        if family == "msvc":
            command = [
                compiler,
                "/nologo",
                "/std:c11",
                "/W4",
                "/WX",
                "/D_CRT_SECURE_NO_WARNINGS",
                f"/I{INCLUDE_DIR}",
                str(TEST_SOURCE),
                str(MODEL_SOURCE),
                f"/Fo{temp}{os.sep}",
                f"/Fe:{output}",
            ]
        else:
            command = [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-pedantic",
                f"-I{INCLUDE_DIR}",
                str(TEST_SOURCE),
                str(MODEL_SOURCE),
                "-o",
                str(output),
            ]

        print("compiling production watch_model.c:", " ".join(command))
        if compiler_environment is None:
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            batch = Path(temp) / "compile_watch_model_test.bat"
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
        print(f"watch model host test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
