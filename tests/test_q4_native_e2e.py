"""Unit tests for the Q4 native notification evidence runner."""

from __future__ import annotations

import argparse
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_q4_native_e2e.py"
SPEC = importlib.util.spec_from_file_location("q4_native_e2e", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot import {SCRIPT}")
Q4 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(Q4)


def settings(**overrides: object) -> argparse.Namespace:
    values = {
        "console_port": 5575,
        "boot_timeout": 120.0,
        "command_timeout": 20.0,
        "ui_settle_seconds": 0.5,
        "marker_timeout": 12.0,
    }
    values.update(overrides)
    return argparse.Namespace(**values)


def q4_state(**overrides: int) -> str:
    fields = {
        "elapsed_ms": 1000,
        "notifications": 1,
        "dnd": 0,
        "active_id": 701,
        "active_generation": 2,
        "presentation": 1,
        "haptic_events": 1,
        "wake_requests": 1,
        "haptic_retries": 0,
        "haptic_log_dropped": 0,
        "wake_log_dropped": 0,
        "pending_effects": 0,
        "inbox_dropped": 0,
    }
    fields.update(overrides)
    return Q4.Q4_STATE_MARKER + " " + " ".join(
        f"{key}={value}" for key, value in fields.items()
    )


class Q4NativeHarnessTest(unittest.TestCase):
    def test_state_parser_requires_exact_numeric_contract(self) -> None:
        parsed = Q4.parse_q4_state(q4_state())
        self.assertIsNotNone(parsed)
        assert parsed is not None
        self.assertEqual(parsed["active_id"], 701)
        self.assertEqual(parsed["presentation"], 1)
        self.assertTrue(Q4.effect_pipeline_quiescent(parsed))
        pending = Q4.parse_q4_state(q4_state(pending_effects=1))
        assert pending is not None
        self.assertFalse(Q4.effect_pipeline_quiescent(pending))
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.require_quiescent_effect_pipeline(pending)
        for field in Q4.Q4_CUMULATIVE_FAULT_FIELDS:
            with self.subTest(field=field), self.assertRaises(
                Q4.Q4NativeFailure
            ):
                Q4.require_no_cumulative_q4_faults(
                    Q4.parse_q4_state(q4_state(**{field: 1}))
                )

        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_q4_state(q4_state().replace(" inbox_dropped=0", ""))
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_q4_state(q4_state() + " extra=0")
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_q4_state(q4_state().replace("notifications=1", "notifications=x"))
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_q4_state(
                q4_state(active_id=0, active_generation=2, presentation=1)
            )

    def test_wait_q4_state_waits_for_pending_but_latches_cumulative_faults(
        self,
    ) -> None:
        class Child:
            def __init__(self, transcript: bytes) -> None:
                self.transcript = bytearray(transcript)

            def poll(self) -> None:
                return None

            def pump(self, _seconds: float) -> None:
                pass

        def boot_with(transcript: bytes, timeout: float = 0.02) -> object:
            boot = type("FakeBoot", (), {})()
            boot.child = Child(transcript)
            boot.args = settings(marker_timeout=timeout)
            boot.launch_offset = 0
            return boot

        pending_then_clear = (
            q4_state(pending_effects=1) + "\n" + q4_state(pending_effects=0) + "\n"
        ).encode()
        ready = Q4.wait_q4_state(
            boot_with(pending_then_clear),
            lambda state: state["active_id"] == 701,
            "pending effect drain",
        )
        self.assertEqual(ready["pending_effects"], 0)

        with self.assertRaisesRegex(
            Q4.Q4NativeFailure, "haptic_retries=1"
        ):
            Q4.wait_q4_state(
                boot_with(
                    (
                        q4_state(haptic_retries=1)
                        + "\n"
                        + q4_state(haptic_retries=0)
                        + "\n"
                    ).encode()
                ),
                lambda state: state["active_id"] == 701,
                "latched cumulative fault",
            )

        with self.assertRaisesRegex(
            Q4.Q4NativeFailure, "timed out waiting for pending effect timeout"
        ):
            Q4.wait_q4_state(
                boot_with((q4_state(pending_effects=1) + "\n").encode()),
                lambda state: state["active_id"] == 701,
                "pending effect timeout",
            )

    def test_screenshot_transition_requires_two_distinct_complete_hashes(
        self,
    ) -> None:
        self.assertTrue(
            Q4.screenshots_changed(
                {"sha256": "a" * 64},
                {"sha256": "b" * 64},
            )
        )
        self.assertFalse(
            Q4.screenshots_changed(
                {"sha256": "a" * 64},
                {"sha256": "a" * 64},
            )
        )
        self.assertFalse(Q4.screenshots_changed({}, {"sha256": "b" * 64}))

    def test_call_visual_contract_rejects_distinct_watch_face_and_call(
        self,
    ) -> None:
        width = Q4.NATIVE.EXPECTED_WIDTH
        height = Q4.NATIVE.EXPECTED_HEIGHT
        watch_pixels = b"\xff\xff\xff\xff" * (width * height)
        call_pixels = bytearray(watch_pixels)
        left, top, right, bottom = Q4.CALL_SCREEN_REGION
        dark_row = b"\x10\x3a\x41\xff" * (right - left)
        for y in range(top, bottom):
            start = (y * width + left) * 4
            call_pixels[start : start + len(dark_row)] = dark_row

        watch_visual = Q4.call_fullscreen_visual(
            Q4.NATIVE.PngImage(width, height, watch_pixels)
        )
        call_visual = Q4.call_fullscreen_visual(
            Q4.NATIVE.PngImage(width, height, bytes(call_pixels))
        )
        alice = {"sha256": "a" * 64}
        bob = {"sha256": "b" * 64}

        self.assertFalse(watch_visual["passed"])
        self.assertTrue(call_visual["passed"])
        self.assertTrue(Q4.screenshots_changed(alice, bob))
        self.assertFalse(
            Q4.call_visual_contract(
                alice,
                bob,
                watch_visual,
                call_visual,
            )
        )
        self.assertTrue(
            Q4.call_visual_contract(
                alice,
                bob,
                call_visual,
                call_visual,
            )
        )

    def test_inject_parser_enforces_scenario_phase_and_counts(self) -> None:
        ordinary = Q4.parse_inject_marker(
            "smart_band:q4:inject:v1 scenario=ordinary phase=updated "
            "accepted=1 requested=1"
        )
        self.assertEqual(
            ordinary,
            {
                "scenario": "ordinary",
                "phase": "updated",
                "accepted": 1,
                "requested": 1,
            },
        )
        workout = Q4.parse_inject_marker(
            "smart_band:q4:inject:v1 scenario=workout phase=armed "
            "accepted=0 requested=1"
        )
        self.assertEqual(workout["accepted"], 0)
        for line in (
            "smart_band:q4:inject:v1 scenario=center phase=updated "
            "accepted=1 requested=1",
            "smart_band:q4:inject:v1 scenario=calls phase=ready "
            "accepted=3 requested=2",
            "smart_band:q4:inject:v1 scenario=calls phase=ready "
            "accepted=2 requested=2 accepted=2",
        ):
            with self.subTest(line=line), self.assertRaises(
                Q4.Q4NativeFailure
            ):
                Q4.parse_inject_marker(line)

    def test_effect_parsers_require_simulation_disclaimers(self) -> None:
        haptic = Q4.parse_haptic_marker(
            "smart_band:q4:haptic:v1 notification_id=721 generation=9 "
            "pattern=urgent simulated=1"
        )
        wake = Q4.parse_wake_marker(
            "smart_band:q4:wake:v1 notification_id=721 generation=9 "
            "reason=notification synthetic=1 power_transition=0"
        )
        self.assertEqual(haptic["pattern"], "urgent")
        self.assertEqual(wake["power_transition"], 0)
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_haptic_marker(
                "smart_band:q4:haptic:v1 notification_id=721 generation=9 "
                "pattern=urgent simulated=0"
            )
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_wake_marker(
                "smart_band:q4:wake:v1 notification_id=721 generation=9 "
                "reason=notification synthetic=1 power_transition=1"
            )

    def test_action_parser_requires_exact_applied_identity(self) -> None:
        action = Q4.parse_action_marker(
            "smart_band:q4:action:v1 notification_id=715 "
            "command=read result=applied"
        )
        self.assertEqual(
            action,
            {
                "notification_id": 715,
                "command": "read",
                "result": "applied",
            },
        )
        for line in (
            "smart_band:q4:action:v1 notification_id=0 "
            "command=read result=applied",
            "smart_band:q4:action:v1 notification_id=715 "
            "command=unknown result=applied",
            "smart_band:q4:action:v1 notification_id=715 "
            "command=read result=unknown",
        ):
            with self.subTest(line=line), self.assertRaises(
                Q4.Q4NativeFailure
            ):
                Q4.parse_action_marker(line)

    def test_marker_collection_fails_closed_on_matching_malformed_line(self) -> None:
        transcript = (
            b"unrelated\n"
            b"smart_band:q4:inject:v1 scenario=ordinary phase=initial "
            b"accepted=1 requested=1\n"
        )
        records = Q4.marker_records(
            transcript, Q4.Q4_INJECT_MARKER, Q4.parse_inject_marker
        )
        self.assertEqual(len(records), 1)
        self.assertEqual(
            Q4.marker_records(
                transcript + b"smart_band:q4:inject:v1 scenario=ordi",
                Q4.Q4_INJECT_MARKER,
                Q4.parse_inject_marker,
            ),
            records,
        )
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.marker_records(
                transcript + b"smart_band:q4:inject:v1 bad\n",
                Q4.Q4_INJECT_MARKER,
                Q4.parse_inject_marker,
            )

    def test_ps_stack_parser_and_summary_report_peak_and_margin(self) -> None:
        output = (
            "\x1b[Kps\r\n"
            "  PID GROUP PRI POLICY TYPE NPX STATE EVENT SIGMASK "
            "STACK USED FILLED COMMAND\r\n"
            "   13    13 100 RR Task - Waiting Semaphore 00000000 "
            "0032528 0011936 36.6% smart_band\r\n"
            "goldfish-armv8a-ap> "
        )
        parsed = Q4.parse_ps_stack(output)
        self.assertEqual(parsed["pid"], 13)
        self.assertEqual(parsed["stack_size_bytes"], 32528)
        self.assertEqual(parsed["stack_used_bytes"], 11936)
        self.assertEqual(parsed["margin_bytes"], 20592)
        self.assertGreater(parsed["margin_percent"], 25.0)

        samples = []
        for index, (scenario, checkpoint) in enumerate(
            (scenario, checkpoint)
            for scenario in Q4.SCENARIOS
            for checkpoint in Q4.STACK_CHECKPOINTS[scenario]
        ):
            sample = dict(parsed)
            used = 10000 + index * 100
            sample.update(
                {
                    "scenario": scenario,
                    "checkpoint": checkpoint,
                    "stack_used_bytes": used,
                    "margin_bytes": 32528 - used,
                    "filled_percent": used * 100.0 / 32528,
                }
            )
            samples.append(sample)
        summary = Q4.summarize_stack_samples(samples)
        self.assertEqual(summary["max_stack_used_bytes"], 10300)
        self.assertEqual(summary["minimum_margin_bytes"], 22228)
        self.assertGreater(summary["minimum_margin_percent"], 25.0)
        self.assertEqual(summary["peak_used_scenario"], "workout")
        self.assertEqual(summary["scenario_count"], 4)
        self.assertEqual(summary["sample_count"], 4)
        self.assertEqual(
            summary["checkpoints_by_scenario"],
            {
                scenario: list(Q4.STACK_CHECKPOINTS[scenario])
                for scenario in Q4.SCENARIOS
            },
        )

        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.summarize_stack_samples(samples[:-1])
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.summarize_stack_samples(samples + [dict(samples[0])])

    def test_stack_evidence_fails_closed(self) -> None:
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_ps_stack("PID COMMAND\n13 smart_band\n")
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_ps_stack(
                "PID GROUP STACK USED FILLED COMMAND\n"
                "13 13 x 32528 40000 120.0% smart_band\n"
            )
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.parse_ps_stack(
                "PID GROUP PRI POLICY TYPE NPX STATE EVENT SIGMASK "
                "STACK USED FILLED COMMAND\n"
                "13 13 100 RR Task - Waiting Semaphore 00000000 "
                "1000 751 75.1% smart_band\n"
            )
        boundary = Q4.parse_ps_stack(
            "PID GROUP PRI POLICY TYPE NPX STATE EVENT SIGMASK "
            "STACK USED FILLED COMMAND\n"
            "13 13 100 RR Task - Waiting Semaphore 00000000 "
            "1000 750 75.0% smart_band\n"
        )
        self.assertEqual(boundary["margin_percent"], 25.0)
        incomplete = [
            {
                "scenario": "ordinary",
                "checkpoint": "only",
                "stack_size_bytes": 32528,
                "stack_used_bytes": 10000,
                "margin_bytes": 22528,
                "filled_percent": 30.7,
            }
        ]
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.summarize_stack_samples(incomplete)

    def test_stack_sampling_is_dispatched_before_strict_collection(self) -> None:
        ps_output = (
            "PID GROUP PRI POLICY TYPE NPX STATE EVENT SIGMASK "
            "STACK USED FILLED COMMAND\n"
            "13 13 100 RR Task - Waiting Semaphore 00000000 "
            "32528 11936 36.6% smart_band\n"
            "goldfish-armv8a-ap> "
        )

        class Child:
            def __init__(self) -> None:
                self.commands: list[str] = []
                self.pumps: list[float] = []

            def send_command(
                self,
                command: str,
                _prompt: bytes,
                _timeout: float,
                _destination: Path,
            ) -> str:
                self.commands.append(command)
                return ps_output if command.startswith("cat ") else "prompt"

            def pump(self, seconds: float) -> None:
                self.pumps.append(seconds)

        with tempfile.TemporaryDirectory() as directory:
            boot = Q4.Boot.__new__(Q4.Boot)
            boot.child = Child()
            boot.scenario = "ordinary"
            boot.prompt = b"goldfish-armv8a-ap> "
            boot.args = settings()
            boot.evidence_dir = Path(directory)
            boot.stack_samples = []
            boot.pending_stack_samples = []

            pending = boot.sample_stack("initial")
            self.assertTrue(pending["pending"])
            self.assertEqual(boot.stack_samples, [])
            self.assertEqual(
                boot.child.commands,
                ["ps > /data/q4-stack-ordinary-01.txt &"],
            )

            boot.collect_stack_samples()
            self.assertEqual(boot.child.pumps, [0.5])
            self.assertEqual(len(boot.stack_samples), 1)
            self.assertEqual(boot.stack_samples[0]["checkpoint"], "initial")
            self.assertEqual(boot.stack_samples[0]["stack_used_bytes"], 11936)
            self.assertEqual(
                boot.child.commands,
                [
                    "ps > /data/q4-stack-ordinary-01.txt &",
                    "cat /data/q4-stack-ordinary-01.txt",
                    "rm /data/q4-stack-ordinary-01.txt",
                ],
            )
            self.assertEqual(boot.pending_stack_samples, [])

    def test_effect_pairing_requires_exact_ids_and_generations(self) -> None:
        transcript = (
            "smart_band:q4:haptic:v1 notification_id=721 generation=3 "
            "pattern=urgent simulated=1\n"
            "smart_band:q4:wake:v1 notification_id=721 generation=3 "
            "reason=notification synthetic=1 power_transition=0\n"
            "smart_band:q4:haptic:v1 notification_id=722 generation=4 "
            "pattern=urgent simulated=1\n"
            "smart_band:q4:wake:v1 notification_id=722 generation=4 "
            "reason=notification synthetic=1 power_transition=0\n"
        ).encode()
        paired = Q4.effect_pairs(transcript, [721, 722])
        self.assertEqual(len(paired["paired_identities"]), 2)
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.effect_pairs(transcript, [721])
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.effect_pairs(
                transcript.replace(b"generation=4", b"generation=5", 1),
                [721, 722],
            )

    def test_ordinary_effect_contract_allows_one_update_wake(self) -> None:
        transcript = (
            "smart_band:q4:haptic:v1 notification_id=701 generation=3 "
            "pattern=normal simulated=1\n"
            "smart_band:q4:haptic:v1 notification_id=701 generation=4 "
            "pattern=normal simulated=1\n"
            "smart_band:q4:wake:v1 notification_id=701 generation=4 "
            "reason=notification synthetic=1 power_transition=0\n"
        ).encode()
        effects = Q4.effect_pairs(
            transcript,
            [701, 701],
            [701],
            require_identity_pairs=False,
        )
        self.assertEqual(
            effects["haptics"][-1]["generation"],
            effects["wakes"][0]["generation"],
        )
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.effect_pairs(transcript, [701, 701])

    def test_launch_argv_and_four_scenario_registry_are_exact(self) -> None:
        self.assertEqual(tuple(Q4.SCENARIO_RUNNERS), Q4.SCENARIOS)
        for scenario in Q4.SCENARIOS:
            self.assertEqual(
                Q4.app_launch_command(scenario),
                f"smart_band --q4-native-scenario={scenario} &",
            )
        with self.assertRaises(Q4.Q4NativeFailure):
            Q4.app_launch_command("unknown")

    def test_all_q4_points_use_reviewable_framed_coordinate_transform(self) -> None:
        points = (
            Q4.NOTIFICATION_LAUNCHER_POINT,
            Q4.OVERLAY_BODY_POINT,
            Q4.OVERLAY_DRAG_START,
            Q4.OVERLAY_DRAG_END,
            Q4.OVERLAY_DISMISS_POINT,
            Q4.OVERLAY_REJECT_POINT,
            Q4.CENTER_FIRST_READ_POINT,
            Q4.CENTER_FIRST_DELETE_POINT,
            Q4.CALL_REJECT_POINT,
            Q4.CALL_ACCEPT_POINT,
        )
        transformed = [Q4.local_point(point) for point in points]
        for source, destination in zip(points, transformed):
            with self.subTest(source=source):
                self.assertGreaterEqual(source[0], 0)
                self.assertLess(source[0], Q4.Q3.LAYOUT_SIZE[0])
                self.assertGreaterEqual(source[1], 0)
                self.assertLess(source[1], Q4.Q3.LAYOUT_SIZE[1])
                self.assertGreaterEqual(destination[0], Q4.Q3.SCREEN_ORIGIN[0])
                self.assertGreaterEqual(destination[1], Q4.Q3.SCREEN_ORIGIN[1])
        self.assertEqual(
            Q4.local_point(Q4.NOTIFICATION_LAUNCHER_POINT), (554, 282)
        )
        self.assertEqual(Q4.CENTER_FIRST_READ_POINT, (228, 183))
        self.assertEqual(Q4.CENTER_FIRST_DELETE_POINT, (295, 183))
        self.assertEqual(Q4.OVERLAY_DISMISS_POINT[1], 137)
        self.assertEqual(Q4.OVERLAY_REJECT_POINT[1], 137)

    def test_drag_uses_press_motion_and_one_release(self) -> None:
        class Console:
            def __init__(self) -> None:
                self.commands: list[str] = []

            def command(self, command: str, _name: str) -> str:
                self.commands.append(command)
                return "OK\n"

        class Child:
            def __init__(self) -> None:
                self.pumps: list[float] = []

            def pump(self, seconds: float) -> None:
                self.pumps.append(seconds)

        boot = type("FakeBoot", (), {})()
        boot.console = Console()
        boot.child = Child()
        Q4.send_drag(
            boot, "isolation", Q4.OVERLAY_DRAG_START, Q4.OVERLAY_DRAG_END
        )
        self.assertEqual(len(boot.console.commands), 6)
        self.assertTrue(all(item.endswith(" 1") for item in boot.console.commands[:-1]))
        self.assertTrue(boot.console.commands[-1].endswith(" 0"))
        self.assertEqual(boot.child.pumps, [0.03] * 6)

    def test_center_mark_read_wait_starts_at_action_boundary(self) -> None:
        class Child:
            def __init__(self) -> None:
                self.transcript = bytearray(b"pre-action-state\n")

            def pump(self, _seconds: float) -> None:
                pass

        class Boot:
            def __init__(self) -> None:
                self.child = Child()
                self.args = settings()
                self.launch_offset = 0

            def click(self, _name: str, _point: tuple[int, int]) -> None:
                self.child.transcript.extend(b"post-click\n")

            def screenshot(self, name: str) -> dict[str, str]:
                return {"name": name}

            def sample_stack(self, checkpoint: str) -> dict[str, str]:
                return {"checkpoint": checkpoint}

        boot = Boot()
        waits: list[tuple[str, int | None]] = []

        def fake_wait_q4_state(
            _boot: object,
            _predicate: object,
            description: str,
            start: int | None = None,
        ) -> dict[str, int]:
            waits.append((description, start))
            return {}

        with (
            mock.patch.object(Q4, "wait_inject", return_value={}),
            mock.patch.object(Q4, "open_notification_center", return_value={}),
            mock.patch.object(Q4, "effect_pairs", return_value={}),
            mock.patch.object(Q4, "wait_action", return_value={}),
            mock.patch.object(Q4, "wait_q4_state", side_effect=fake_wait_q4_state),
        ):
            Q4.run_center(boot)

        mark_read_wait = next(
            item for item in waits if item[0] == "Notification Center Mark read action"
        )
        self.assertEqual(mark_read_wait[1], len(b"pre-action-state\n"))

    def test_process_metadata_is_attributed_and_atomically_replaced(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            boot = Q4.Boot.__new__(Q4.Boot)
            boot.evidence_dir = Path(directory)
            boot.run_id = "q4-run-id"
            boot.runtime_output = Path(directory) / "runtime-output"
            boot.executables = (Path("/emulator"), Path("/qemu"))
            boot.process_baseline = [
                {"pid": 7, "starttime_ticks": 11}
            ]
            boot.write_process_metadata(13, 13)
            process_file = Path(directory) / "emulator-process.json"
            metadata = json.loads(process_file.read_text(encoding="utf-8"))
            self.assertEqual(metadata["run_id"], "q4-run-id")
            self.assertEqual(metadata["pid"], 13)
            self.assertEqual(metadata["pgid"], 13)
            self.assertEqual(metadata["baseline"], boot.process_baseline)
            self.assertFalse(
                (Path(directory) / "emulator-process.json.tmp").exists()
            )

    def test_settings_fail_closed(self) -> None:
        Q4.validate_settings(settings())
        for bad in (
            settings(console_port=0),
            settings(boot_timeout=0),
            settings(marker_timeout=-1),
        ):
            with self.subTest(bad=bad), self.assertRaises(Q4.Q4NativeFailure):
                Q4.validate_settings(bad)


if __name__ == "__main__":
    unittest.main()
