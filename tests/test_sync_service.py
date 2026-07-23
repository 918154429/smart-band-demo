"""Build and run the Q6 history sync session + loopback fault tests."""

from __future__ import annotations

import os
import binascii
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "openvela_app" / "smart_band"
INCLUDE = APP / "include"
SOURCES = [
    Path(__file__).with_name("sync_service_test.c"),
    APP / "services" / "sync_protocol.c",
    APP / "services" / "sync_service.c",
    APP / "platform" / "loopback" / "sync_loopback.c",
]
VECTOR = Path(__file__).with_name("vectors") / "sync-v1-history.json"


def validate_vector() -> None:
    vector = json.loads(VECTOR.read_text(encoding="utf-8"))
    frame = bytes.fromhex(vector["capabilities_frame_hex"])
    if len(frame) != 29 or int.from_bytes(frame[10:14], "little") != int(vector["transaction_id"], 16):
        raise RuntimeError("history capabilities golden vector metadata drifted")
    if frame[18] != 2 or int.from_bytes(frame[21:25], "little") != int(vector["feature_bits"], 16):
        raise RuntimeError("history feature vector payload drifted")
    if int.from_bytes(frame[25:27], "little") != vector["mtu"]:
        raise RuntimeError("history MTU vector payload drifted")
    if binascii.crc_hqx(frame[:-2], 0xFFFF) != int.from_bytes(frame[-2:], "little"):
        raise RuntimeError("history capabilities golden vector CRC is invalid")
    expected_lengths = {
        "history_request_frame_hex": 23,
        "history_data_frame_hex": 53,
        "history_ack_frame_hex": 23,
    }
    messages: dict[str, bytes] = {}
    for key, expected_length in expected_lengths.items():
        message = bytes.fromhex(vector[key])
        messages[key] = message
        if len(message) != expected_length:
            raise RuntimeError(f"{key} length drifted")
        if binascii.crc_hqx(message[:-2], 0xFFFF) != int.from_bytes(message[-2:], "little"):
            raise RuntimeError(f"{key} CRC is invalid")

    history_transaction = int(vector["history_transaction_id"], 16)
    resume_cursor = vector["history_resume_cursor"]
    total = vector["history_total"]
    request = messages["history_request_frame_hex"]
    data = messages["history_data_frame_hex"]
    ack = messages["history_ack_frame_hex"]
    if (
        request[4:7] != bytes((2, 0, 0))
        or int.from_bytes(request[10:14], "little") != history_transaction
        or int.from_bytes(request[14:18], "little") != resume_cursor
        or request[18] != 3
        or int.from_bytes(request[19:21], "little") != resume_cursor
    ):
        raise RuntimeError("history request golden fields drifted")
    if (
        data[4:7] != bytes((1, 3, 0))
        or int.from_bytes(data[10:14], "little") != history_transaction
        or int.from_bytes(data[14:18], "little") != resume_cursor
        or data[18] != 4
        or int.from_bytes(data[19:21], "little") != resume_cursor
        or int.from_bytes(data[21:23], "little") != total
    ):
        raise RuntimeError("history data golden envelope drifted")
    frozen_record = vector["history_record"]
    record = data[23:51]
    decoded_record = {
        "day_key": int.from_bytes(record[0:4], "little", signed=True),
        "steps": int.from_bytes(record[4:8], "little"),
        "active_seconds": int.from_bytes(record[8:12], "little"),
        "calories_milli_kcal": int.from_bytes(record[12:16], "little"),
        "heart_weighted_bpm_seconds": int.from_bytes(record[16:20], "little"),
        "heart_duration_seconds": int.from_bytes(record[20:24], "little"),
        "heart_min_bpm": record[24],
        "heart_max_bpm": record[25],
        "source_flags": record[26],
        "flags": record[27],
    }
    if decoded_record != frozen_record:
        raise RuntimeError("history daily-record golden fields drifted")
    if (
        ack[4:7] != bytes((3, 0, 0))
        or int.from_bytes(ack[10:14], "little") != history_transaction
        or int.from_bytes(ack[14:18], "little") != resume_cursor
        or ack[18] != 5
        or int.from_bytes(ack[19:21], "little") != resume_cursor + 1
    ):
        raise RuntimeError("history ACK golden fields drifted")
    if {item["name"] for item in vector["malformed"]} != {"truncated", "bad_crc", "wrong_message"}:
        raise RuntimeError("history malformed vector set drifted")


def find_vs_environment() -> Path | None:
    if os.name != "nt":
        return None
    vswhere = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    )
    if not vswhere.is_file():
        return None
    result = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
         "installationPath"],
        check=True, capture_output=True, text=True,
    )
    path = result.stdout.strip()
    environment = Path(path) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    return environment if path and environment.is_file() else None


def find_compiler() -> tuple[str, str, Path | None]:
    requested = os.environ.get("CC")
    candidates = [requested] if requested else ["gcc", "clang", "cc", "cl"]
    for candidate in candidates:
        if candidate and shutil.which(candidate):
            family = "msvc" if Path(candidate).name.lower() in {"cl", "cl.exe"} else "unix"
            return candidate, family, None
    if requested and Path(requested).name.lower() not in {"cl", "cl.exe"}:
        raise RuntimeError(f"requested compiler unavailable: {requested}")
    environment = find_vs_environment()
    if environment:
        return "cl", "msvc", environment
    raise RuntimeError("no C11 compiler found")


def main() -> None:
    validate_vector()
    compiler, family, environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-sync-service-") as temp:
        build = Path(temp)
        output = build / ("sync_service_test.exe" if os.name == "nt" else "sync_service_test")
        if family == "msvc":
            command = [
                compiler, "/nologo", "/std:c11", "/W4", "/WX", "/permissive-",
                "/TC", f"/I{INCLUDE}", *(str(path) for path in SOURCES),
                f"/Fo{build}{os.sep}", f"/Fe:{output}",
            ]
        else:
            command = [
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                f"-I{INCLUDE}", *(str(path) for path in SOURCES), "-o", str(output),
            ]
        print("running:", subprocess.list2cmdline(command), flush=True)
        if environment is None:
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            batch = build / "compile.bat"
            batch.write_text(
                "@echo off\r\n"
                f'call "{environment}" >nul || exit /b 1\r\n'
                f"{subprocess.list2cmdline(command)}\r\n",
                encoding="utf-8",
            )
            subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Q6 sync service test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
