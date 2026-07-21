#!/usr/bin/env python3
"""Compare a native Lotus screenshot outside reviewed dynamic regions."""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
E2E_PATH = ROOT / "scripts" / "run_native_e2e.py"
SPEC = importlib.util.spec_from_file_location("watch_face_e2e", E2E_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load native comparison helpers: {E2E_PATH}")
E2E = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(E2E)

PROFILES = {
    "framed": {
        "reference": E2E.WATCH_FACE_REFERENCE,
        "masks": E2E.WATCH_FACE_DYNAMIC_MASKS,
        "width": 1280,
        "height": 800,
        "minimum_compared_ratio": 0.96,
    },
    "compact": {
        "reference": E2E.COMPACT_WATCH_FACE_REFERENCE,
        "masks": E2E.COMPACT_WATCH_FACE_DYNAMIC_MASKS,
        "width": 336,
        "height": 480,
        "minimum_compared_ratio": 0.90,
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare a Lotus native screenshot to a reviewed baseline."
    )
    parser.add_argument("--profile", choices=sorted(PROFILES), required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def compare(profile_name: str, candidate_path: Path) -> dict[str, object]:
    profile = PROFILES[profile_name]
    reference_path = Path(profile["reference"])
    width = int(profile["width"])
    height = int(profile["height"])
    reference, reference_record = E2E.screenshot_record(
        reference_path, width, height
    )
    candidate, candidate_record = E2E.screenshot_record(
        candidate_path, width, height
    )
    difference = E2E.masked_image_difference(
        reference, candidate, profile["masks"]
    )
    minimum_ratio = float(profile["minimum_compared_ratio"])
    passed = bool(
        difference["exact_match_outside_masks"]
        and difference["compared_ratio"] >= minimum_ratio
    )
    return {
        "schema_version": 1,
        "profile": profile_name,
        "reference": reference_record,
        "candidate": candidate_record,
        "minimum_compared_ratio": minimum_ratio,
        "difference": difference,
        "passed": passed,
    }


def main() -> int:
    args = parse_args()
    result = compare(args.profile, args.candidate.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, sort_keys=True))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, E2E.NativeE2EFailure) as exc:
        print(f"native watch-face comparison failed: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
