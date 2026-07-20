#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import importlib.util
import signal
import sys
import threading
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "collect_q0_baseline.py"
SPEC = importlib.util.spec_from_file_location("q0_baseline", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
Q0 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(Q0)


class Q0BaselineTest(unittest.TestCase):
    def test_timeout_output_accepts_bytes(self) -> None:
        self.assertEqual(Q0.timeout_output(b"partial\xff"), "partial�")

    def test_nearest_rank_p95_uses_nineteenth_of_twenty_samples(self) -> None:
        self.assertEqual(Q0.percentile_nearest_rank(list(range(1, 21)), 0.95), 19)

    def test_startup_gate_requires_all_samples_and_minimum_count(self) -> None:
        samples = [
            {
                "status": "passed",
                "app_ui_ready_seconds": value,
                "boot_seconds": value + 0.5,
            }
            for value in (index / 100 for index in range(1, 21))
        ]
        summary = Q0.build_startup_summary(samples, 20, 0.5)
        self.assertTrue(summary["gate_passed"])
        self.assertEqual(summary["app_ui_ready"]["p50_seconds"], 0.105)
        self.assertEqual(summary["app_ui_ready"]["p95_seconds"], 0.19)

        samples[-1]["status"] = "failed"
        self.assertFalse(Q0.build_startup_summary(samples, 20, 0.5)["gate_passed"])
        with self.assertRaisesRegex(Q0.BaselineFailure, "at least 20"):
            Q0.build_startup_summary(samples, 1, 0.5)
        samples[-1] = {
            "status": "passed",
            "app_ui_ready_seconds": 0.0,
            "boot_seconds": 0.1,
        }
        self.assertFalse(Q0.build_startup_summary(samples, 20, 0.5)["gate_passed"])

    def test_formal_gate_arguments_cannot_be_downgraded(self) -> None:
        args = argparse.Namespace(
            runs=20,
            minimum_gate_runs=20,
            app_start_p95_budget=2.0,
            boot_timeout=240.0,
            command_timeout=30.0,
            run_timeout=360.0,
            settle_seconds=0.1,
            stability_seconds=0.1,
            console_port=5554,
            baseline_revision="1" * 40,
            artifact_source_revision="2" * 40,
            expected_nuttx_sha256="3" * 64,
        )
        Q0.validate_gate_arguments(args)
        for name, value in (
            ("runs", 1),
            ("minimum_gate_runs", 1),
            ("app_start_p95_budget", 2.1),
            ("run_timeout", float("nan")),
            ("console_port", True),
        ):
            changed = argparse.Namespace(**vars(args))
            setattr(changed, name, value)
            with self.assertRaises(Q0.BaselineFailure, msg=name):
                Q0.validate_gate_arguments(changed)

    def test_nonempty_evidence_directory_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence = Path(directory)
            evidence.joinpath("old-failure.log").write_text("keep\n", encoding="utf-8")
            with self.assertRaisesRegex(Q0.BaselineFailure, "not empty"):
                Q0.ensure_new_evidence_dir(evidence)

    def test_evidence_path_must_not_overlap_protected_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            protected = Path(directory) / "fixed-output"
            protected.mkdir()
            with self.assertRaisesRegex(Q0.BaselineFailure, "must not overlap"):
                Q0.require_disjoint_evidence_path(protected / "evidence", protected)
            with self.assertRaisesRegex(Q0.BaselineFailure, "must not overlap"):
                Q0.require_disjoint_evidence_path(protected.parent, protected)
            Q0.require_disjoint_evidence_path(Path(directory) / "sibling", protected)

    def test_evidence_manifest_uses_relative_paths_and_excludes_itself(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence = Path(directory)
            evidence.joinpath("top.txt").write_text("top\n", encoding="utf-8")
            nested = evidence / "nested"
            nested.mkdir()
            nested.joinpath("sample.json").write_text("{}\n", encoding="utf-8")

            self.assertEqual(Q0.write_evidence_manifest(evidence), 2)
            lines = evidence.joinpath("evidence.sha256").read_text(
                encoding="utf-8"
            ).splitlines()
            self.assertEqual(len(lines), 2)
            self.assertTrue(lines[0].endswith("  nested/sample.json"))
            self.assertTrue(lines[1].endswith("  top.txt"))
            self.assertNotIn("evidence.sha256", "\n".join(lines))

    def test_static_asset_inventory_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            assets = repo / "openvela_app" / "smart_band" / "assets"
            assets.mkdir(parents=True)
            assets.joinpath("face.png").write_bytes(b"png")
            repo.joinpath("openvela_app/smart_band/icon_assets.c").write_text(
                "const int icon = 1;\n", encoding="utf-8"
            )
            destination = repo / "assets.json"
            result = Q0.collect_static_assets(repo, destination)
            self.assertEqual(result["file_count"], 2)
            self.assertEqual(
                [item["path"] for item in result["files"]],
                [
                    "openvela_app/smart_band/assets/face.png",
                    "openvela_app/smart_band/icon_assets.c",
                ],
            )
            self.assertTrue(destination.is_file())

    def test_runtime_staging_isolates_every_input_and_rolls_back_partial_copy(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "fixed"
            runtime = base / "runtime"
            source.mkdir()
            for index, name in enumerate(Q0.RUNTIME_INPUTS):
                source.joinpath(name).write_bytes(f"input-{index}".encode())

            staging = Q0.stage_runtime_output(source, runtime)
            for name in Q0.RUNTIME_INPUTS:
                self.assertFalse(os.path.samefile(source / name, runtime / name))
                self.assertTrue(staging["files"][name]["isolated"])
            runtime.joinpath("vela_data.bin").write_bytes(b"changed")
            self.assertTrue(Q0.verify_staged_source_unchanged(staging))
            self.assertTrue(Q0.cleanup_runtime_inputs(runtime)["passed"])

            broken = base / "broken"
            source.joinpath("vela_data.bin").unlink()
            with self.assertRaises(Q0.BaselineFailure):
                Q0.stage_runtime_output(source, broken)
            self.assertFalse(broken.exists())

    def test_cleanup_log_contract_rejects_other_failure_text(self) -> None:
        self.assertTrue(
            Q0.smoke_cleanup_log_passed(
                "requested emulator shutdown\nemulator exit status: -15\n"
            )
        )
        self.assertFalse(
            Q0.smoke_cleanup_log_passed(
                "cleanup failed: detached helper remains\nemulator exit status: 0\n"
            )
        )

    def test_timeout_terminates_the_smoke_wrapper(self) -> None:
        returncode, _stdout, _stderr, timed_out, actions = Q0.run_command_with_timeout(
            [sys.executable, "-c", "import time; time.sleep(30)"], 0.05
        )
        self.assertTrue(timed_out)
        self.assertIsNotNone(returncode)
        self.assertTrue(actions)

    def test_gate_receipt_must_match_and_include_all_required_checks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "receipt.json"
            destination = base / "copied.json"
            receipt = {
                "schema_version": 1,
                "baseline_revision": "1" * 40,
                "artifact_source_revision": "2" * 40,
                "nuttx_sha256": "3" * 64,
                "collector_sha256": "a" * 64,
                "smoke_sha256": "b" * 64,
                "smart_band_tree_sha256": "4" * 64,
                "baseline_smart_band_git_tree": "5" * 40,
                "artifact_smart_band_git_tree": "5" * 40,
                "openvela_revisions": {
                    name: str(index) * 40
                    for index, name in enumerate(Q0.OPENVELA_REVISION_KEYS, 6)
                },
                "fixed_build": {
                    "run_id": 123,
                    "artifact_id": 456,
                    "conclusion": "success",
                    "head_sha": "2" * 40,
                    "artifact_digest": "sha256:" + "c" * 64,
                },
                "fixed_input_sha256": {
                    ".config": "d" * 64,
                    "nuttx": "3" * 64,
                    "vela_system.bin": "e" * 64,
                    "vela_data.bin": "f" * 64,
                },
                "checks": {name: True for name in Q0.REQUIRED_GATE_RECEIPT_CHECKS},
                "evidence": {
                    "host_actions": {
                        "run_id": 10,
                        "head_sha": "1" * 40,
                        "conclusion": "success",
                    },
                    "browser_actions": {
                        "run_id": 11,
                        "head_sha": "1" * 40,
                        "conclusion": "success",
                    },
                },
            }
            source.write_text(json.dumps(receipt), encoding="utf-8")
            loaded = Q0.load_gate_receipt(
                source,
                destination,
                "1" * 40,
                "2" * 40,
                "3" * 64,
                "a" * 64,
                "b" * 64,
            )
            self.assertTrue(all(loaded["required_checks"].values()))
            self.assertTrue(destination.is_file())

            receipt["checks"]["browser"] = False
            source.write_text(json.dumps(receipt), encoding="utf-8")
            with self.assertRaisesRegex(Q0.BaselineFailure, "browser"):
                Q0.load_gate_receipt(
                    source,
                    base / "rejected.json",
                    "1" * 40,
                    "2" * 40,
                    "3" * 64,
                    "a" * 64,
                    "b" * 64,
                )

            receipt["checks"]["browser"] = True
            receipt["fixed_build"]["head_sha"] = "9" * 40
            source.write_text(json.dumps(receipt), encoding="utf-8")
            with self.assertRaisesRegex(Q0.BaselineFailure, "fixed-build"):
                Q0.load_gate_receipt(
                    source,
                    base / "wrong-head.json",
                    "1" * 40,
                    "2" * 40,
                    "3" * 64,
                    "a" * 64,
                    "b" * 64,
                )

            receipt["fixed_build"]["head_sha"] = "2" * 40
            source.write_text(json.dumps(receipt), encoding="utf-8")
            with self.assertRaisesRegex(Q0.BaselineFailure, "smoke_sha256"):
                Q0.load_gate_receipt(
                    source,
                    base / "wrong-smoke.json",
                    "1" * 40,
                    "2" * 40,
                    "3" * 64,
                    "a" * 64,
                    "0" * 64,
                )

    def test_resource_gate_requires_hash_assets_and_symbol_anchors(self) -> None:
        expected = "a" * 64
        resources = {
            "required_tools_ok": True,
            "artifacts": {"nuttx": {"sha256": expected}},
            "static_assets": {"file_count": 1, "total_source_bytes": 10},
            "linker_map": None,
            "smart_band_symbols": {
                "anchors_present": True,
                "matched_symbols": 2,
                "summed_symbol_bytes": 3,
            },
        }
        self.assertTrue(Q0.resource_gate_passes(resources, expected))
        resources["smart_band_symbols"]["anchors_present"] = False
        resources["linker_map"] = {"bytes": 10, "anchor_present": True}
        self.assertFalse(Q0.resource_gate_passes(resources, expected))

    @unittest.skipUnless(os.name == "posix", "POSIX signal cleanup contract")
    def test_signal_interrupt_cleans_wrapper_and_staged_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "fixed"
            sample = base / "sample"
            source.mkdir()
            for name in Q0.RUNTIME_INPUTS:
                source.joinpath(name).write_bytes(b"input")
            fake_smoke = base / "fake-smoke.py"
            fake_smoke.write_text(
                "import time\ntime.sleep(30)\n", encoding="utf-8"
            )
            args = argparse.Namespace(
                console_port=5968,
                boot_timeout=30.0,
                command_timeout=30.0,
                settle_seconds=0.0,
                stability_seconds=0.0,
                run_timeout=30.0,
            )
            old_handler = signal.getsignal(signal.SIGINT)
            signal.signal(
                signal.SIGINT,
                lambda signum, frame: (_ for _ in ()).throw(
                    Q0.CollectionInterrupted("test SIGINT")
                ),
            )
            timer = threading.Timer(0.2, os.kill, args=(os.getpid(), signal.SIGINT))
            timer.start()
            try:
                with self.assertRaises(Q0.CollectionInterrupted):
                    Q0.run_smoke_sample(args, base, source, fake_smoke, sample, 1)
            finally:
                timer.cancel()
                signal.signal(signal.SIGINT, old_handler)
            self.assertFalse(sample.joinpath("runtime-output").exists())
            result = json.loads(
                sample.joinpath("collector-sample.json").read_text(encoding="utf-8")
            )
            self.assertEqual(result["status"], "failed")
            self.assertTrue(result["runtime_inputs_cleaned"])

    def test_failure_envelope_retains_machine_readable_failure_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence = Path(directory)
            try:
                raise Q0.BaselineFailure("expected failure")
            except Q0.BaselineFailure as exc:
                Q0.write_failure_envelope(evidence, exc)
            baseline = json.loads(
                evidence.joinpath("q0-baseline.json").read_text(encoding="utf-8")
            )
            self.assertEqual(baseline["status"], "failed")
            self.assertEqual(baseline["failure"]["message"], "expected failure")
            self.assertTrue(evidence.joinpath("evidence.sha256").is_file())


if __name__ == "__main__":
    unittest.main()
