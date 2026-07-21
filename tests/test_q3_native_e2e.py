"""Unit tests for the Q3 native E2E evidence harness."""

from __future__ import annotations

import argparse
import importlib.util
import unittest
from pathlib import Path
from unittest import mock


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
    @staticmethod
    def marker_for_page(page: int) -> bytes:
        return (
            f"smart_band:q3:v1 elapsed_ms={page + 1} page={page} view=0 "
            "state=0 mode=0 active_ms=0 steps=0 recovery=0 phase=0 "
            "checkpoint=0 daily=0 sessions=0 daily_store=0 session_store=0 "
            "queue=0 dropped=0 evicted=0 coalesced=0 inbox_dropped=0 "
            "objects=1 tick_gap_max_ms=0\r\n"
        ).encode()

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
        self.assertEqual(parsed["page"], Q3.PAGE_APPS)
        self.assertEqual(parsed["view"], Q3.VIEW_WORKOUT)
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
        self.assertEqual(
            Q3.framed_screen_geometry(1280, 800), ((453, 43), (373, 714))
        )
        self.assertEqual(
            Q3.framed_screen_geometry(336, 480), ((0, 0), (336, 480))
        )
        self.assertEqual(Q3.local_point(0, 0), Q3.SCREEN_ORIGIN)
        self.assertEqual(Q3.local_point(329, 625), (824, 755))
        self.assertEqual(
            Q3.local_point(*Q3.WORKOUT_LAUNCHER_POINT), (554, 244)
        )
        self.assertEqual(
            Q3.local_point(*Q3.HISTORY_LAUNCHER_POINT), (731, 244)
        )
        self.assertEqual(Q3.local_point(*Q3.START_WALK_POINT), (554, 459))
        self.assertEqual(
            Q3.local_point(*Q3.SESSION_PRIMARY_POINT), (523, 722)
        )
        self.assertEqual(
            Q3.local_point(*Q3.SESSION_FINISH_POINT), (642, 722)
        )
        self.assertEqual(
            Q3.local_point(*Q3.RECOVERY_RESUME_POINT), (554, 703)
        )
        self.assertEqual(
            Q3.local_point(*Q3.CONFIRM_ACCEPT_POINT), (731, 696)
        )
        self.assertEqual(
            Q3.local_point(*Q3.SUMMARY_DONE_POINT), (642, 720)
        )
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.local_point(330, 100)
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.framed_screen_geometry(0, 800)

    def test_settings_reject_unsafe_or_non_gate_values(self) -> None:
        Q3.validate_settings(settings())
        for bad in ("data/q3", "/data/../q3", "/data/q3;rm"):
            with self.subTest(path=bad), self.assertRaises(Q3.Q3NativeFailure):
                Q3.validate_settings(settings(storage_path=bad))
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.validate_settings(settings(sample_interval_seconds=0))
        with self.assertRaises(Q3.Q3NativeFailure):
            Q3.validate_settings(settings(soak_seconds=5))

    def test_swipe_to_apps_retries_until_structured_page_marker(self) -> None:
        class Console:
            def __init__(self) -> None:
                self.commands: list[str] = []

            def command(self, command: str, _name: str) -> str:
                self.commands.append(command)
                return "OK\n"

        class Child:
            def __init__(self, owner: Q3NativeHarnessTest) -> None:
                self.owner = owner
                self.transcript = owner.marker_for_page(0)
                self.pumps: list[float] = []

            def pump(self, seconds: float) -> None:
                self.pumps.append(seconds)
                self.transcript += self.owner.marker_for_page(len(self.pumps))

        console = Console()
        child = Child(self)
        with mock.patch.object(Q3.time, "sleep") as sleep:
            Q3.swipe_to_apps(console, child, ROOT)
        expected_commands = 3 * len(Q3.NATIVE.build_swipe_commands())
        self.assertEqual(len(console.commands), expected_commands)
        self.assertEqual(sleep.call_count, expected_commands)
        self.assertEqual(child.pumps, [Q3.POST_SWIPE_SECONDS] * 3)

    def test_click_pumps_frame_between_mouse_down_and_up(self) -> None:
        events: list[tuple[str, object]] = []

        class Console:
            def command(self, command: str, name: str) -> str:
                events.append(("command", (command, name)))
                return "OK\n"

        class Child:
            def pump(self, seconds: float) -> None:
                events.append(("pump", seconds))

        Q3.click(Console(), Child(), ROOT, "start-walk", (554, 485))

        self.assertEqual(
            events,
            [
                (
                    "command",
                    ("event mouse 554 485 0 1", "console-start-walk-down.txt"),
                ),
                ("pump", Q3.CLICK_HOLD_SECONDS),
                (
                    "command",
                    ("event mouse 554 485 0 0", "console-start-walk-up.txt"),
                ),
            ],
        )

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
