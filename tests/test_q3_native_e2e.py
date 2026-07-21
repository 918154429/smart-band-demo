"""Unit tests for the Q3 native E2E evidence harness."""

from __future__ import annotations

import argparse
import importlib.util
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_q3_native_e2e.py"
SPEC = importlib.util.spec_from_file_location("q3_native_e2e", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot import {SCRIPT}")
Q3 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(Q3)


def settings(**overrides: object) -> argparse.Namespace:
    values = {
        "console_port": 5565,
        "storage_path": "/data/smart-band-q3",
        "boot_timeout": 120.0,
        "command_timeout": 20.0,
        "ui_settle_seconds": 3.0,
        "warmup_seconds": 0,
        "soak_seconds": 0,
        "sample_interval_seconds": 10,
    }
    values.update(overrides)
    return argparse.Namespace(**values)


class Q3NativeHarnessTest(unittest.TestCase):
    def test_parse_marker_requires_complete_numeric_contract(self) -> None:
        line = (
            "smart_band:q3:v1 elapsed_ms=4200 page=3 view=1 state=3 mode=0 "
            "active_ms=1000 steps=7 recovery=0 phase=0 checkpoint=0 daily=1 "
            "sessions=0 daily_store=0 session_store=1 queue=0 dropped=0 "
            "evicted=0 coalesced=2 inbox_dropped=0 objects=88 "
            "tick_gap_max_ms=1003"
        )
        parsed = Q3.parse_q3_marker(line)
        self.assertIsNotNone(parsed)
        assert parsed is not None
        self.assertEqual(parsed["state"], Q3.STATE_PAUSED)
        self.assertEqual(parsed["steps"], 7)
        self.assertEqual(parsed["objects"], 88)
        self.assertIsNone(Q3.parse_q3_marker(line.replace(" objects=88", "")))
        self.assertIsNone(Q3.parse_q3_marker(line.replace("steps=7", "steps=bad")))

    def test_marker_states_ignores_unrelated_console_output(self) -> None:
        marker = (
            "smart_band:q3:v1 elapsed_ms=1 page=0 view=0 state=0 mode=0 "
            "active_ms=0 steps=0 recovery=0 phase=0 checkpoint=0 daily=0 "
            "sessions=0 daily_store=0 session_store=0 queue=0 dropped=0 "
            "evicted=0 coalesced=0 inbox_dropped=0 objects=1 "
            "tick_gap_max_ms=0"
        )
        states = Q3.marker_states(f"NSH>\r\n{marker}\r\nOK\r\n".encode())
        self.assertEqual(len(states), 1)
        self.assertEqual(states[0]["elapsed_ms"], 1)

    def test_local_points_map_into_framed_emulator(self) -> None:
        self.assertEqual(Q3.local_point(0, 0), Q3.SCREEN_ORIGIN)
        self.assertEqual(Q3.local_point(335, 479), (807, 639))
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.local_point(336, 100)

    def test_settings_reject_unsafe_or_non_gate_values(self) -> None:
        Q3.validate_settings(settings())
        for bad in ("data/q3", "/data/../q3", "/data/q3;rm"):
            with self.subTest(path=bad), self.assertRaises(Q3.Q3NativeFailure):
                Q3.validate_settings(settings(storage_path=bad))
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.validate_settings(settings(sample_interval_seconds=0))
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.validate_settings(settings(soak_seconds=5))

    def test_stability_contract_rejects_queue_or_timer_regression(self) -> None:
        stable = {
            "queue": 0,
            "dropped": 0,
            "evicted": 0,
            "inbox_dropped": 0,
            "objects": 80,
            "tick_gap_max_ms": 1100,
        }
        Q3.require_stable_state(stable)
        for key, value in (("dropped", 1), ("objects", 0),
                           ("tick_gap_max_ms", 2501)):
            changed = dict(stable)
            changed[key] = value
            with self.subTest(key=key), self.assertRaises(Q3.Q3NativeFailure):
                Q3.require_stable_state(changed)


if __name__ == "__main__":
    unittest.main()
