"""Startup evidence and failure-retention tests; never launch the game."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import startup_stress as stress


class StartupStressTests(unittest.TestCase):
    def setUp(self):
        base = stress.replay.ROOT / ".test-tmp" / "startup-stress"
        base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def write_rows(self, path, rows):
        path.parent.mkdir(parents=True, exist_ok=True)
        stress.replay.write_jsonl(path, rows)

    def test_startup_matrix_has_twelve_distinct_bounded_cohorts(self):
        cohorts = stress.make_cohorts("startup-matrix")
        self.assertEqual(len(cohorts), 12)
        self.assertEqual(len({row["id"] for row in cohorts}), 12)
        self.assertEqual({(row["fixture"]["entrance"], row["fixture"]["presentation_fps"], row["trace"]) for row in cohorts},
                         {(scene, fps, trace) for scene in (187, 238) for fps in (20, 60, 120) for trace in (True, False)})
        for row in cohorts:
            self.assertEqual((row["fixture"]["setup_ticks"], row["fixture"]["ticks"]), (1, 1))
            self.assertNotIn("initial_player", row["fixture"])
            stress.replay.validate_fixture(row["fixture"])

    def test_full_gravity_recipe_retains_original_oracle_content(self):
        cohort, = stress.make_cohorts("full-gravity")
        original = stress.replay.read_json(stress.replay.FIXTURES / "gravity-fall.json")
        self.assertEqual(stress.replay.digest(cohort["fixture"]), stress.replay.digest(original))
        self.assertTrue(cohort["trace"])

    def test_round_robin_plan_counts_attempts_without_replacement(self):
        plan = stress.scheduled_attempts([{"id": "a"}, {"id": "b"}], 3)
        self.assertEqual([(row["cohort"], row["round"]) for row in plan],
                         [("a", 1), ("b", 1), ("a", 2), ("b", 2), ("a", 3), ("b", 3)])
        self.assertEqual([row["ordinal"] for row in plan], list(range(1, 7)))

    def test_scene_callback_alone_does_not_claim_completed_init_or_measurement(self):
        log = self.root / "work/logs/Ship of Harkinian.log"
        log.parent.mkdir(parents=True)
        log.write_text("Scene Init - sceneNum: 0x55, entranceIndex: 0xee\n", encoding="utf-8")
        result = stress.observe_attempt(self.root, False, False)
        self.assertTrue(result["scene_callback_observed"])
        self.assertIsNone(result["scene_initialization_completed"])
        self.assertIsNone(result["measured_replay_began"])

    def test_partial_trace_preserves_prefix_without_blessing_or_repair(self):
        path = self.root / "output/trace.jsonl"
        row = {"schema": 1, "tick": 0, "time_q": 0, "sequence": 0, "phase": "input_poll",
               "kind": "phase", "engine_frame": 1, "measuring": False}
        self.write_rows(path, [row])
        with path.open("a", encoding="utf-8") as stream:
            stream.write('{"schema":1,"tick":')
        original = path.read_bytes()
        result = stress.observe_attempt(self.root, True, False)
        self.assertTrue(result["scene_initialization_completed"])
        self.assertIsNone(result["measured_replay_began"])
        self.assertEqual(result["trace_progress"]["complete_records"], 1)
        self.assertEqual(result["trace_progress"]["invalid_record"]["line"], 2)
        self.assertEqual(path.read_bytes(), original)
        with self.assertRaises(stress.replay.ReplayError):
            stress.replay.load_trace(self.root / "output", 1)

    def test_initial_snapshot_confirms_measurement_even_with_trace_disabled(self):
        self.write_rows(self.root / "output/snapshots.jsonl", [
            {"schema": 1, "tick": 0, "time_q": 0, "player": {"action": "idle"}}])
        result = stress.observe_attempt(self.root, False, False)
        self.assertTrue(result["scene_initialization_completed"])
        self.assertTrue(result["measured_replay_began"])

    def test_crash_and_resource_evidence_is_retained_with_full_log_identity(self):
        path = self.root / "work/logs/Ship of Harkinian.log"
        path.parent.mkdir(parents=True)
        path.write_text("Using D3D adapter: unit-test\nReading archive: soot.o2r\n"
                        "[critical] Exception: 0xc0000005\nTraceback:\n  Fast::gfx_load_tlut_handler_rdp\n"
                        "Native simulation graphics diagnostics (addresses excluded from replay hashes):\n"
                        "  texture_resource=none (raw/segmented source)\n", encoding="utf-8")
        before = stress.replay.file_digest(path)
        result = stress.log_evidence(self.root)
        self.assertEqual(result["exception_codes"], ["0xc0000005"])
        self.assertTrue(result["tlut_handler_in_crash_log"])
        self.assertEqual(result["logs"][0]["sha256"], before)
        self.assertTrue(any("texture_resource=none" in row["text"] for row in result["crash_metadata"][0]["lines"]))
        self.assertEqual(stress.replay.file_digest(path), before)

    def test_timeout_is_saved_without_retry_or_discard(self):
        cohort = {"fixture": {"id": "unit", "ticks": 1, "presentation_fps": 20},
                  "trace": False, "fixture_path": str(self.root / "fixture.json")}
        directory = self.root / "attempt"
        def timeout(*args):
            stress.replay.write_json(directory / "invocation.json", {"status": "timeout", "host_seconds": 45})
            raise stress.replay.ReplayError("timeout")
        with mock.patch.object(stress.replay, "launch", side_effect=timeout) as launch:
            result = stress.run_attempt(self.root / "fake.exe", cohort, directory, {}, 45, None)
        self.assertEqual(launch.call_count, 1)
        self.assertEqual((result["status"], result["failure_kind"]), ("failed", "timeout"))
        self.assertTrue(result["process_start_confirmed"])
        self.assertTrue((directory / "startup.json").is_file())
        self.assertIsNone(result["observations"]["measured_replay_began"])

    def test_campaign_continues_after_failure_and_preserves_every_attempt(self):
        self.check_campaign_failure(fail_fast=False)

    def test_fail_fast_retains_first_failure_and_unlaunched_schedule(self):
        self.check_campaign_failure(fail_fast=True)

    def check_campaign_failure(self, fail_fast):
        executable = self.root / "fake.exe"
        executable.write_bytes(b"unit-test-placeholder-never-executed")
        assets = self.root / "assets"
        assets.mkdir()
        for name in ("oot.o2r", "soh.o2r"):
            (assets / name).write_bytes(b"unit-test-placeholder-not-game-data")
        cohorts = stress.make_cohorts("startup-matrix")[:2]
        for cohort in cohorts:
            cohort["trace"] = False
        inputs = {name: assets / name for name in ("oot.o2r", "soh.o2r")}
        provenance = {"executable": {"sha256": stress.replay.file_digest(executable)},
                      "assets": {name: {"sha256": stress.replay.file_digest(path)} for name, path in inputs.items()}}
        calls = []
        def launch(exe, fixture, directory, staged, timeout, trace):
            calls.append(directory)
            invocation = {"status": "exited", "exit_code": 7 if len(calls) == 1 else 0, "host_seconds": 0.01}
            stress.replay.write_json(directory / "invocation.json", invocation)
            if len(calls) == 1:
                raise stress.replay.ReplayError("deliberate unit-test failure")
            return invocation
        output = self.root / "campaign"
        with mock.patch.object(stress, "make_cohorts", return_value=cohorts), \
                mock.patch.object(stress.replay, "git_capture", return_value="a" * 40), \
                mock.patch.object(stress.replay, "provenance", return_value=provenance), \
                mock.patch.object(stress.replay, "launch", side_effect=launch), \
                mock.patch.object(stress.replay, "compare_runs", return_value={"status": "pass"}), \
                contextlib.redirect_stdout(io.StringIO()):
            code = stress.main(["--exe", str(executable), "--assets", str(assets), "--source-commit", "unit",
                                "--output", str(output), "--starts-per-cohort", "2"] +
                               (["--fail-fast"] if fail_fast else []))
        receipt = stress.replay.read_json(output / "stress_result.json")
        self.assertEqual(code, 2)
        self.assertEqual(receipt["status"], "incomplete" if fail_fast else "failures-observed")
        count = 1 if fail_fast else 4
        self.assertEqual(len(calls), count)
        self.assertEqual(len(set(calls)), count)
        self.assertEqual([row["status"] for row in receipt["attempts"]],
                         ["failed"] + (["scheduled"] * 3 if fail_fast else ["pass"] * 3))
        self.assertEqual(receipt["summary"]["total"]["process_starts_confirmed"], count)
        self.assertEqual(receipt["summary"]["total"]["failed_or_interrupted_attempts"], 1)
        self.assertEqual("stop_reason" in receipt, fail_fast)
        self.assertTrue(all((directory / "startup.json").is_file() for directory in calls))

    def test_changed_executable_or_asset_identity_stops_comparable_campaign(self):
        exe, asset, fixture = (self.root / name for name in ("fake.exe", "asset", "fixture.json"))
        for path in (exe, asset, fixture):
            path.write_bytes(b"original")
        provenance = {"executable": {"sha256": stress.replay.file_digest(exe)},
                      "assets": {"asset": {"sha256": stress.replay.file_digest(asset)}}}
        fixture_hash = stress.replay.file_digest(fixture)
        stress.verify_inputs(exe, {"asset": asset}, fixture, provenance, fixture_hash)
        asset.write_bytes(b"changed")
        with self.assertRaises(stress.replay.ReplayError):
            stress.verify_inputs(exe, {"asset": asset}, fixture, provenance, fixture_hash)


if __name__ == "__main__":
    unittest.main()
