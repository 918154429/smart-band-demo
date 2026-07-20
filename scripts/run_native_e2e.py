#!/usr/bin/env python3
"""Run the native watch-face -> heart-page -> sensor-value E2E journey."""

from __future__ import annotations

import argparse
import binascii
import datetime as dt
import hashlib
import importlib.util
import json
import os
import re
import secrets
import signal
import shutil
import socket
import struct
import sys
import time
import traceback
import zlib
from pathlib import Path
from typing import Any, NamedTuple


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_DIR = "cmake_out/vela_goldfish-arm64-v8a-ap"
RUNTIME_INPUTS = (".config", "nuttx", "vela_system.bin", "vela_data.bin")
EXPECTED_WIDTH = 1280
EXPECTED_HEIGHT = 800
PAGE_TRANSITION_REGION = (470, 210, 810, 660)
PAGE_TITLE_REGION = (570, 165, 710, 200)
HEART_VALUE_REGION = (550, 330, 790, 390)
SOURCE_LABEL_REGION = (650, 490, 790, 545)
MIN_CHANGED_PIXELS = 32
MAX_SWIPE_ATTEMPTS = 3
RUN_ID_ENV = "SMART_BAND_NATIVE_E2E_RUN_ID"
GOLDEN_REGIONS = {
    "heart_page_title": {
        "expected_text": "Heart Rate",
        "region": PAGE_TITLE_REGION,
        "sha256": "ee5e90a2ec9ee48f2c47de74f91f49760042011cb7ba8f80b6ed7a9209077688",
    },
    "heart_value_104_bpm": {
        "expected_text": "104 bpm",
        "region": HEART_VALUE_REGION,
        "sha256": "6eda2500a3515df5da0624356d877b95f5208503d8947da80393100595664761",
    },
    "source_sensor": {
        "expected_text": "Source / Sensor",
        "region": SOURCE_LABEL_REGION,
        "sha256": "cfd1eec161120d19aa5a3b7a46c704bb8078b38a8adf78b0a0fa8edf44f1c60a",
    },
}
REQUIRED_CHECKS = (
    "runtime_staged_isolated",
    "fixed_output_unchanged",
    "runtime_input_cleanup",
    "nsh_ready",
    "uorb_heart_node",
    "console_ping",
    "watch_face_png",
    "swipe_console_ok",
    "heart_model_png",
    "page_transition_pixels",
    "heart_page_title_golden",
    "sensor_enabled",
    "sensor_set_console_ok",
    "sensor_get_104",
    "heart_sensor_png",
    "heart_value_transition_pixels",
    "source_label_transition_pixels",
    "heart_value_104_bpm_golden",
    "source_sensor_golden",
    "heart_metric_structured_state",
    "pidof",
    "ps",
    "app_no_fatal",
    "process_cleanup",
    "attributed_process_cleanup",
    "port_cleanup",
)


class NativeE2EFailure(RuntimeError):
    """Raised when the native journey cannot prove a required assertion."""


class PngImage(NamedTuple):
    width: int
    height: int
    pixels: bytes


def load_smoke_module(path: Path = ROOT / "scripts" / "smoke_openvela_emulator.py"):
    spec = importlib.util.spec_from_file_location("native_e2e_smoke", path)
    if spec is None or spec.loader is None:
        raise NativeE2EFailure(f"cannot load emulator smoke helpers: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SMOKE = load_smoke_module()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a native screenshot, swipe, and heart-sensor E2E journey."
    )
    parser.add_argument("--openvela-root", required=True, type=Path)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--console-port", type=int, default=5554)
    parser.add_argument("--boot-timeout", type=float, default=240.0)
    parser.add_argument("--command-timeout", type=float, default=30.0)
    parser.add_argument("--ui-settle-seconds", type=float, default=1.0)
    parser.add_argument("--post-swipe-seconds", type=float, default=1.2)
    parser.add_argument("--swipe-step-delay", type=float, default=0.03)
    parser.add_argument("--sensor-samples", type=int, default=12)
    parser.add_argument("--sensor-sample-delay", type=float, default=0.25)
    parser.add_argument("--sensor-settle-seconds", type=float, default=1.2)
    return parser.parse_args()


def write_text(path: Path, value: str) -> None:
    path.write_text(value, encoding="utf-8", errors="replace")


def write_json(path: Path, value: Any) -> None:
    write_text(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def ensure_empty_evidence_dir(path: Path) -> None:
    if path.exists():
        if not path.is_dir():
            raise NativeE2EFailure(f"evidence path is not a directory: {path}")
        if any(path.iterdir()):
            raise NativeE2EFailure(f"evidence directory is not empty: {path}")
        return
    path.mkdir(parents=True)


def paths_overlap(first: Path, second: Path) -> bool:
    first = first.resolve()
    second = second.resolve()
    return first == second or first in second.parents or second in first.parents


def require_disjoint_paths(source_output: Path, evidence_dir: Path) -> None:
    if paths_overlap(source_output, evidence_dir):
        raise NativeE2EFailure(
            "evidence directory and fixed output must not overlap: "
            f"{evidence_dir} <-> {source_output}"
        )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_fingerprint(path: Path) -> dict[str, Any]:
    stat = path.stat()
    return {
        "path": str(path),
        "bytes": stat.st_size,
        "device": stat.st_dev,
        "inode": stat.st_ino,
        "mtime_ns": stat.st_mtime_ns,
        "sha256": sha256_file(path),
    }


def stage_runtime_output(source_output: Path, runtime_output: Path) -> dict[str, Any]:
    if runtime_output.exists():
        raise NativeE2EFailure(f"runtime output already exists: {runtime_output}")
    runtime_output.mkdir()
    records: dict[str, Any] = {}
    try:
        for name in RUNTIME_INPUTS:
            source = source_output / name
            destination = runtime_output / name
            if not source.is_file() or source.stat().st_size == 0:
                raise NativeE2EFailure(f"runtime input is missing or empty: {source}")
            source_before = file_fingerprint(source)
            try:
                shutil.copy2(source, destination)
            except OSError as exc:
                raise NativeE2EFailure(
                    f"runtime staging failed for {source} -> {destination}: {exc}"
                ) from exc
            staged = file_fingerprint(destination)
            hardlinked = (
                source_before["device"] == staged["device"]
                and source_before["inode"] == staged["inode"]
            )
            isolated = not hardlinked
            initial_hash_matches = source_before["sha256"] == staged["sha256"]
            if not isolated or not initial_hash_matches:
                raise NativeE2EFailure(
                    f"runtime input is not an isolated identical copy: {destination}"
                )
            records[name] = {
                "source": source_before,
                "staged": staged,
                "method": "copy2",
                "hardlinked": hardlinked,
                "isolated": isolated,
                "initial_hash_matches": initial_hash_matches,
            }

        config = runtime_output / "config.ini"
        write_text(config, "hw.sensors.heart_rate=yes\n")
    except Exception:
        try:
            shutil.rmtree(runtime_output)
        except OSError as cleanup_error:
            raise NativeE2EFailure(
                f"runtime staging rollback failed for {runtime_output}: {cleanup_error}"
            ) from cleanup_error
        raise
    return {
        "source_output": str(source_output),
        "runtime_output": str(runtime_output),
        "config_ini": {
            "path": str(config),
            "content": "hw.sensors.heart_rate=yes\n",
        },
        "files": records,
    }


def verify_fixed_output_unchanged(runtime: dict[str, Any]) -> bool:
    unchanged = True
    for record in runtime["files"].values():
        before = record["source"]
        after = file_fingerprint(Path(before["path"]))
        staged_after = file_fingerprint(Path(record["staged"]["path"]))
        record["source_after"] = after
        record["staged_after"] = staged_after
        record["source_unchanged"] = all(
            before[key] == after[key]
            for key in ("bytes", "device", "inode", "mtime_ns", "sha256")
        )
        unchanged = unchanged and record["source_unchanged"]
    return unchanged


def cleanup_staged_runtime_inputs(runtime_output: Path) -> dict[str, Any]:
    removed: list[str] = []
    missing: list[str] = []
    errors: list[str] = []
    inventory: list[dict[str, Any]] = []
    if runtime_output.is_symlink():
        try:
            runtime_output.unlink()
        except OSError as exc:
            errors.append(f"cannot remove runtime-output symlink: {exc}")
        return {
            "removed": removed,
            "already_missing": list(RUNTIME_INPUTS),
            "inventory": inventory,
            "errors": ["runtime output became a symlink", *errors],
            "tree_removed": not runtime_output.exists(),
            "passed": False,
        }
    if runtime_output.is_dir():
        for path in sorted(item for item in runtime_output.rglob("*") if item.is_file()):
            inventory.append(
                {
                    "path": path.relative_to(runtime_output).as_posix(),
                    "bytes": path.stat().st_size,
                }
            )
    for name in RUNTIME_INPUTS:
        path = runtime_output / name
        if path.parent != runtime_output:
            raise NativeE2EFailure(f"runtime cleanup escaped staging directory: {path}")
        if not path.exists():
            missing.append(name)
            continue
        try:
            path.unlink()
            removed.append(name)
        except OSError as exc:
            errors.append(f"cannot remove {name}: {exc}")
    if runtime_output.exists():
        try:
            shutil.rmtree(runtime_output)
        except OSError as exc:
            errors.append(f"cannot remove runtime output tree: {exc}")
    tree_removed = not runtime_output.exists()
    return {
        "removed": removed,
        "already_missing": missing,
        "inventory": inventory,
        "errors": errors,
        "tree_removed": tree_removed,
        "passed": tree_removed and not missing and not errors,
    }


def write_evidence_manifest(evidence_dir: Path) -> int:
    manifest = evidence_dir / "evidence.sha256"
    files = sorted(
        path
        for path in evidence_dir.rglob("*")
        if path.is_file() and path != manifest
    )
    lines = [
        f"{sha256_file(path)}  {path.relative_to(evidence_dir).as_posix()}"
        for path in files
    ]
    write_text(manifest, "\n".join(lines) + ("\n" if lines else ""))
    return len(files)


def _paeth(left: int, up: int, upper_left: int) -> int:
    estimate = left + up - upper_left
    left_distance = abs(estimate - left)
    up_distance = abs(estimate - up)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= up_distance and left_distance <= upper_left_distance:
        return left
    if up_distance <= upper_left_distance:
        return up
    return upper_left


def decode_png_rgba(path: Path) -> PngImage:
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise NativeE2EFailure(f"not a PNG file: {path}")

    offset = 8
    width = height = bit_depth = color_type = interlace = None
    idat = bytearray()
    saw_iend = False
    while offset < len(data):
        if offset + 12 > len(data):
            raise NativeE2EFailure(f"truncated PNG chunk in {path}")
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        kind = data[offset + 4 : offset + 8]
        payload_start = offset + 8
        payload_end = payload_start + length
        if payload_end + 4 > len(data):
            raise NativeE2EFailure(f"truncated PNG payload in {path}")
        payload = data[payload_start:payload_end]
        expected_crc = struct.unpack(">I", data[payload_end : payload_end + 4])[0]
        actual_crc = binascii.crc32(kind + payload) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise NativeE2EFailure(f"PNG CRC mismatch for {kind!r} in {path}")
        offset = payload_end + 4

        if kind == b"IHDR":
            if len(payload) != 13 or width is not None:
                raise NativeE2EFailure(f"invalid PNG IHDR in {path}")
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", payload)
            )
            if compression != 0 or filtering != 0:
                raise NativeE2EFailure(f"unsupported PNG compression/filter method in {path}")
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            saw_iend = True
            break

    if width is None or not saw_iend or not idat:
        raise NativeE2EFailure(f"incomplete PNG structure: {path}")
    if bit_depth != 8 or color_type != 6 or interlace != 0:
        raise NativeE2EFailure(
            f"expected non-interlaced RGBA8 PNG, got bit_depth={bit_depth}, "
            f"color_type={color_type}, interlace={interlace}: {path}"
        )

    bytes_per_pixel = 4
    stride = width * bytes_per_pixel
    try:
        filtered = zlib.decompress(bytes(idat))
    except zlib.error as exc:
        raise NativeE2EFailure(f"cannot decompress PNG pixels in {path}: {exc}") from exc
    expected_size = height * (stride + 1)
    if len(filtered) != expected_size:
        raise NativeE2EFailure(
            f"unexpected PNG pixel payload size {len(filtered)} != {expected_size}: {path}"
        )

    pixels = bytearray(width * height * bytes_per_pixel)
    previous = bytearray(stride)
    source_offset = 0
    for row_index in range(height):
        filter_type = filtered[source_offset]
        source_offset += 1
        row = bytearray(filtered[source_offset : source_offset + stride])
        source_offset += stride
        if filter_type not in (0, 1, 2, 3, 4):
            raise NativeE2EFailure(f"unsupported PNG row filter {filter_type}: {path}")
        for index, value in enumerate(row):
            left = row[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            up = previous[index]
            upper_left = previous[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            if filter_type == 1:
                row[index] = (value + left) & 0xFF
            elif filter_type == 2:
                row[index] = (value + up) & 0xFF
            elif filter_type == 3:
                row[index] = (value + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                row[index] = (value + _paeth(left, up, upper_left)) & 0xFF
        destination = row_index * stride
        pixels[destination : destination + stride] = row
        previous = row
    return PngImage(width, height, bytes(pixels))


def screenshot_record(path: Path, expected_width: int, expected_height: int) -> tuple[PngImage, dict[str, Any]]:
    image = decode_png_rgba(path)
    if (image.width, image.height) != (expected_width, expected_height):
        raise NativeE2EFailure(
            f"unexpected screenshot size {image.width}x{image.height}, "
            f"expected {expected_width}x{expected_height}: {path}"
        )

    colors: set[bytes] = set()
    channel_min = [255, 255, 255, 255]
    channel_max = [0, 0, 0, 0]
    for index in range(0, len(image.pixels), 4):
        pixel = image.pixels[index : index + 4]
        colors.add(pixel)
        for channel, value in enumerate(pixel):
            channel_min[channel] = min(channel_min[channel], value)
            channel_max[channel] = max(channel_max[channel], value)
    nonblank = len(colors) > 1 and any(
        channel_max[index] > channel_min[index] for index in range(3)
    )
    if not nonblank:
        raise NativeE2EFailure(f"screenshot is blank: {path}")
    return image, {
        "path": str(path),
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "width": image.width,
        "height": image.height,
        "format": "RGBA8 non-interlaced PNG",
        "unique_rgba_colors": len(colors),
        "channel_min": channel_min,
        "channel_max": channel_max,
        "nonblank": nonblank,
    }


def region_difference(
    before: PngImage, after: PngImage, region: tuple[int, int, int, int]
) -> dict[str, Any]:
    if (before.width, before.height) != (after.width, after.height):
        raise NativeE2EFailure("cannot compare screenshots with different dimensions")
    left, top, right, bottom = region
    if not (0 <= left < right <= before.width and 0 <= top < bottom <= before.height):
        raise NativeE2EFailure(f"invalid screenshot comparison region: {region}")

    changed = 0
    channel_delta_total = 0
    max_channel_delta = 0
    for y in range(top, bottom):
        for x in range(left, right):
            offset = (y * before.width + x) * 4
            first = before.pixels[offset : offset + 4]
            second = after.pixels[offset : offset + 4]
            if first != second:
                changed += 1
            for channel in range(3):
                delta = abs(first[channel] - second[channel])
                channel_delta_total += delta
                max_channel_delta = max(max_channel_delta, delta)
    total_pixels = (right - left) * (bottom - top)
    return {
        "region": [left, top, right, bottom],
        "changed_pixels": changed,
        "total_pixels": total_pixels,
        "changed_ratio": round(changed / total_pixels, 6),
        "mean_absolute_rgb_delta": round(
            channel_delta_total / (total_pixels * 3), 6
        ),
        "max_channel_delta": max_channel_delta,
    }


def region_fingerprint(
    image: PngImage, region: tuple[int, int, int, int]
) -> dict[str, Any]:
    left, top, right, bottom = region
    if not (0 <= left < right <= image.width and 0 <= top < bottom <= image.height):
        raise NativeE2EFailure(f"invalid screenshot fingerprint region: {region}")
    pixels = b"".join(
        image.pixels[
            (y * image.width + left) * 4 : (y * image.width + right) * 4
        ]
        for y in range(top, bottom)
    )
    return {
        "region": [left, top, right, bottom],
        "bytes": len(pixels),
        "sha256": hashlib.sha256(pixels).hexdigest(),
    }


def golden_region_record(image: PngImage, name: str) -> dict[str, Any]:
    expected = GOLDEN_REGIONS[name]
    record = region_fingerprint(image, expected["region"])
    record.update(
        {
            "expected_text": expected["expected_text"],
            "expected_sha256": expected["sha256"],
            "matches": record["sha256"] == expected["sha256"],
        }
    )
    return record


def build_swipe_commands() -> list[str]:
    points = [(x, 400, 1) for x in range(800, 479, -40)]
    points.append((480, 400, 0))
    return [f"event mouse {x} {y} 0 {button}" for x, y, button in points]


def console_response_ok(response: str) -> bool:
    return re.search(r"(?:^|\r?\n)OK\r?\n?$", response) is not None


def uorb_node_present(output: str) -> bool:
    clean = output.replace("\r", "")
    return re.search(r"(?m)^\s*c[rw-]+.*?/dev/uorb/sensor_hrate0\s*$", clean) is not None


def port_available(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        try:
            probe.bind(("127.0.0.1", port))
        except OSError:
            return False
    return True


def wait_for_port_available(port: int, timeout: float = 5.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if port_available(port):
            return True
        time.sleep(0.1)
    return port_available(port)


def process_group_exists(pgid: int | None) -> bool:
    if pgid is None or os.name != "posix":
        return False
    try:
        os.killpg(pgid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def attributed_process_snapshot(
    run_id: str, runtime_output: Path, executables: tuple[Path, ...]
) -> list[dict[str, Any]]:
    if os.name != "posix" or not Path("/proc").is_dir():
        return []
    run_marker = f"{RUN_ID_ENV}={run_id}".encode()
    runtime_marker = os.fsencode(str(runtime_output.resolve()))
    executable_paths = {str(path.resolve()) for path in executables}
    records = []
    for process_dir in Path("/proc").iterdir():
        if not process_dir.name.isdigit():
            continue
        pid = int(process_dir.name)
        if pid == os.getpid():
            continue
        try:
            stat_text = (process_dir / "stat").read_text(encoding="utf-8")
            tail = stat_text[stat_text.rfind(")") + 2 :].split()
            command_bytes = (process_dir / "cmdline").read_bytes()
            environment = (process_dir / "environ").read_bytes()
            executable = os.readlink(process_dir / "exe")
            identity = {
                "pid": pid,
                "ppid": int(tail[1]),
                "pgid": int(tail[2]),
                "sid": int(tail[3]),
                "starttime_ticks": int(tail[19]),
                "executable": executable,
                "command": command_bytes.replace(b"\0", b" ")
                .decode("utf-8", errors="replace")
                .strip(),
            }
        except (FileNotFoundError, PermissionError, ProcessLookupError, OSError, ValueError):
            continue
        reasons = []
        if run_marker in environment.split(b"\0"):
            reasons.append("run_id")
        if runtime_marker in command_bytes:
            reasons.append("runtime_path")
        if executable in executable_paths:
            reasons.append("emulator_executable")
        if reasons:
            identity["reasons"] = reasons
            records.append(identity)
    return sorted(records, key=lambda record: (record["pid"], record["starttime_ticks"]))


def cleanup_attributed_processes(
    run_id: str,
    runtime_output: Path,
    executables: tuple[Path, ...],
    baseline: list[dict[str, Any]],
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "run_id": run_id,
        "supported": os.name == "posix" and Path("/proc").is_dir(),
        "baseline": baseline,
        "initial": [],
        "actions": [],
        "remaining": [],
        "absent": False,
    }
    if not result["supported"]:
        return result
    baseline_identities = {
        (record["pid"], record["starttime_ticks"]) for record in baseline
    }

    def new_processes() -> list[dict[str, Any]]:
        return [
            record
            for record in attributed_process_snapshot(run_id, runtime_output, executables)
            if (record["pid"], record["starttime_ticks"]) not in baseline_identities
        ]

    result["initial"] = new_processes()
    for process in result["initial"]:
        if not ({"run_id", "runtime_path"} & set(process["reasons"])):
            continue
        try:
            os.kill(process["pid"], signal.SIGTERM)
            result["actions"].append(f"sent SIGTERM to attributed process {process['pid']}")
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 5.0
    remaining = new_processes()
    while remaining and time.monotonic() < deadline:
        time.sleep(0.1)
        remaining = new_processes()
    for process in remaining:
        if not ({"run_id", "runtime_path"} & set(process["reasons"])):
            continue
        try:
            os.kill(process["pid"], signal.SIGKILL)
            result["actions"].append(f"sent SIGKILL to attributed process {process['pid']}")
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 2.0
    remaining = new_processes()
    while remaining and time.monotonic() < deadline:
        time.sleep(0.1)
        remaining = new_processes()
    result["remaining"] = remaining
    result["absent"] = not remaining
    return result


def all_required_checks_pass(checks: dict[str, bool]) -> bool:
    return all(checks.get(name) is True for name in REQUIRED_CHECKS)


def require_check(
    checks: dict[str, bool], name: str, condition: bool, message: str
) -> None:
    checks[name] = bool(condition)
    if not condition:
        raise NativeE2EFailure(message)


def capture_screenshot(
    console: Any,
    evidence_dir: Path,
    name: str,
    expected_width: int = EXPECTED_WIDTH,
    expected_height: int = EXPECTED_HEIGHT,
) -> tuple[PngImage, dict[str, Any]]:
    capture_dir = evidence_dir / "raw-screenshots" / name
    capture_dir.mkdir(parents=True)
    response = console.command(
        f"screenrecord screenshot {capture_dir}",
        f"console-screenshot-{name}.txt",
    )
    deadline = time.monotonic() + 5.0
    candidates: list[Path] = []
    while time.monotonic() < deadline:
        candidates = sorted(path for path in capture_dir.glob("*.png") if path.is_file())
        if candidates and all(path.stat().st_size > 0 for path in candidates):
            break
        time.sleep(0.05)
    if len(candidates) != 1:
        raise NativeE2EFailure(
            f"expected one screenshot for {name}, found {len(candidates)} in {capture_dir}"
        )
    destination = evidence_dir / f"{name}.png"
    os.replace(candidates[0], destination)
    image, record = screenshot_record(destination, expected_width, expected_height)
    record["console_response"] = response.strip()
    record["console_ok"] = console_response_ok(response)
    return image, record


def run_journey(args: argparse.Namespace) -> int:
    if not 1 <= args.console_port <= 65535:
        raise NativeE2EFailure("console port must be between 1 and 65535")
    if args.sensor_samples <= 0:
        raise NativeE2EFailure("sensor sample count must be positive")
    for name in (
        "boot_timeout",
        "command_timeout",
        "ui_settle_seconds",
        "post_swipe_seconds",
        "swipe_step_delay",
        "sensor_sample_delay",
        "sensor_settle_seconds",
    ):
        if getattr(args, name) < 0:
            raise NativeE2EFailure(f"{name.replace('_', '-')} must not be negative")

    openvela_root = args.openvela_root.resolve()
    source_output = SMOKE.resolve_under(openvela_root, args.output_dir).resolve()
    evidence_dir = args.evidence_dir.resolve()
    require_disjoint_paths(source_output, evidence_dir)
    ensure_empty_evidence_dir(evidence_dir)
    runtime_output = evidence_dir / "runtime-output"
    run_id = secrets.token_hex(16)
    checks: dict[str, bool] = {}
    journey: dict[str, Any] = {
        "status": "running",
        "phase": "preflight",
        "started_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "openvela_root": str(openvela_root),
        "fixed_output": str(source_output),
        "evidence_dir": str(evidence_dir),
        "console_port": args.console_port,
        "run_id": run_id,
        "harness": {
            "path": str(Path(__file__).resolve()),
            "sha256": sha256_file(Path(__file__).resolve()),
        },
        "smoke_helper": {
            "path": str((ROOT / "scripts" / "smoke_openvela_emulator.py").resolve()),
            "sha256": sha256_file(
                (ROOT / "scripts" / "smoke_openvela_emulator.py").resolve()
            ),
        },
        "checks": checks,
        "nsh": {},
        "console": {},
        "screenshots": {},
        "pixel_transitions": {},
        "golden_regions": {},
        "rendered_metric_state": {},
        "process_attribution": {},
        "cleanup": {},
    }

    def checkpoint(phase: str) -> None:
        journey["phase"] = phase
        write_json(evidence_dir / "journey.json", journey)

    checkpoint("preflight")
    child = None
    console = None
    emulator_pgid: int | None = None
    cleanup_log: list[str] = []
    failure: dict[str, Any] | None = None
    runtime: dict[str, Any] | None = None
    attributed_baseline: list[dict[str, Any]] = []
    attributed_executables: tuple[Path, ...] = ()
    previous_handlers: dict[signal.Signals, Any] = {}

    def handle_signal(signum: int, _frame: Any) -> None:
        raise NativeE2EFailure(f"received {signal.Signals(signum).name}")

    try:
        for handled_signal in (signal.SIGTERM, signal.SIGINT):
            previous_handlers[handled_signal] = signal.getsignal(handled_signal)
            signal.signal(handled_signal, handle_signal)

        require_check(
            checks,
            "port_preflight",
            port_available(args.console_port),
            f"console port is already in use: {args.console_port}",
        )
        runtime = stage_runtime_output(source_output, runtime_output)
        journey["runtime"] = runtime
        runtime_isolated = all(
            runtime["files"][name]["isolated"]
            and runtime["files"][name]["initial_hash_matches"]
            and runtime["files"][name]["method"] == "copy2"
            for name in RUNTIME_INPUTS
        )
        require_check(
            checks,
            "runtime_staged_isolated",
            runtime_isolated,
            "runtime staging did not isolate every fixed input",
        )
        checkpoint("runtime-staged")

        emulator_script = openvela_root / "emulator.sh"
        emulator_root = openvela_root / "prebuilts/emulator/linux-x86_64"
        emulator_binary = emulator_root / "emulator"
        emulator_library_path = emulator_root / "lib64"
        qemu_headless = (
            emulator_root / "qemu/linux-x86_64/qemu-system-aarch64-headless"
        )
        opengl_renderer = emulator_library_path / "libOpenglRender.so"
        skin_dir = openvela_root / "prebuilts/emulator/skins"
        SMOKE.validate_runtime_inputs(emulator_script, runtime_output)
        if not skin_dir.is_dir():
            raise NativeE2EFailure(f"emulator skin directory is missing: {skin_dir}")
        if not qemu_headless.is_file() or not os.access(qemu_headless, os.X_OK):
            raise NativeE2EFailure(f"headless QEMU is missing or not executable: {qemu_headless}")
        SMOKE.verify_elf(emulator_binary, evidence_dir, "emulator")
        SMOKE.verify_elf(
            qemu_headless,
            evidence_dir,
            "qemu-headless",
            emulator_library_path,
        )
        SMOKE.verify_elf(
            opengl_renderer,
            evidence_dir,
            "opengl-renderer",
            emulator_library_path,
            require_executable=False,
        )
        attributed_executables = (emulator_binary, qemu_headless)
        attributed_baseline = attributed_process_snapshot(
            run_id, runtime_output, attributed_executables
        )
        journey["process_attribution"] = {
            "environment": RUN_ID_ENV,
            "run_id": run_id,
            "runtime_output": str(runtime_output),
            "executables": [str(path) for path in attributed_executables],
            "baseline": attributed_baseline,
        }
        prompt_text = SMOKE.config_value(runtime_output / ".config", "CONFIG_NSH_PROMPT_STRING")
        prompt = prompt_text.encode("utf-8")
        command = [
            str(emulator_script),
            str(runtime_output),
            "-no-window",
            "-no-audio",
            "-accel",
            "off",
            "-port",
            str(args.console_port),
            "-skin",
            "xiaomi_smart_screen_10",
            "-skindir",
            str(skin_dir),
        ]
        journey["emulator_command"] = command
        write_text(evidence_dir / "emulator-command.txt", " ".join(command) + "\n")

        child = SMOKE.PtyChild(command, openvela_root, evidence_dir / "emulator.log")
        previous_run_id = os.environ.get(RUN_ID_ENV)
        os.environ[RUN_ID_ENV] = run_id
        try:
            child.start()
        finally:
            if previous_run_id is None:
                os.environ.pop(RUN_ID_ENV, None)
            else:
                os.environ[RUN_ID_ENV] = previous_run_id
        if child.pid is None:
            raise NativeE2EFailure("emulator PTY did not return a child PID")
        emulator_pgid = os.getpgid(child.pid)
        journey["emulator_process"] = {"pid": child.pid, "pgid": emulator_pgid}
        write_json(evidence_dir / "emulator-process.json", journey["emulator_process"])
        child.wait_for(prompt, 0, args.boot_timeout)

        ready_marker = "SMART_BAND_NATIVE_E2E_READY"
        ready_output = child.send_command(
            f"echo {ready_marker}",
            prompt,
            args.command_timeout,
            evidence_dir / "nsh-ready.txt",
        )
        ready_ok = ready_marker in [
            line.strip() for line in ready_output.replace("\r", "").splitlines()
        ]
        journey["nsh"]["ready"] = ready_ok
        require_check(checks, "nsh_ready", ready_ok, "NSH readiness command failed")

        uorb_output = child.send_command(
            "ls -l /dev/uorb/sensor_hrate0",
            prompt,
            args.command_timeout,
            evidence_dir / "nsh-uorb-heart-rate.txt",
        )
        uorb_ok = uorb_node_present(uorb_output)
        journey["nsh"]["heart_rate_node"] = uorb_ok
        require_check(
            checks,
            "uorb_heart_node",
            uorb_ok,
            "NSH did not expose /dev/uorb/sensor_hrate0",
        )

        console = SMOKE.connect_console(args.console_port, evidence_dir)
        ping_output = console.command("ping", "console-ping.txt")
        ping_ok = "I am alive!" in ping_output and console_response_ok(ping_output)
        journey["console"]["ping"] = ping_output.strip()
        require_check(checks, "console_ping", ping_ok, "emulator console ping failed")

        app_transcript_start = len(child.transcript)
        app_started_at = time.monotonic()
        child.send_command(
            "smart_band &",
            prompt,
            args.command_timeout,
            evidence_dir / "nsh-smart-band-launch.txt",
        )
        child.wait_for(
            b"smart_band: UI ready",
            app_transcript_start,
            args.command_timeout,
        )
        journey["app_ui_ready_seconds"] = round(
            time.monotonic() - app_started_at, 6
        )
        child.pump(args.ui_settle_seconds)
        checkpoint("watch-face")

        watch_image, watch_record = capture_screenshot(
            console, evidence_dir, "watch-face"
        )
        journey["screenshots"]["watch_face"] = watch_record
        require_check(
            checks,
            "watch_face_png",
            watch_record["console_ok"] and watch_record["nonblank"],
            "watch-face screenshot failed validation",
        )
        checkpoint("swipe")

        swipe_attempts = []
        model_image = None
        model_record = None
        page_difference = None
        title_golden = None
        for attempt in range(1, MAX_SWIPE_ATTEMPTS + 1):
            swipe_results = []
            for index, command_text in enumerate(build_swipe_commands(), 1):
                response = console.command(
                    command_text,
                    f"console-swipe-{attempt:02d}-{index:02d}.txt",
                )
                swipe_results.append(
                    {
                        "command": command_text,
                        "response": response.strip(),
                        "ok": console_response_ok(response),
                    }
                )
                time.sleep(args.swipe_step_delay)
            child.pump(args.post_swipe_seconds)
            attempt_image, attempt_record = capture_screenshot(
                console, evidence_dir, f"heart-model-attempt-{attempt:02d}"
            )
            attempt_difference = region_difference(
                watch_image, attempt_image, PAGE_TRANSITION_REGION
            )
            attempt_title = golden_region_record(attempt_image, "heart_page_title")
            swipe_attempts.append(
                {
                    "attempt": attempt,
                    "commands": swipe_results,
                    "screenshot": attempt_record,
                    "page_difference": attempt_difference,
                    "title_golden": attempt_title,
                }
            )
            model_image = attempt_image
            model_record = attempt_record
            page_difference = attempt_difference
            title_golden = attempt_title
            if attempt_title["matches"]:
                canonical = evidence_dir / "heart-model.png"
                shutil.copy2(Path(attempt_record["path"]), canonical)
                model_record = dict(attempt_record)
                model_record["path"] = str(canonical)
                break
        journey["console"]["swipe_attempts"] = swipe_attempts
        journey["console"]["swipe"] = swipe_attempts[-1]["commands"]
        require_check(
            checks,
            "swipe_console_ok",
            all(
                item["ok"]
                for attempt in swipe_attempts
                for item in attempt["commands"]
            ),
            "one or more swipe events were rejected",
        )
        if model_image is None or model_record is None or page_difference is None or title_golden is None:
            raise NativeE2EFailure("no heart-page swipe attempt was captured")
        journey["screenshots"]["heart_model"] = model_record
        require_check(
            checks,
            "heart_model_png",
            model_record["console_ok"] and model_record["nonblank"],
            "heart-model screenshot failed validation",
        )
        journey["pixel_transitions"]["watch_face_to_heart_model"] = page_difference
        require_check(
            checks,
            "page_transition_pixels",
            page_difference["changed_pixels"] >= MIN_CHANGED_PIXELS,
            "watch-face to heart-page target region did not change",
        )
        journey["golden_regions"]["heart_page_title"] = title_golden
        require_check(
            checks,
            "heart_page_title_golden",
            title_golden["matches"],
            "heart page title did not match the reviewed Heart Rate golden region",
        )
        checkpoint("sensor-injection")

        sensor_status = console.command("sensor status", "console-sensor-status.txt")
        sensor_enabled = (
            "heart-rate: enabled." in sensor_status
            and console_response_ok(sensor_status)
        )
        journey["console"]["sensor_status"] = sensor_status.strip()
        require_check(
            checks,
            "sensor_enabled",
            sensor_enabled,
            "emulator heart-rate sensor is not enabled",
        )

        sensor_sets = []
        for index in range(1, args.sensor_samples + 1):
            response = console.command(
                "sensor set heart-rate 104",
                f"console-sensor-set-{index:02d}.txt",
            )
            sensor_sets.append(
                {"sample": index, "response": response.strip(), "ok": console_response_ok(response)}
            )
            time.sleep(args.sensor_sample_delay)
        journey["console"]["sensor_sets"] = sensor_sets
        require_check(
            checks,
            "sensor_set_console_ok",
            all(item["ok"] for item in sensor_sets),
            "one or more heart-rate sensor samples were rejected",
        )

        sensor_get = console.command(
            "sensor get heart-rate", "console-sensor-get.txt"
        )
        sensor_get_ok = (
            "heart-rate = 104" in sensor_get and console_response_ok(sensor_get)
        )
        journey["console"]["sensor_get"] = sensor_get.strip()
        require_check(
            checks,
            "sensor_get_104",
            sensor_get_ok,
            "emulator did not report heart-rate = 104",
        )
        child.pump(args.sensor_settle_seconds)

        sensor_image, sensor_record = capture_screenshot(
            console, evidence_dir, "heart-sensor"
        )
        journey["screenshots"]["heart_sensor"] = sensor_record
        require_check(
            checks,
            "heart_sensor_png",
            sensor_record["console_ok"] and sensor_record["nonblank"],
            "heart-sensor screenshot failed validation",
        )
        heart_value_difference = region_difference(
            model_image, sensor_image, HEART_VALUE_REGION
        )
        source_label_difference = region_difference(
            model_image, sensor_image, SOURCE_LABEL_REGION
        )
        journey["pixel_transitions"]["heart_model_to_sensor"] = {
            "heart_value": heart_value_difference,
            "source_label": source_label_difference,
        }
        require_check(
            checks,
            "heart_value_transition_pixels",
            heart_value_difference["changed_pixels"] >= MIN_CHANGED_PIXELS,
            "heart-rate value region did not change after sensor injection",
        )
        require_check(
            checks,
            "source_label_transition_pixels",
            source_label_difference["changed_pixels"] >= MIN_CHANGED_PIXELS,
            "heart-rate source label did not change after sensor injection",
        )
        value_golden = golden_region_record(sensor_image, "heart_value_104_bpm")
        source_golden = golden_region_record(sensor_image, "source_sensor")
        journey["golden_regions"].update(
            {
                "heart_value_104_bpm": value_golden,
                "source_sensor": source_golden,
            }
        )
        require_check(
            checks,
            "heart_value_104_bpm_golden",
            value_golden["matches"],
            "heart value did not match the reviewed 104 bpm golden region",
        )
        require_check(
            checks,
            "source_sensor_golden",
            source_golden["matches"],
            "source did not match the reviewed Source / Sensor golden region",
        )
        rendered_state_ok = bool(
            title_golden["matches"]
            and value_golden["matches"]
            and source_golden["matches"]
            and sensor_enabled
            and sensor_get_ok
            and uorb_ok
        )
        journey["rendered_metric_state"] = {
            "page": "heart_rate",
            "value_bpm": 104,
            "source": "sensor",
            "freshness": "fresh",
            "passed": rendered_state_ok,
            "derivation": {
                "page": "exact Heart Rate golden ROI",
                "value": "exact 104 bpm golden ROI plus console get",
                "source_freshness": (
                    "exact Sensor golden ROI; production mapping renders "
                    "Sensor stale when freshness is stale"
                ),
                "transport": "enabled emulator sensor plus NSH uORB node",
            },
        }
        require_check(
            checks,
            "heart_metric_structured_state",
            rendered_state_ok,
            "rendered heart metric state was not page=heart_rate value=104 source=sensor freshness=fresh",
        )
        checkpoint("runtime-assertions")

        pid_output = child.send_command(
            "pidof smart_band",
            prompt,
            args.command_timeout,
            evidence_dir / "nsh-pidof-smart-band.txt",
        )
        pids = SMOKE.extract_pidof(pid_output)
        journey["nsh"]["smart_band_pids"] = pids
        require_check(checks, "pidof", bool(pids), "pidof did not find smart_band")

        ps_output = child.send_command(
            "ps", prompt, args.command_timeout, evidence_dir / "nsh-ps.txt"
        )
        ps_ok = re.search(
            r"(?<![A-Za-z0-9_])smart_band(?![A-Za-z0-9_])", ps_output
        ) is not None
        journey["nsh"]["ps_contains_smart_band"] = ps_ok
        require_check(checks, "ps", ps_ok, "NSH ps did not contain smart_band")

        app_output = bytes(child.transcript[app_transcript_start:]).decode(
            "utf-8", errors="replace"
        )
        fatal_marker = SMOKE.find_app_failure(app_output)
        app_ok = fatal_marker is None and child.poll() is None
        journey["nsh"]["fatal_marker"] = fatal_marker
        require_check(
            checks,
            "app_no_fatal",
            app_ok,
            f"native app emitted a fatal marker or exited: {fatal_marker}",
        )
        checkpoint("cleanup")
    except Exception as exc:
        failure = {
            "type": type(exc).__name__,
            "message": str(exc),
            "traceback": "failure.txt",
        }
        write_text(evidence_dir / "failure.txt", traceback.format_exc())
    finally:
        journey["phase"] = "cleanup"
        if console is not None:
            try:
                response = console.command(
                    "kill", "console-kill.txt", allow_eof=True
                )
                journey["cleanup"]["console_kill_response"] = response.strip()
                cleanup_log.append("requested emulator shutdown through console")
            except Exception as exc:
                cleanup_log.append(f"console shutdown failed: {exc}")
            console.close()
        if child is not None:
            try:
                child.stop_process_group(cleanup_log)
            except Exception as exc:
                cleanup_log.append(f"process-group cleanup failed: {exc}")
            try:
                child.close()
            except Exception as exc:
                cleanup_log.append(f"PTY close failed: {exc}")
        write_text(evidence_dir / "cleanup.txt", "\n".join(cleanup_log) + "\n")

        try:
            attributed_cleanup = cleanup_attributed_processes(
                run_id,
                runtime_output,
                attributed_executables,
                attributed_baseline,
            )
        except Exception as exc:
            attributed_cleanup = {
                "run_id": run_id,
                "supported": os.name == "posix",
                "baseline": attributed_baseline,
                "initial": [],
                "actions": [],
                "remaining": [],
                "absent": False,
                "error": str(exc),
            }
        process_clean = not process_group_exists(emulator_pgid)
        port_clean = wait_for_port_available(args.console_port)
        checks["process_cleanup"] = process_clean
        checks["attributed_process_cleanup"] = attributed_cleanup["absent"]
        checks["port_cleanup"] = port_clean
        journey["cleanup"].update(
            {
                "process_group_absent": process_clean,
                "attributed_processes": attributed_cleanup,
                "console_port_available": port_clean,
                "log": "cleanup.txt",
            }
        )
        if runtime is not None:
            try:
                checks["fixed_output_unchanged"] = verify_fixed_output_unchanged(runtime)
            except Exception as exc:
                checks["fixed_output_unchanged"] = False
                cleanup_log.append(f"fixed-output verification failed: {exc}")
                write_text(evidence_dir / "cleanup.txt", "\n".join(cleanup_log) + "\n")
        else:
            checks["fixed_output_unchanged"] = False

        if runtime is not None:
            try:
                runtime_cleanup = cleanup_staged_runtime_inputs(runtime_output)
                checks["runtime_input_cleanup"] = runtime_cleanup["passed"]
                journey["cleanup"]["runtime_inputs"] = runtime_cleanup
            except Exception as exc:
                checks["runtime_input_cleanup"] = False
                cleanup_log.append(f"runtime-input cleanup failed: {exc}")
                write_text(evidence_dir / "cleanup.txt", "\n".join(cleanup_log) + "\n")
        else:
            checks["runtime_input_cleanup"] = False

        missing_checks = [
            name for name in REQUIRED_CHECKS if checks.get(name) is not True
        ]
        if failure is None and missing_checks:
            failure = {
                "type": "NativeE2EFailure",
                "message": "required checks failed: " + ", ".join(missing_checks),
            }
        journey["failure"] = failure
        journey["status"] = (
            "passed" if failure is None and all_required_checks_pass(checks) else "failed"
        )
        journey["phase"] = "complete"
        journey["completed_at_utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
        journey["evidence_manifest"] = "evidence.sha256"
        write_json(evidence_dir / "journey.json", journey)
        write_evidence_manifest(evidence_dir)
        for handled_signal, previous_handler in previous_handlers.items():
            signal.signal(handled_signal, previous_handler)

    print(json.dumps({"status": journey["status"], "evidence_dir": str(evidence_dir)}, sort_keys=True))
    return 0 if journey["status"] == "passed" else 1


def main() -> int:
    return run_journey(parse_args())


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except NativeE2EFailure as exc:
        print(f"native E2E failed before launch: {exc}", file=sys.stderr)
        raise SystemExit(1)
