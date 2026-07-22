"""Compile and execute the fixed-capacity storage transaction tests."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

from test_ui_compile import find_compiler


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "openvela_app" / "smart_band"
INCLUDE_DIR = APP_DIR / "include"
TEST_SOURCE = Path(__file__).with_name("storage_transaction_test.c")
PRODUCTION_SOURCES = [
    APP_DIR / "services" / "storage_codec.c",
    APP_DIR / "services" / "store.c",
    APP_DIR / "services" / "storage_transaction.c",
    APP_DIR / "platform" / "storage" / "storage_fault.c",
    APP_DIR / "platform" / "storage" / "storage_memory.c",
]


def compile_and_run() -> None:
    compiler, family, compiler_environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-storage-transaction-") as temp:
        build_root = Path(temp)
        output = build_root / (
            "storage_transaction_test.exe" if os.name == "nt" else "storage_transaction_test"
        )
        sources = [str(TEST_SOURCE), *(str(source) for source in PRODUCTION_SOURCES)]
        if family == "msvc":
            command = [
                compiler,
                "/nologo",
                "/std:c11",
                "/W4",
                "/WX",
                "/D_CRT_SECURE_NO_WARNINGS",
                f"/I{INCLUDE_DIR}",
                *sources,
                f"/Fo{build_root}{os.sep}",
                f"/Fe:{output}",
            ]
        else:
            command = [
                compiler,
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-pedantic",
                f"-I{INCLUDE_DIR}",
                *sources,
                "-o",
                str(output),
            ]

        print("compiling storage transaction tests:", " ".join(command), flush=True)
        if compiler_environment is None:
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            batch = build_root / "compile_storage_transaction_test.bat"
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
        print(f"storage transaction test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
