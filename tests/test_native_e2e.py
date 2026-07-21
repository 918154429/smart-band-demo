#!/usr/bin/env python3

from __future__ import annotations

import binascii
import importlib.util
import os
import subprocess
import struct
import sys
import tempfile
import time
import unittest
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "run_native_e2e.py"
SPEC = importlib.util.spec_from_file_location("native_e2e", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
E2E = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(E2E)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_rgba_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    if len(pixels) != width * height * 4:
        raise ValueError("invalid RGBA pixel count")
    rows = b"".join(
        b"\x00" + pixels[row * width * 4 : (row + 1) * width * 4]
        for row in range(height)
    )
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(rows))
        + png_chunk(b"IEND", b"")
    )


def solid_pixels(width: int, height: int, color: tuple[int, int, int, int]) -> bytearray:
    return bytearray(bytes(color) * (width * height))


class NativeE2ETest(unittest.TestCase):
    def test_nonempty_evidence_directory_is_rejected_without_removing_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence = Path(directory)
            old_failure = evidence / "old-failure.log"
            old_failure.write_text("preserve\n", encoding="utf-8")
            with self.assertRaisesRegex(E2E.NativeE2EFailure, "not empty"):
                E2E.ensure_empty_evidence_dir(evidence)
            self.assertEqual(old_failure.read_text(encoding="utf-8"), "preserve\n")

    def test_runtime_output_isolates_every_input_and_enables_heart_rate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "fixed-output"
            runtime = base / "evidence" / "runtime-output"
            source.mkdir()
            runtime.parent.mkdir()
            for index, name in enumerate(E2E.RUNTIME_INPUTS, 1):
                source.joinpath(name).write_bytes(f"input-{index}".encode())

            result = E2E.stage_runtime_output(source, runtime)

            for name in E2E.RUNTIME_INPUTS:
                self.assertFalse(os.path.samefile(source / name, runtime / name))
                self.assertFalse(result["files"][name]["hardlinked"])
                self.assertTrue(result["files"][name]["isolated"])
                self.assertTrue(result["files"][name]["initial_hash_matches"])
            runtime.joinpath("nuttx").write_bytes(b"runtime mutation")
            self.assertEqual(source.joinpath("nuttx").read_bytes(), b"input-2")
            self.assertEqual(
                runtime.joinpath("config.ini").read_text(encoding="utf-8"),
                "hw.sensors.heart_rate=yes\n",
            )
            self.assertTrue(E2E.verify_fixed_output_unchanged(result))
            cleanup = E2E.cleanup_staged_runtime_inputs(runtime)
            self.assertTrue(cleanup["passed"])
            self.assertEqual(set(cleanup["removed"]), set(E2E.RUNTIME_INPUTS))
            self.assertTrue(source.joinpath("vela_data.bin").is_file())
            self.assertFalse(runtime.exists())

    def test_partial_runtime_staging_is_rolled_back(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "fixed-output"
            runtime = base / "evidence" / "runtime-output"
            source.mkdir()
            runtime.parent.mkdir()
            for name in E2E.RUNTIME_INPUTS[:-1]:
                source.joinpath(name).write_bytes(b"input")

            with self.assertRaisesRegex(E2E.NativeE2EFailure, "missing or empty"):
                E2E.stage_runtime_output(source, runtime)

            self.assertFalse(runtime.exists())

    def test_evidence_and_fixed_output_must_be_disjoint(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "fixed-output"
            output.mkdir()
            with self.assertRaisesRegex(E2E.NativeE2EFailure, "must not overlap"):
                E2E.require_disjoint_paths(output, output / "evidence")
            E2E.require_disjoint_paths(output, Path(directory) / "evidence")

    def test_png_decoder_and_nonblank_validation_use_only_rgba_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "screen.png"
            pixels = solid_pixels(3, 2, (240, 245, 244, 255))
            pixels[4:8] = bytes((0, 12, 16, 255))
            write_rgba_png(path, 3, 2, bytes(pixels))

            image, record = E2E.screenshot_record(path, 3, 2)

            self.assertEqual((image.width, image.height), (3, 2))
            self.assertEqual(image.pixels, bytes(pixels))
            self.assertEqual(record["format"], "RGBA8 non-interlaced PNG")
            self.assertTrue(record["nonblank"])
            self.assertEqual(record["unique_rgba_colors"], 2)

    def test_png_decoder_rejects_crc_corruption(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "corrupt.png"
            write_rgba_png(path, 2, 2, bytes(solid_pixels(2, 2, (1, 2, 3, 255))))
            data = bytearray(path.read_bytes())
            data[-5] ^= 0x01
            path.write_bytes(data)
            with self.assertRaisesRegex(E2E.NativeE2EFailure, "CRC mismatch"):
                E2E.decode_png_rgba(path)

    def test_region_difference_is_limited_to_the_target_rectangle(self) -> None:
        width = height = 4
        before_pixels = solid_pixels(width, height, (10, 20, 30, 255))
        after_pixels = bytearray(before_pixels)
        inside = (1 * width + 1) * 4
        outside = (3 * width + 3) * 4
        after_pixels[inside : inside + 4] = bytes((100, 110, 120, 255))
        after_pixels[outside : outside + 4] = bytes((200, 210, 220, 255))
        before = E2E.PngImage(width, height, bytes(before_pixels))
        after = E2E.PngImage(width, height, bytes(after_pixels))

        difference = E2E.region_difference(before, after, (0, 0, 2, 2))

        self.assertEqual(difference["changed_pixels"], 1)
        self.assertEqual(difference["total_pixels"], 4)
        self.assertEqual(difference["changed_ratio"], 0.25)

    def test_region_fingerprint_rejects_a_single_pixel_change(self) -> None:
        width = height = 4
        pixels = solid_pixels(width, height, (10, 20, 30, 255))
        before = E2E.PngImage(width, height, bytes(pixels))
        expected = E2E.region_fingerprint(before, (1, 1, 3, 3))["sha256"]
        offset = (1 * width + 1) * 4
        pixels[offset : offset + 4] = bytes((11, 20, 30, 255))
        after = E2E.PngImage(width, height, bytes(pixels))

        self.assertNotEqual(
            E2E.region_fingerprint(after, (1, 1, 3, 3))["sha256"], expected
        )

    def test_masked_difference_ignores_only_reviewed_rectangles(self) -> None:
        width = height = 4
        reference_pixels = solid_pixels(width, height, (10, 20, 30, 255))
        candidate_pixels = bytearray(reference_pixels)
        inside_mask = (1 * width + 1) * 4
        outside_mask = (3 * width + 3) * 4
        candidate_pixels[inside_mask : inside_mask + 4] = bytes((100, 20, 30, 255))
        reference = E2E.PngImage(width, height, bytes(reference_pixels))
        candidate = E2E.PngImage(width, height, bytes(candidate_pixels))

        ignored = E2E.masked_image_difference(
            reference, candidate, {"dynamic": (1, 1, 2, 2)}
        )
        self.assertTrue(ignored["exact_match_outside_masks"])
        self.assertEqual(ignored["masked_pixels"], 1)
        self.assertEqual(ignored["compared_pixels"], 15)

        candidate_pixels[outside_mask : outside_mask + 4] = bytes((10, 99, 30, 255))
        candidate = E2E.PngImage(width, height, bytes(candidate_pixels))
        changed = E2E.masked_image_difference(
            reference, candidate, {"dynamic": (1, 1, 2, 2)}
        )
        self.assertFalse(changed["exact_match_outside_masks"])
        self.assertEqual(changed["changed_pixels"], 1)
        self.assertEqual(changed["mismatch_bounds"], [3, 3, 4, 4])

    def test_reviewed_watch_face_masks_leave_most_pixels_exact(self) -> None:
        reference = E2E.decode_png_rgba(E2E.WATCH_FACE_REFERENCE)
        comparison = E2E.masked_image_difference(
            reference, reference, E2E.WATCH_FACE_DYNAMIC_MASKS
        )
        self.assertTrue(comparison["exact_match_outside_masks"])
        self.assertGreater(comparison["compared_ratio"], 0.95)

    def test_reviewed_compact_watch_face_masks_leave_most_pixels_exact(self) -> None:
        reference = E2E.decode_png_rgba(E2E.COMPACT_WATCH_FACE_REFERENCE)
        comparison = E2E.masked_image_difference(
            reference, reference, E2E.COMPACT_WATCH_FACE_DYNAMIC_MASKS
        )
        self.assertEqual(
            E2E.COMPACT_WATCH_FACE_DYNAMIC_MASKS["time"], (98, 84, 247, 116)
        )
        self.assertTrue(comparison["exact_match_outside_masks"])
        self.assertGreater(comparison["compared_ratio"], 0.90)

    def test_reviewed_native_images_match_only_the_expected_golden_text(self) -> None:
        evidence = ROOT / "docs" / "evidence"
        model = E2E.decode_png_rgba(
            evidence / "q1v-native-e2e-heart-model-20260720.png"
        )
        sensor = E2E.decode_png_rgba(
            evidence / "q1v-native-e2e-heart-sensor-20260720.png"
        )
        self.assertTrue(E2E.golden_region_record(model, "heart_page_title")["matches"])
        self.assertTrue(
            E2E.golden_region_record(sensor, "heart_value_104_bpm")["matches"]
        )
        self.assertTrue(E2E.golden_region_record(sensor, "source_sensor")["matches"])
        self.assertFalse(
            E2E.golden_region_record(model, "heart_value_104_bpm")["matches"]
        )
        self.assertFalse(E2E.golden_region_record(model, "source_sensor")["matches"])

    def test_swipe_commands_are_a_right_to_left_press_drag_release(self) -> None:
        commands = E2E.build_swipe_commands()
        self.assertEqual(commands[0], "event mouse 800 400 0 1")
        self.assertEqual(commands[-1], "event mouse 480 400 0 0")
        self.assertEqual(len(commands), 10)
        pressed_x = [int(command.split()[2]) for command in commands[:-1]]
        self.assertEqual(pressed_x, sorted(pressed_x, reverse=True))

    def test_required_checks_include_cleanup_and_pixel_transitions(self) -> None:
        checks = {name: True for name in E2E.REQUIRED_CHECKS}
        self.assertTrue(E2E.all_required_checks_pass(checks))
        checks["port_cleanup"] = False
        self.assertFalse(E2E.all_required_checks_pass(checks))
        checks["port_cleanup"] = True
        checks["source_label_transition_pixels"] = False
        self.assertFalse(E2E.all_required_checks_pass(checks))
        checks["source_label_transition_pixels"] = True
        checks["heart_value_104_bpm_golden"] = False
        self.assertFalse(E2E.all_required_checks_pass(checks))
        checks["heart_value_104_bpm_golden"] = True
        checks["activity_face_selected"] = False
        self.assertFalse(E2E.all_required_checks_pass(checks))

    @unittest.skipUnless(os.name == "posix", "POSIX attributed-process cleanup")
    def test_attributed_cleanup_finds_a_detached_run_id_process(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            runtime = Path(directory) / "runtime-output"
            run_id = "unit-test-run-id"
            baseline = E2E.attributed_process_snapshot(run_id, runtime, ())
            environment = os.environ.copy()
            environment[E2E.RUN_ID_ENV] = run_id
            child = subprocess.Popen(
                [sys.executable, "-c", "import time; time.sleep(30)"],
                env=environment,
                start_new_session=True,
            )
            try:
                deadline = time.monotonic() + 3.0
                while time.monotonic() < deadline:
                    if any(
                        record["pid"] == child.pid
                        for record in E2E.attributed_process_snapshot(run_id, runtime, ())
                    ):
                        break
                    time.sleep(0.05)
                cleanup = E2E.cleanup_attributed_processes(
                    run_id, runtime, (), baseline
                )
                child.wait(timeout=5.0)
                self.assertTrue(cleanup["absent"])
                self.assertTrue(
                    any(record["pid"] == child.pid for record in cleanup["initial"])
                )
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()

    def test_evidence_manifest_hashes_files_but_not_itself(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence = Path(directory)
            evidence.joinpath("journey.json").write_text("{}\n", encoding="utf-8")
            nested = evidence / "logs"
            nested.mkdir()
            nested.joinpath("console.txt").write_text("OK\n", encoding="utf-8")

            count = E2E.write_evidence_manifest(evidence)
            manifest = evidence.joinpath("evidence.sha256").read_text(encoding="utf-8")

            self.assertEqual(count, 2)
            self.assertIn("  journey.json\n", manifest)
            self.assertIn("  logs/console.txt\n", manifest)
            self.assertNotIn("evidence.sha256", manifest)

    def test_uorb_node_assertion_rejects_echo_only_and_accepts_device_listing(self) -> None:
        echoed = "ls -l /dev/uorb/sensor_hrate0\r\nNo such file or directory\r\n"
        listed = (
            "ls -l /dev/uorb/sensor_hrate0\r\n"
            " crw-rw-rw- 0 0 0 /dev/uorb/sensor_hrate0\r\n"
        )
        self.assertFalse(E2E.uorb_node_present(echoed))
        self.assertTrue(E2E.uorb_node_present(listed))


if __name__ == "__main__":
    unittest.main()
