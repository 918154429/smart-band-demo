#!/usr/bin/env python3
"""Collect the reproducible Q0 native startup and resource baseline."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
import platform
import re
import secrets
import signal
import shutil
import socket
import subprocess
import sys
import time
import traceback
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_DIR = "cmake_out/vela_goldfish-arm64-v8a-ap"
Q0_REQUIRED_RUNS = 20
Q0_MAX_APP_START_P95_SECONDS = 2.0
RUNTIME_INPUTS = (".config", "nuttx", "vela_system.bin", "vela_data.bin")
RUN_ID_ENV = "SMART_BAND_Q0_RUN_ID"
REQUIRED_GATE_RECEIPT_CHECKS = (
    "host",
    "browser",
    "shell",
    "fixed_build_native_smoke",
    "source_tree_equivalent",
    "working_tree_attributed",
)
REQUIRED_SYMBOL_ANCHORS = ("smart_band_main", "smart_band_icon_heart_map")
FULL_GIT_SHA = re.compile(r"^[0-9a-f]{40}$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
OPENVELA_REVISION_KEYS = ("manifest", "claude", "emulator", "emulator_tools")
LIVE_OPENVELA_REVISION_KEYS = ("emulator", "emulator_tools")
SMART_BAND_SYMBOL = re.compile(
    r"(?:smart_band|watch_model|watch_|sensor_|calculator_|game_2048|"
    r"mines_|timer_|stopwatch_|tetris_|weather_|wooden_fish_)"
)


class BaselineFailure(RuntimeError):
    """Raised when the baseline cannot be collected reliably."""


class CollectionInterrupted(BaselineFailure):
    """Raised after an external signal requests an evidence-preserving stop."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect 20 cold native starts plus binary/resource evidence."
    )
    parser.add_argument("--openvela-root", required=True, type=Path)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--source-repo", type=Path, default=ROOT)
    parser.add_argument(
        "--smoke-script",
        type=Path,
        default=ROOT / "scripts" / "smoke_openvela_emulator.py",
    )
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--runs", type=int, default=20)
    parser.add_argument("--minimum-gate-runs", type=int, default=20)
    parser.add_argument("--console-port", type=int, default=5554)
    parser.add_argument("--boot-timeout", type=float, default=240.0)
    parser.add_argument("--command-timeout", type=float, default=30.0)
    parser.add_argument("--settle-seconds", type=float, default=0.1)
    parser.add_argument("--stability-seconds", type=float, default=0.1)
    parser.add_argument("--run-timeout", type=float, default=360.0)
    parser.add_argument("--app-start-p95-budget", type=float, default=2.0)
    parser.add_argument("--baseline-revision", required=True)
    parser.add_argument("--artifact-source-revision", required=True)
    parser.add_argument("--expected-nuttx-sha256", required=True)
    parser.add_argument("--gate-receipt", required=True, type=Path)
    return parser.parse_args()


def write_text(path: Path, value: str) -> None:
    path.write_text(value, encoding="utf-8", errors="replace")


def write_json(path: Path, value: Any) -> None:
    write_text(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


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


def timeout_output(value: str | bytes | None) -> str:
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def finite_number(value: Any, *, minimum: float | None = None) -> bool:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    number = float(value)
    if not math.isfinite(number):
        return False
    return minimum is None or number >= minimum


def validate_gate_arguments(args: argparse.Namespace) -> None:
    if isinstance(args.runs, bool) or args.runs < Q0_REQUIRED_RUNS:
        raise BaselineFailure(f"formal Q0 requires at least {Q0_REQUIRED_RUNS} runs")
    if (
        isinstance(args.minimum_gate_runs, bool)
        or args.minimum_gate_runs < Q0_REQUIRED_RUNS
        or args.minimum_gate_runs > args.runs
    ):
        raise BaselineFailure(
            f"minimum gate runs must be between {Q0_REQUIRED_RUNS} and --runs"
        )
    if not finite_number(args.app_start_p95_budget, minimum=0.0) or not (
        0.0 < float(args.app_start_p95_budget) <= Q0_MAX_APP_START_P95_SECONDS
    ):
        raise BaselineFailure(
            f"app start p95 budget must be in (0, {Q0_MAX_APP_START_P95_SECONDS}]"
        )
    for name in ("boot_timeout", "command_timeout", "run_timeout"):
        if not finite_number(getattr(args, name), minimum=0.0) or getattr(args, name) <= 0:
            raise BaselineFailure(f"{name.replace('_', '-')} must be finite and positive")
    for name in ("settle_seconds", "stability_seconds"):
        if not finite_number(getattr(args, name), minimum=0.0):
            raise BaselineFailure(f"{name.replace('_', '-')} must be finite and non-negative")
    if not isinstance(args.console_port, int) or isinstance(args.console_port, bool):
        raise BaselineFailure("console port must be an integer")
    if not 1 <= args.console_port <= 65535:
        raise BaselineFailure("console port must be between 1 and 65535")
    if not FULL_GIT_SHA.fullmatch(args.baseline_revision):
        raise BaselineFailure("baseline revision must be a full lowercase Git SHA")
    if not FULL_GIT_SHA.fullmatch(args.artifact_source_revision):
        raise BaselineFailure("artifact source revision must be a full lowercase Git SHA")
    if not SHA256.fullmatch(args.expected_nuttx_sha256):
        raise BaselineFailure("expected NuttX SHA-256 must be 64 lowercase hex characters")


def percentile_nearest_rank(values: list[float], percentile: float) -> float:
    if not values:
        raise ValueError("at least one value is required")
    if not 0.0 < percentile <= 1.0:
        raise ValueError("percentile must be in (0, 1]")
    ordered = sorted(values)
    rank = max(1, math.ceil(percentile * len(ordered)))
    return ordered[rank - 1]


def metric_summary(values: list[float]) -> dict[str, float | int]:
    if not values:
        return {"count": 0}
    ordered = sorted(values)
    midpoint = len(ordered) // 2
    if len(ordered) % 2:
        median = ordered[midpoint]
    else:
        median = (ordered[midpoint - 1] + ordered[midpoint]) / 2.0
    return {
        "count": len(ordered),
        "min_seconds": round(ordered[0], 6),
        "p50_seconds": round(median, 6),
        "p95_seconds": round(percentile_nearest_rank(ordered, 0.95), 6),
        "max_seconds": round(ordered[-1], 6),
    }


def run_to_file(
    command: list[str], destination: Path, cwd: Path | None = None
) -> dict[str, Any]:
    executable = shutil.which(command[0])
    if executable is None:
        write_text(destination, f"tool unavailable: {command[0]}\n")
        return {"available": False, "returncode": None, "command": command}
    with destination.open("w", encoding="utf-8", errors="replace") as stream:
        result = subprocess.run(
            [executable, *command[1:]],
            cwd=cwd,
            check=False,
            stdout=stream,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    return {
        "available": True,
        "returncode": result.returncode,
        "command": command,
    }


def git_text(repo: Path, *arguments: str) -> str | None:
    if not (repo / ".git").exists() or shutil.which("git") is None:
        return None
    result = subprocess.run(
        ["git", "-C", str(repo), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def resolve_output_dir(openvela_root: Path, value: str) -> Path:
    path = Path(value)
    return path.resolve() if path.is_absolute() else (openvela_root / path).resolve()


def ensure_new_evidence_dir(evidence_dir: Path) -> None:
    if evidence_dir.exists() and any(evidence_dir.iterdir()):
        raise BaselineFailure(f"evidence directory is not empty: {evidence_dir}")
    evidence_dir.mkdir(parents=True, exist_ok=True)


def paths_overlap(first: Path, second: Path) -> bool:
    first = first.resolve()
    second = second.resolve()
    return first == second or first in second.parents or second in first.parents


def require_disjoint_evidence_path(evidence_dir: Path, *protected_paths: Path) -> None:
    for protected in protected_paths:
        if paths_overlap(evidence_dir, protected):
            raise BaselineFailure(
                "evidence directory must not overlap protected input path: "
                f"{evidence_dir} <-> {protected}"
            )


def artifact_record(path: Path) -> dict[str, Any]:
    if not path.is_file() or path.stat().st_size == 0:
        raise BaselineFailure(f"required artifact is missing or empty: {path}")
    return {
        "path": str(path),
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def collect_tree_manifest(source_repo: Path, destination: Path) -> dict[str, Any]:
    tree_root = source_repo / "openvela_app" / "smart_band"
    if not tree_root.is_dir():
        raise BaselineFailure(f"smart-band source tree is missing: {tree_root}")
    files = []
    digest = hashlib.sha256()
    paths = [item for item in tree_root.rglob("*") if item.is_file()]
    for path in sorted(paths, key=lambda item: item.relative_to(tree_root).as_posix()):
        relative = path.relative_to(tree_root).as_posix()
        file_hash = sha256_file(path)
        line = f"{file_hash}  {relative}\n"
        digest.update(line.encode("utf-8"))
        files.append({"path": relative, "bytes": path.stat().st_size, "sha256": file_hash})
    if not files:
        raise BaselineFailure(f"smart-band source tree contains no files: {tree_root}")
    result = {
        "root": str(tree_root),
        "file_count": len(files),
        "total_bytes": sum(item["bytes"] for item in files),
        "sha256": digest.hexdigest(),
        "files": files,
    }
    write_json(destination, result)
    return result


def load_gate_receipt(
    source: Path,
    destination: Path,
    baseline_revision: str,
    artifact_source_revision: str,
    expected_nuttx_sha256: str,
    collector_sha256: str,
    smoke_sha256: str,
) -> dict[str, Any]:
    if not source.is_file():
        raise BaselineFailure(f"gate receipt is missing: {source}")
    try:
        receipt = json.loads(source.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise BaselineFailure(f"gate receipt is invalid: {source}: {exc}") from exc
    if not isinstance(receipt, dict) or receipt.get("schema_version") != 1:
        raise BaselineFailure("gate receipt must use schema_version 1")
    expected_fields = {
        "baseline_revision": baseline_revision,
        "artifact_source_revision": artifact_source_revision,
        "nuttx_sha256": expected_nuttx_sha256,
        "collector_sha256": collector_sha256,
        "smoke_sha256": smoke_sha256,
    }
    for name, expected in expected_fields.items():
        if receipt.get(name) != expected:
            raise BaselineFailure(
                f"gate receipt {name} does not match the requested baseline"
            )
    tree_hash = receipt.get("smart_band_tree_sha256")
    if not isinstance(tree_hash, str) or not SHA256.fullmatch(tree_hash):
        raise BaselineFailure("gate receipt is missing smart_band_tree_sha256")
    baseline_tree = receipt.get("baseline_smart_band_git_tree")
    artifact_tree = receipt.get("artifact_smart_band_git_tree")
    if (
        not isinstance(baseline_tree, str)
        or not FULL_GIT_SHA.fullmatch(baseline_tree)
        or baseline_tree != artifact_tree
    ):
        raise BaselineFailure("gate receipt does not prove baseline/artifact tree equivalence")
    revisions = receipt.get("openvela_revisions")
    if not isinstance(revisions, dict) or any(
        not isinstance(revisions.get(name), str)
        or not FULL_GIT_SHA.fullmatch(revisions[name])
        for name in OPENVELA_REVISION_KEYS
    ):
        raise BaselineFailure("gate receipt is missing fixed openvela revisions")
    fixed_build = receipt.get("fixed_build")
    if (
        not isinstance(fixed_build, dict)
        or fixed_build.get("conclusion") != "success"
        or not isinstance(fixed_build.get("run_id"), int)
        or isinstance(fixed_build.get("run_id"), bool)
        or fixed_build["run_id"] <= 0
        or not isinstance(fixed_build.get("artifact_id"), int)
        or isinstance(fixed_build.get("artifact_id"), bool)
        or fixed_build["artifact_id"] <= 0
        or fixed_build.get("head_sha") != artifact_source_revision
        or not isinstance(fixed_build.get("artifact_digest"), str)
        or not re.fullmatch(r"sha256:[0-9a-f]{64}", fixed_build["artifact_digest"])
    ):
        raise BaselineFailure("gate receipt is missing successful fixed-build provenance")
    fixed_input_hashes = receipt.get("fixed_input_sha256")
    if (
        not isinstance(fixed_input_hashes, dict)
        or set(fixed_input_hashes) != set(RUNTIME_INPUTS)
        or any(
            not isinstance(fixed_input_hashes[name], str)
            or not SHA256.fullmatch(fixed_input_hashes[name])
            for name in RUNTIME_INPUTS
        )
        or fixed_input_hashes["nuttx"] != expected_nuttx_sha256
    ):
        raise BaselineFailure("gate receipt is missing the four fixed input hashes")
    evidence = receipt.get("evidence")
    if not isinstance(evidence, dict):
        raise BaselineFailure("gate receipt is missing validation evidence")
    for name in ("host_actions", "browser_actions"):
        action = evidence.get(name)
        if (
            not isinstance(action, dict)
            or action.get("conclusion") != "success"
            or action.get("head_sha") != baseline_revision
            or not isinstance(action.get("run_id"), int)
            or isinstance(action.get("run_id"), bool)
            or action["run_id"] <= 0
        ):
            raise BaselineFailure(
                f"gate receipt {name} is not a successful baseline-revision run"
            )
    checks = receipt.get("checks")
    if not isinstance(checks, dict):
        raise BaselineFailure("gate receipt checks must be an object")
    failed = [name for name in REQUIRED_GATE_RECEIPT_CHECKS if checks.get(name) is not True]
    if failed:
        raise BaselineFailure("gate receipt required checks failed: " + ", ".join(failed))
    shutil.copy2(source, destination)
    return {
        "path": str(destination),
        "sha256": sha256_file(destination),
        "smart_band_tree_sha256": tree_hash,
        "openvela_revisions": revisions,
        "fixed_input_sha256": fixed_input_hashes,
        "required_checks": {name: checks[name] for name in REQUIRED_GATE_RECEIPT_CHECKS},
        "receipt": receipt,
    }


def stage_runtime_output(source_output: Path, runtime_output: Path) -> dict[str, Any]:
    if runtime_output.exists():
        raise BaselineFailure(f"runtime output already exists: {runtime_output}")
    runtime_output.mkdir()
    records: dict[str, Any] = {}
    try:
        for name in RUNTIME_INPUTS:
            source = source_output / name
            destination = runtime_output / name
            before = artifact_record(source)
            shutil.copy2(source, destination)
            staged = artifact_record(destination)
            isolated = not os.path.samefile(source, destination)
            identical = before["bytes"] == staged["bytes"] and before["sha256"] == staged["sha256"]
            if not isolated or not identical:
                raise BaselineFailure(
                    f"runtime input is not an isolated identical copy: {destination}"
                )
            records[name] = {
                "source_before": before,
                "staged": staged,
                "method": "copy2",
                "isolated": isolated,
                "initial_hash_matches": identical,
            }
    except Exception:
        try:
            shutil.rmtree(runtime_output)
        except OSError as cleanup_error:
            raise BaselineFailure(
                f"runtime staging rollback failed for {runtime_output}: {cleanup_error}"
            ) from cleanup_error
        raise
    return {"runtime_output": str(runtime_output), "files": records}


def verify_staged_source_unchanged(staging: dict[str, Any]) -> bool:
    unchanged = True
    for record in staging["files"].values():
        before = record["source_before"]
        after = artifact_record(Path(before["path"]))
        record["source_after"] = after
        record["source_unchanged"] = all(
            before[key] == after[key] for key in ("bytes", "sha256")
        )
        unchanged = unchanged and record["source_unchanged"]
    return unchanged


def cleanup_runtime_inputs(runtime_output: Path) -> dict[str, Any]:
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
            raise BaselineFailure(f"runtime cleanup escaped staging directory: {path}")
        if path.exists():
            try:
                path.unlink()
                removed.append(name)
            except OSError as exc:
                errors.append(f"cannot remove {name}: {exc}")
        else:
            missing.append(name)
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


def collect_static_assets(source_repo: Path, destination: Path) -> dict[str, Any]:
    smart_band_root = source_repo / "openvela_app" / "smart_band"
    candidates: list[Path] = []
    assets_root = smart_band_root / "assets"
    if assets_root.is_dir():
        candidates.extend(path for path in assets_root.rglob("*") if path.is_file())
    generated_source = smart_band_root / "icon_assets.c"
    if generated_source.is_file():
        candidates.append(generated_source)

    files = []
    for path in sorted(set(candidates)):
        files.append(
            {
                "path": path.relative_to(source_repo).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": sha256_file(path),
            }
        )
    result = {
        "file_count": len(files),
        "total_source_bytes": sum(item["bytes"] for item in files),
        "files": files,
    }
    write_json(destination, result)
    return result


def collect_symbol_subset(symbol_map: Path, destination: Path) -> dict[str, Any]:
    matched_lines: list[str] = []
    matched_names: set[str] = set()
    symbol_bytes = 0
    with symbol_map.open("r", encoding="utf-8", errors="replace") as stream:
        for line in stream:
            if not SMART_BAND_SYMBOL.search(line):
                continue
            matched_lines.append(line.rstrip("\n"))
            fields = line.split()
            if fields:
                matched_names.add(fields[-1])
            if len(fields) >= 4 and fields[1].isdigit():
                symbol_bytes += int(fields[1])
    write_text(destination, "\n".join(matched_lines) + ("\n" if matched_lines else ""))
    anchors = {name: name in matched_names for name in REQUIRED_SYMBOL_ANCHORS}
    return {
        "matched_symbols": len(matched_lines),
        "summed_symbol_bytes": symbol_bytes,
        "required_anchors": anchors,
        "anchors_present": all(anchors.values()),
    }


def collect_resource_snapshot(
    openvela_root: Path,
    output_dir: Path,
    source_repo: Path,
    evidence_dir: Path,
) -> dict[str, Any]:
    artifacts = {
        name: artifact_record(output_dir / name)
        for name in ("nuttx", ".config", "vela_system.bin", "vela_data.bin")
    }
    tool_results = {
        "file": run_to_file(["file", str(output_dir / "nuttx")], evidence_dir / "nuttx-file.txt"),
        "size": run_to_file(
            ["size", "-A", "-d", str(output_dir / "nuttx")],
            evidence_dir / "binary-size.txt",
        ),
        "readelf_header": run_to_file(
            ["readelf", "-h", str(output_dir / "nuttx")],
            evidence_dir / "elf-header.txt",
        ),
        "readelf_sections": run_to_file(
            ["readelf", "-SW", str(output_dir / "nuttx")],
            evidence_dir / "elf-sections.txt",
        ),
        "nm": run_to_file(
            ["nm", "-a", "-S", "--size-sort", "--radix=d", str(output_dir / "nuttx")],
            evidence_dir / "nuttx-symbol-map.txt",
        ),
    }
    required_tools_ok = all(
        tool_results[name]["available"] and tool_results[name]["returncode"] == 0
        for name in ("size", "readelf_header", "readelf_sections", "nm")
    )

    symbol_subset = {
        "matched_symbols": 0,
        "summed_symbol_bytes": 0,
        "required_anchors": {name: False for name in REQUIRED_SYMBOL_ANCHORS},
        "anchors_present": False,
    }
    if tool_results["nm"]["returncode"] == 0:
        symbol_subset = collect_symbol_subset(
            evidence_dir / "nuttx-symbol-map.txt",
            evidence_dir / "smart-band-symbols.txt",
        )

    linker_maps = sorted(
        path for path in output_dir.rglob("*.map") if path.is_file() and path.stat().st_size > 0
    )
    linker_map_record: dict[str, Any] | None = None
    rejected_linker_maps: list[dict[str, Any]] = []
    selected = None
    for candidate in linker_maps:
        has_anchor = False
        with candidate.open("r", encoding="utf-8", errors="replace") as stream:
            for line in stream:
                if "smart_band_main" in line:
                    has_anchor = True
                    break
        if has_anchor:
            selected = candidate
            break
        rejected_linker_maps.append(
            {"path": str(candidate), "bytes": candidate.stat().st_size, "reason": "missing smart_band_main"}
        )
    if selected is not None:
        copied = evidence_dir / "nuttx-linker.map"
        shutil.copy2(selected, copied)
        linker_map_record = {
            "source": str(selected),
            "bytes": copied.stat().st_size,
            "sha256": sha256_file(copied),
            "required_anchor": "smart_band_main",
            "anchor_present": True,
        }

    assets = collect_static_assets(source_repo, evidence_dir / "static-assets.json")
    revisions = {
        "emulator": git_text(
            openvela_root / "prebuilts" / "emulator" / "linux-x86_64",
            "rev-parse",
            "HEAD",
        ),
        "emulator_tools": git_text(
            openvela_root / "prebuilts" / "emulator" / "tools",
            "rev-parse",
            "HEAD",
        ),
        "manifest": git_text(openvela_root / ".repo" / "manifests", "rev-parse", "HEAD"),
        "claude": git_text(openvela_root / ".claude", "rev-parse", "HEAD"),
    }
    result = {
        "artifacts": artifacts,
        "tools": tool_results,
        "required_tools_ok": required_tools_ok,
        "linker_map": linker_map_record,
        "rejected_linker_maps": rejected_linker_maps,
        "map_evidence": "linker-map" if linker_map_record else "elf-symbol-map",
        "smart_band_symbols": symbol_subset,
        "static_assets": assets,
        "revisions": revisions,
    }
    write_json(evidence_dir / "resource-snapshot.json", result)
    return result


def resource_gate_passes(resources: dict[str, Any], expected_nuttx_sha256: str) -> bool:
    symbols = resources["smart_band_symbols"]
    symbol_map_ok = (
        symbols["anchors_present"]
        and symbols["matched_symbols"] > 0
        and symbols["summed_symbol_bytes"] > 0
    )
    linker_map = resources["linker_map"]
    linker_map_ok = bool(
        linker_map
        and linker_map.get("bytes", 0) > 0
        and linker_map.get("anchor_present") is True
    )
    assets = resources["static_assets"]
    return bool(
        resources["required_tools_ok"]
        and resources["artifacts"]["nuttx"]["sha256"] == expected_nuttx_sha256
        and assets["file_count"] > 0
        and assets["total_source_bytes"] > 0
        and symbol_map_ok
        and (linker_map is None or linker_map_ok)
    )


def port_available(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
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


def smoke_cleanup_log_passed(value: str) -> bool:
    clean = value.lower()
    return bool(value.strip()) and "emulator exit status:" in clean and not re.search(
        r"\b(?:failed|survived)\b", clean
    )


def terminate_wrapper_process(
    process: subprocess.Popen[str], actions: list[str]
) -> tuple[str, str]:
    if process.poll() is None:
        try:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGTERM)
                actions.append(f"sent SIGTERM to smoke wrapper process group {process.pid}")
            else:
                process.terminate()
                actions.append(f"terminated smoke wrapper process {process.pid}")
        except ProcessLookupError:
            pass
    try:
        stdout, stderr = process.communicate(timeout=15.0)
    except subprocess.TimeoutExpired:
        if os.name == "posix":
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            actions.append(f"sent SIGKILL to smoke wrapper process group {process.pid}")
        else:
            process.kill()
            actions.append(f"killed smoke wrapper process {process.pid}")
        stdout, stderr = process.communicate()
    return stdout or "", stderr or ""


def run_command_with_timeout(
    command: list[str], timeout: float, environment: dict[str, str] | None = None
) -> tuple[int | None, str, str, bool, list[str]]:
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        start_new_session=os.name == "posix",
        env=environment,
    )
    timed_out = False
    actions: list[str] = []
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        actions.append(f"smoke wrapper timed out after {timeout} seconds")
        stdout, stderr = terminate_wrapper_process(process, actions)
    except BaseException:
        actions.append("collector interrupted while smoke wrapper was active")
        terminate_wrapper_process(process, actions)
        raise
    return process.returncode, stdout or "", stderr or "", timed_out, actions


def processes_referencing_path(
    path: Path, run_id: str | None = None
) -> list[dict[str, Any]]:
    if os.name != "posix" or not Path("/proc").is_dir():
        return []
    target = os.fsencode(str(path.resolve()))
    run_marker = (
        f"{RUN_ID_ENV}={run_id}".encode() if run_id is not None else None
    )
    matches = []
    for process_dir in Path("/proc").iterdir():
        if not process_dir.name.isdigit():
            continue
        pid = int(process_dir.name)
        if pid == os.getpid():
            continue
        try:
            command = (process_dir / "cmdline").read_bytes()
            environment = (process_dir / "environ").read_bytes()
        except OSError:
            continue
        reasons = []
        if target in command:
            reasons.append("runtime_path")
        if run_marker is not None and run_marker in environment.split(b"\0"):
            reasons.append("run_id")
        if not reasons:
            continue
        matches.append(
            {
                "pid": pid,
                "command": command.replace(b"\0", b" ").decode("utf-8", errors="replace").strip(),
                "reasons": reasons,
            }
        )
    return matches


def cleanup_runtime_processes(runtime_output: Path, run_id: str) -> dict[str, Any]:
    result: dict[str, Any] = {
        "path": str(runtime_output),
        "run_id": run_id,
        "supported": os.name == "posix" and Path("/proc").is_dir(),
        "initial": [],
        "actions": [],
        "remaining": [],
        "absent": False,
    }
    if not result["supported"]:
        return result
    result["initial"] = processes_referencing_path(runtime_output, run_id)
    for process in result["initial"]:
        try:
            os.kill(process["pid"], signal.SIGTERM)
            result["actions"].append(f"sent SIGTERM to runtime process {process['pid']}")
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 5.0
    remaining = processes_referencing_path(runtime_output, run_id)
    while remaining and time.monotonic() < deadline:
        time.sleep(0.1)
        remaining = processes_referencing_path(runtime_output, run_id)
    for process in remaining:
        try:
            os.kill(process["pid"], signal.SIGKILL)
            result["actions"].append(f"sent SIGKILL to runtime process {process['pid']}")
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 2.0
    remaining = processes_referencing_path(runtime_output, run_id)
    while remaining and time.monotonic() < deadline:
        time.sleep(0.1)
        remaining = processes_referencing_path(runtime_output, run_id)
    result["remaining"] = remaining
    result["absent"] = not remaining
    return result


def cleanup_recorded_emulator_group(sample_dir: Path, run_id: str) -> dict[str, Any]:
    process_record = sample_dir / "emulator-process.json"
    result: dict[str, Any] = {
        "record": str(process_record),
        "record_present": process_record.is_file(),
        "pgid": None,
        "actions": [],
        "absent": False,
    }
    if not process_record.is_file():
        return result
    try:
        record = json.loads(process_record.read_text(encoding="utf-8"))
        pid = record.get("pid")
        pgid = record.get("pgid")
    except (OSError, json.JSONDecodeError, AttributeError) as exc:
        result["error"] = f"invalid emulator process record: {exc}"
        return result
    if (
        not isinstance(pid, int)
        or isinstance(pid, bool)
        or not isinstance(pgid, int)
        or isinstance(pgid, bool)
        or pid <= 1
        or pgid <= 1
        or pid != pgid
    ):
        result["error"] = "unsafe emulator PID/PGID record"
        return result
    result["pgid"] = pgid
    if os.name != "posix":
        result["error"] = "process-group verification requires POSIX"
        return result

    if process_group_exists(pgid):
        matching_group_processes = []
        for process in processes_referencing_path(sample_dir / "runtime-output", run_id):
            try:
                if os.getpgid(process["pid"]) == pgid:
                    matching_group_processes.append(process)
            except ProcessLookupError:
                continue
        result["matching_group_processes"] = matching_group_processes
        if not matching_group_processes:
            result["error"] = (
                "recorded PGID exists but no process matches this sample run-id or runtime path"
            )
            return result
        os.killpg(pgid, signal.SIGTERM)
        result["actions"].append(f"sent SIGTERM to recorded emulator process group {pgid}")
        deadline = time.monotonic() + 5.0
        while process_group_exists(pgid) and time.monotonic() < deadline:
            time.sleep(0.1)
    if process_group_exists(pgid):
        os.killpg(pgid, signal.SIGKILL)
        result["actions"].append(f"sent SIGKILL to recorded emulator process group {pgid}")
        deadline = time.monotonic() + 2.0
        while process_group_exists(pgid) and time.monotonic() < deadline:
            time.sleep(0.1)
    result["absent"] = not process_group_exists(pgid)
    return result


def run_smoke_sample(
    args: argparse.Namespace,
    openvela_root: Path,
    source_output: Path,
    smoke_script: Path,
    sample_dir: Path,
    index: int,
) -> dict[str, Any]:
    sample_dir.mkdir(parents=True, exist_ok=False)
    runtime_output = sample_dir / "runtime-output"
    run_id = secrets.token_hex(16)
    staging = stage_runtime_output(source_output, runtime_output)
    staging["run_id"] = run_id
    write_json(sample_dir / "runtime-staging.json", staging)
    command = [
        sys.executable,
        str(smoke_script),
        "--openvela-root",
        str(openvela_root),
        "--output-dir",
        str(runtime_output),
        "--evidence-dir",
        str(sample_dir),
        "--console-port",
        str(args.console_port),
        "--boot-timeout",
        str(args.boot_timeout),
        "--command-timeout",
        str(args.command_timeout),
        "--settle-seconds",
        str(args.settle_seconds),
        "--stability-seconds",
        str(args.stability_seconds),
    ]
    started_at = time.monotonic()
    process_error: str | None = None
    caught_exception: Exception | None = None
    environment = os.environ.copy()
    environment[RUN_ID_ENV] = run_id
    try:
        returncode, stdout, stderr, timed_out, timeout_actions = run_command_with_timeout(
            command, args.run_timeout, environment
        )
    except Exception as exc:
        caught_exception = exc
        returncode = None
        stdout = ""
        stderr = ""
        timed_out = False
        timeout_actions = []
        process_error = f"cannot run smoke wrapper: {exc}"
    elapsed = time.monotonic() - started_at
    write_text(sample_dir / "collector-stdout.txt", stdout)
    write_text(sample_dir / "collector-stderr.txt", stderr)
    write_text(sample_dir / "collector-command.txt", " ".join(command) + "\n")

    runtime_path = sample_dir / "runtime-smoke.json"
    runtime: dict[str, Any] | None = None
    error: str | None = None
    if runtime_path.is_file():
        try:
            runtime = json.loads(runtime_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError) as exc:
            error = f"invalid runtime-smoke.json: {exc}"
    elif returncode == 0:
        error = "runtime-smoke.json was not produced"

    try:
        group_cleanup = cleanup_recorded_emulator_group(sample_dir, run_id)
    except Exception as exc:
        group_cleanup = {
            "record": str(sample_dir / "emulator-process.json"),
            "record_present": (sample_dir / "emulator-process.json").is_file(),
            "pgid": None,
            "actions": [],
            "absent": False,
            "error": str(exc),
        }
    try:
        runtime_process_cleanup = cleanup_runtime_processes(runtime_output, run_id)
    except Exception as exc:
        runtime_process_cleanup = {
            "path": str(runtime_output),
            "run_id": run_id,
            "supported": os.name == "posix",
            "initial": [],
            "actions": [],
            "remaining": [],
            "absent": False,
            "error": str(exc),
        }
    cleanup_path = sample_dir / "emulator-cleanup.txt"
    cleanup_text = (
        cleanup_path.read_text(encoding="utf-8", errors="replace")
        if cleanup_path.is_file()
        else ""
    )
    smoke_cleanup_ok = smoke_cleanup_log_passed(cleanup_text)
    try:
        port_released = wait_for_port_available(args.console_port)
    except Exception:
        port_released = False
    try:
        fixed_output_unchanged = verify_staged_source_unchanged(staging)
    except Exception as exc:
        fixed_output_unchanged = False
        staging["source_verification_error"] = str(exc)
    try:
        runtime_cleanup = cleanup_runtime_inputs(runtime_output)
    except Exception as exc:
        fallback_errors = [str(exc)]
        try:
            if runtime_output.is_symlink():
                runtime_output.unlink()
            elif runtime_output.exists():
                shutil.rmtree(runtime_output)
        except OSError as fallback_exc:
            fallback_errors.append(f"fallback cleanup failed: {fallback_exc}")
        runtime_cleanup = {
            "removed": [],
            "already_missing": [],
            "inventory": [],
            "errors": fallback_errors,
            "tree_removed": not runtime_output.exists(),
            "passed": False,
        }
    staging["fixed_output_unchanged"] = fixed_output_unchanged
    staging["cleanup"] = runtime_cleanup
    write_json(sample_dir / "runtime-staging.json", staging)
    cleanup = {
        "timeout_actions": timeout_actions,
        "smoke_cleanup_log_passed": smoke_cleanup_ok,
        "emulator_process_group": group_cleanup,
        "runtime_processes": runtime_process_cleanup,
        "console_port_released": port_released,
        "runtime_inputs": runtime_cleanup,
    }
    write_json(sample_dir / "collector-cleanup.json", cleanup)
    cleanup_ok = bool(
        smoke_cleanup_ok
        and group_cleanup["record_present"]
        and group_cleanup["absent"]
        and runtime_process_cleanup["absent"]
        and port_released
        and runtime_cleanup["passed"]
    )
    app_seconds = runtime.get("app_ui_ready_seconds") if runtime else None
    boot_seconds = runtime.get("boot_seconds") if runtime else None
    passed = (
        not timed_out
        and returncode == 0
        and runtime is not None
        and runtime.get("status") == "passed"
        and finite_number(app_seconds, minimum=0.0)
        and float(app_seconds) > 0.0
        and finite_number(boot_seconds, minimum=0.0)
        and float(boot_seconds) > 0.0
        and cleanup_ok
        and fixed_output_unchanged
        and process_error is None
    )
    if not passed and error is None:
        error = "smoke, timing, or cleanup contract failed"
    sample = {
        "run": index,
        "run_id": run_id,
        "status": "passed" if passed else "failed",
        "returncode": returncode,
        "timed_out": timed_out,
        "elapsed_seconds": round(elapsed, 6),
        "boot_seconds": boot_seconds,
        "app_ui_ready_seconds": app_seconds,
        "cleanup_ok": cleanup_ok,
        "smoke_cleanup_log_passed": smoke_cleanup_ok,
        "emulator_process_group_absent": group_cleanup["absent"],
        "runtime_processes_absent": runtime_process_cleanup["absent"],
        "console_port_released": port_released,
        "runtime_inputs_isolated": all(
            record["isolated"] and record["initial_hash_matches"]
            for record in staging["files"].values()
        ),
        "runtime_inputs_cleaned": runtime_cleanup["passed"],
        "fixed_output_unchanged": fixed_output_unchanged,
        "process_error": process_error,
        "error": error,
        "evidence_dir": str(sample_dir),
    }
    write_json(sample_dir / "collector-sample.json", sample)
    if isinstance(caught_exception, CollectionInterrupted):
        raise caught_exception
    return sample


def build_startup_summary(
    samples: list[dict[str, Any]],
    minimum_gate_runs: int,
    app_start_p95_budget: float,
) -> dict[str, Any]:
    if (
        isinstance(minimum_gate_runs, bool)
        or minimum_gate_runs < Q0_REQUIRED_RUNS
    ):
        raise BaselineFailure(
            f"formal startup summary requires at least {Q0_REQUIRED_RUNS} gate runs"
        )
    if not finite_number(app_start_p95_budget, minimum=0.0) or not (
        0.0 < float(app_start_p95_budget) <= Q0_MAX_APP_START_P95_SECONDS
    ):
        raise BaselineFailure("invalid formal app start p95 budget")
    passed = [sample for sample in samples if sample["status"] == "passed"]
    app_values = [
        float(sample["app_ui_ready_seconds"])
        for sample in passed
        if finite_number(sample.get("app_ui_ready_seconds"), minimum=0.0)
        and float(sample["app_ui_ready_seconds"]) > 0.0
    ]
    boot_values = [
        float(sample["boot_seconds"])
        for sample in passed
        if finite_number(sample.get("boot_seconds"), minimum=0.0)
        and float(sample["boot_seconds"]) > 0.0
    ]
    app_metrics = metric_summary(app_values)
    boot_metrics = metric_summary(boot_values)
    gate_passed = (
        len(samples) >= minimum_gate_runs
        and len(passed) == len(samples)
        and len(app_values) == len(samples)
        and len(boot_values) == len(samples)
        and bool(app_values)
        and float(app_metrics["p95_seconds"]) <= app_start_p95_budget
    )
    return {
        "requested_runs": len(samples),
        "minimum_gate_runs": minimum_gate_runs,
        "formal_required_runs": Q0_REQUIRED_RUNS,
        "passed_runs": len(passed),
        "failed_runs": len(samples) - len(passed),
        "percentile_method": "nearest-rank; p50 is the median",
        "app_ui_ready": app_metrics,
        "emulator_to_nsh": boot_metrics,
        "app_start_p95_budget_seconds": app_start_p95_budget,
        "gate_passed": gate_passed,
        "samples": samples,
    }


def collect_host_snapshot(
    evidence_dir: Path, openvela_root: Path, source_repo: Path, suffix: str
) -> dict[str, Any]:
    command_results = {
        "disk": run_to_file(["df", "-h", "/", str(openvela_root)], evidence_dir / f"disk-{suffix}.txt"),
        "usage": run_to_file(
            ["du", "-sh", str(openvela_root), str(source_repo), str(evidence_dir)],
            evidence_dir / f"usage-{suffix}.txt",
        ),
        "processes": run_to_file(
            ["ps", "-eo", "pid=,ppid=,pgid=,args="],
            evidence_dir / f"processes-{suffix}.txt",
        ),
        "ports": run_to_file(["ss", "-ltnp"], evidence_dir / f"ports-{suffix}.txt"),
    }
    return command_results


def render_summary(
    baseline: dict[str, Any], startup: dict[str, Any], resources: dict[str, Any]
) -> str:
    app = startup["app_ui_ready"]
    boot = startup["emulator_to_nsh"]
    lines = [
        "# Q0 native performance and resource baseline",
        "",
        f"- Status: `{baseline['status']}`",
        f"- Baseline revision: `{baseline.get('baseline_revision') or 'not supplied'}`",
        f"- Artifact source revision: `{baseline.get('artifact_source_revision') or 'not supplied'}`",
        f"- Startup runs: `{startup['passed_runs']}/{startup['requested_runs']}` passed",
        (
            "- smart_band -> UI ready: "
            f"p50 `{app.get('p50_seconds', 'n/a')} s`, "
            f"p95 `{app.get('p95_seconds', 'n/a')} s`, "
            f"max `{app.get('max_seconds', 'n/a')} s`"
        ),
        (
            "- Emulator -> NSH: "
            f"p50 `{boot.get('p50_seconds', 'n/a')} s`, "
            f"p95 `{boot.get('p95_seconds', 'n/a')} s`, "
            f"max `{boot.get('max_seconds', 'n/a')} s`"
        ),
        f"- Map evidence: `{resources['map_evidence']}`",
        f"- NuttX ELF bytes: `{resources['artifacts']['nuttx']['bytes']}`",
        f"- Linked smart-band symbols matched: `{resources['smart_band_symbols']['matched_symbols']}`",
        f"- Static asset source bytes: `{resources['static_assets']['total_source_bytes']}`",
        "",
        "The ELF symbol map is retained when the packaged artifact has no linker map. "
        "This is recorded explicitly and is not described as a linker-generated map.",
        "",
    ]
    return "\n".join(lines)


def _collect_baseline(args: argparse.Namespace, evidence_dir: Path) -> int:
    openvela_root = args.openvela_root.resolve()
    source_repo = args.source_repo.resolve()
    smoke_script = args.smoke_script.resolve()
    output_dir = resolve_output_dir(openvela_root, args.output_dir)
    if not smoke_script.is_file():
        raise BaselineFailure(f"smoke script is missing: {smoke_script}")
    collector_sha256 = sha256_file(Path(__file__).resolve())
    smoke_sha256 = sha256_file(smoke_script)

    receipt = load_gate_receipt(
        args.gate_receipt.resolve(),
        evidence_dir / "gate-receipt.json",
        args.baseline_revision,
        args.artifact_source_revision,
        args.expected_nuttx_sha256,
        collector_sha256,
        smoke_sha256,
    )
    source_tree = collect_tree_manifest(source_repo, evidence_dir / "smart-band-tree.json")
    source_tree_matches = source_tree["sha256"] == receipt["smart_band_tree_sha256"]
    if not source_tree_matches:
        raise BaselineFailure(
            "remote smart-band source tree does not match the validated gate receipt"
        )

    inputs = {
        "collected_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "host": {
            "node": platform.node(),
            "platform": platform.platform(),
            "python": platform.python_version(),
            "cpu_count": os.cpu_count(),
        },
        "baseline_revision": args.baseline_revision,
        "artifact_source_revision": args.artifact_source_revision,
        "source_repo": str(source_repo),
        "source_repo_head": git_text(source_repo, "rev-parse", "HEAD"),
        "source_repo_status": git_text(source_repo, "status", "--short", "--branch"),
        "openvela_root": str(openvela_root),
        "output_dir": str(output_dir),
        "collector_script": {
            "path": str(Path(__file__).resolve()),
            "sha256": collector_sha256,
        },
        "smoke_script": {
            "path": str(smoke_script),
            "sha256": smoke_sha256,
        },
        "configuration": {
            "runs": args.runs,
            "minimum_gate_runs": args.minimum_gate_runs,
            "console_port": args.console_port,
            "settle_seconds": args.settle_seconds,
            "stability_seconds": args.stability_seconds,
            "app_start_p95_budget_seconds": args.app_start_p95_budget,
        },
        "expected_nuttx_sha256": args.expected_nuttx_sha256,
        "gate_receipt": {
            "path": "gate-receipt.json",
            "sha256": receipt["sha256"],
            "required_checks": receipt["required_checks"],
        },
        "smart_band_tree": {
            "path": "smart-band-tree.json",
            "sha256": source_tree["sha256"],
            "matches_gate_receipt": source_tree_matches,
        },
    }
    write_json(evidence_dir / "baseline-inputs.json", inputs)
    host_before = collect_host_snapshot(evidence_dir, openvela_root, source_repo, "before")
    resources = collect_resource_snapshot(
        openvela_root, output_dir, source_repo, evidence_dir
    )
    openvela_revisions_match = all(
        resources["revisions"].get(name) == receipt["openvela_revisions"][name]
        for name in LIVE_OPENVELA_REVISION_KEYS
    )
    fixed_inputs_match = all(
        resources["artifacts"][name]["sha256"]
        == receipt["fixed_input_sha256"][name]
        for name in RUNTIME_INPUTS
    )
    if not fixed_inputs_match:
        raise BaselineFailure("fixed runtime inputs do not match the gate receipt")

    samples_root = evidence_dir / "startup-samples"
    samples_root.mkdir()
    samples: list[dict[str, Any]] = []
    for index in range(1, args.runs + 1):
        sample = run_smoke_sample(
            args,
            openvela_root,
            output_dir,
            smoke_script,
            samples_root / f"run-{index:02d}",
            index,
        )
        samples.append(sample)
        print(json.dumps(sample, sort_keys=True), flush=True)
        if not (
            sample["status"] == "passed"
            and sample["fixed_output_unchanged"]
            and sample["emulator_process_group_absent"]
            and sample["runtime_processes_absent"]
            and sample["console_port_released"]
        ):
            break

    startup = build_startup_summary(
        samples, args.minimum_gate_runs, args.app_start_p95_budget
    )
    write_json(evidence_dir / "startup-summary.json", startup)
    host_after = collect_host_snapshot(evidence_dir, openvela_root, source_repo, "after")
    artifacts_after = {
        name: artifact_record(output_dir / name) for name in RUNTIME_INPUTS
    }
    artifact_integrity = {
        "before": resources["artifacts"],
        "after": artifacts_after,
        "files": {},
    }
    for name in RUNTIME_INPUTS:
        before = resources["artifacts"][name]
        after = artifacts_after[name]
        artifact_integrity["files"][name] = {
            "unchanged": before["bytes"] == after["bytes"]
            and before["sha256"] == after["sha256"]
        }
    artifact_integrity["gate_passed"] = all(
        record["unchanged"] for record in artifact_integrity["files"].values()
    )
    write_json(evidence_dir / "fixed-output-integrity.json", artifact_integrity)

    resource_gate = resource_gate_passes(resources, args.expected_nuttx_sha256)
    host_snapshot_gate = all(
        record["available"] and record["returncode"] == 0
        for snapshot in (host_before, host_after)
        for record in snapshot.values()
    )
    required_checks = {
        "gate_receipt": all(receipt["required_checks"].values()),
        "source_tree_matches": source_tree_matches,
        "startup": startup["gate_passed"],
        "resource": resource_gate,
        "fixed_output_integrity": artifact_integrity["gate_passed"],
        "host_snapshots": host_snapshot_gate,
        "provenance": (
            inputs["baseline_revision"] == receipt["receipt"]["baseline_revision"]
            and inputs["artifact_source_revision"]
            == receipt["receipt"]["artifact_source_revision"]
            and resources["artifacts"]["nuttx"]["sha256"]
            == args.expected_nuttx_sha256
            and openvela_revisions_match
            and fixed_inputs_match
        ),
    }
    status = "passed" if all(required_checks.values()) else "failed"
    baseline = {
        "status": status,
        "baseline_revision": args.baseline_revision,
        "artifact_source_revision": args.artifact_source_revision,
        "startup_gate_passed": startup["gate_passed"],
        "resource_gate_passed": resource_gate,
        "fixed_output_integrity_gate_passed": artifact_integrity["gate_passed"],
        "gate_receipt_passed": required_checks["gate_receipt"],
        "source_tree_gate_passed": source_tree_matches,
        "required_checks": required_checks,
        "host_snapshot_before": host_before,
        "host_snapshot_after": host_after,
        "resource_snapshot": "resource-snapshot.json",
        "fixed_output_integrity": "fixed-output-integrity.json",
        "gate_receipt": "gate-receipt.json",
        "smart_band_tree": "smart-band-tree.json",
        "startup_summary": "startup-summary.json",
        "evidence_manifest": "evidence.sha256",
    }
    write_json(evidence_dir / "q0-baseline.json", baseline)
    write_text(
        evidence_dir / "q0-baseline-summary.md",
        render_summary(baseline, startup, resources),
    )
    manifest_files = write_evidence_manifest(evidence_dir)
    baseline["evidence_file_count"] = manifest_files
    write_json(evidence_dir / "q0-baseline.json", baseline)
    write_evidence_manifest(evidence_dir)
    print(json.dumps(baseline, sort_keys=True))
    return 0 if status == "passed" else 1


def write_failure_envelope(evidence_dir: Path, exc: Exception) -> None:
    write_text(evidence_dir / "failure.txt", traceback.format_exc())
    baseline = {
        "status": "failed",
        "failure": {
            "type": type(exc).__name__,
            "message": str(exc),
            "traceback": "failure.txt",
        },
        "evidence_manifest": "evidence.sha256",
    }
    write_json(evidence_dir / "q0-baseline.json", baseline)
    baseline["evidence_file_count"] = write_evidence_manifest(evidence_dir)
    write_json(evidence_dir / "q0-baseline.json", baseline)
    write_evidence_manifest(evidence_dir)


def main() -> int:
    args = parse_args()
    validate_gate_arguments(args)
    if os.name != "posix":
        raise BaselineFailure("formal Q0 collection requires a POSIX host")
    evidence_dir = args.evidence_dir.resolve()
    openvela_root = args.openvela_root.resolve()
    source_repo = args.source_repo.resolve()
    output_dir = resolve_output_dir(openvela_root, args.output_dir)
    require_disjoint_evidence_path(
        evidence_dir, openvela_root, source_repo, output_dir
    )
    ensure_new_evidence_dir(evidence_dir)
    previous_handlers: dict[signal.Signals, Any] = {}

    def handle_signal(signum: int, _frame: Any) -> None:
        raise CollectionInterrupted(f"received {signal.Signals(signum).name}")

    try:
        for handled_signal in (signal.SIGTERM, signal.SIGINT):
            previous_handlers[handled_signal] = signal.getsignal(handled_signal)
            signal.signal(handled_signal, handle_signal)
        try:
            return _collect_baseline(args, evidence_dir)
        except Exception as exc:
            write_failure_envelope(evidence_dir, exc)
            if isinstance(exc, BaselineFailure):
                raise
            raise BaselineFailure(f"unexpected baseline collection failure: {exc}") from exc
    finally:
        for handled_signal, previous_handler in previous_handlers.items():
            signal.signal(handled_signal, previous_handler)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BaselineFailure as exc:
        print(f"Q0 baseline failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
