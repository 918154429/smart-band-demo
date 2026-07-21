"""Compile and run the watch-face registry and Lotus lifecycle tests."""

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
FAKE_LVGL_DIR = Path(__file__).with_name("fake_lvgl")
SOURCES = [
    Path(__file__).with_name("watch_face_registry_test.c"),
    Path(__file__).with_name("fake_lvgl_runtime.c"),
    APP_DIR / "watch_model.c",
    APP_DIR / "icon_assets.c",
    APP_DIR / "ui" / "lvgl" / "components.c",
    APP_DIR / "ui" / "lvgl" / "faces" / "lotus_face.c",
    APP_DIR / "ui" / "lvgl" / "faces" / "watch_face_registry.c",
]


def compile_and_run() -> None:
    compiler, family, compiler_environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-watch-face-") as temp:
        output = Path(temp) / (
            "watch_face_registry_test.exe" if os.name == "nt"
            else "watch_face_registry_test"
        )
        sources = [str(source) for source in SOURCES]
        if family == "msvc":
            command = [
                compiler, "/nologo", "/std:c11", "/W4", "/WX",
                "/D_CRT_SECURE_NO_WARNINGS", "/D_CRT_NONSTDC_NO_WARNINGS",
                f"/I{FAKE_LVGL_DIR}", f"/I{INCLUDE_DIR}", *sources,
                f"/Fo{temp}{os.sep}", f"/Fe:{output}",
            ]
        else:
            command = [
                compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-pedantic", f"-I{FAKE_LVGL_DIR}", f"-I{INCLUDE_DIR}",
                *sources, "-o", str(output),
            ]

        print("compiling watch-face registry tests:", " ".join(command),
              flush=True)
        if compiler_environment is None:
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            batch = Path(temp) / "compile_watch_face_registry.bat"
            batch.write_text(
                "@echo off\r\n"
                f'call "{compiler_environment}" >nul || exit /b 1\r\n'
                f"{subprocess.list2cmdline(command)}\r\n",
                encoding="utf-8",
            )
            subprocess.run(["cmd.exe", "/d", "/c", str(batch)],
                           cwd=ROOT, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)


if __name__ == "__main__":
    try:
        compile_and_run()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"watch-face registry test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
