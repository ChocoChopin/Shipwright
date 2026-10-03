"""Trust tests for the replay comparator, completion gate, and negative control."""
import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest

import run_corpus as replay


def float32(value):
    return {"bits": struct.pack("!f", value).hex(), "value": value}


def fixture():
    return {"schema": 1, "id": "unit-fixture", "category": "harness", "ticks": 4,
            "seed": 10, "entrance": 187, "age": 1, "input": [
                {"time_num": 0, "time_den": 1, "sequence": 0, "buttons": 0, "stick_x": 0, "stick_y": 0}]}


class CanonicalComparisonTests(unittest.TestCase):
    def test_hash_ignores_object_key_order_but_not_array_order(self):
        self.assertEqual(replay.digest({"a": 1, "b": 2}), replay.digest({"b": 2, "a": 1}))
        self.assertNotEqual(replay.digest([1, 2]), replay.digest([2, 1]))

    def test_negative_zero_and_bit_change_are_exact(self):
        self.assertNotEqual(replay.digest(float32(-0.0)), replay.digest(float32(0.0)))
        value = {"player": {"position": float32(1.0)}}
        altered = copy.deepcopy(value)
        mutation = replay.perturb_float(altered)
        self.assertEqual(mutation["field"], "$.player.position")
        self.assertNotEqual(replay.digest(value), replay.digest(altered))
        self.assertEqual(replay.first_differences(value, altered)[0]["field"], "$.player.position.bits")

    def test_integer_enum_boolean_and_missing_fields_are_not_coerced(self):
        for a, b in ((1, True), (1, 1.0), ({"mode": 0}, {})):
            self.assertTrue(replay.first_differences(a, b))

    def test_nonfinite_decimal_or_ieee_bits_are_rejected(self):
        for value in (float("nan"), float("inf"), {"bits": "7fc00000", "value": 0},
                      {"bits": "7f800000", "value": 0}, {"bits": "3f800000", "value": 2}):
            with self.assertRaises(replay.ReplayError):
                replay.digest(value)

    def test_duplicate_json_keys_and_nonstandard_numbers_are_rejected(self):
        for document in ('{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}'):
            with self.assertRaises(replay.ReplayError):
                replay.strict_json(document)

    def test_array_lifecycle_order_produces_first_actor_field(self):
        left = {"actors": [{"id": "scene0:spawn1"}, {"id": "scene0:spawn2"}]}
        right = {"actors": list(reversed(left["actors"]))}
        self.assertEqual(replay.first_differences(left, right)[0]["field"], "$.actors[0].id")


class FixtureTests(unittest.TestCase):
    def test_all_committed_fixtures_are_valid(self):
        paths = list(replay.FIXTURES.rglob("*.json"))
        self.assertGreaterEqual(len(paths), 7)
        for path in paths:
            with self.subTest(fixture=path.name):
                replay.validate_fixture(replay.read_json(path))

    def test_nonlattice_input_times_remain_exact(self):
        data = fixture()
        data["input"].append({"time_num": 1, "time_den": 100, "sequence": 1,
                              "buttons": 32768, "stick_x": 0, "stick_y": 0})
        self.assertEqual(replay.validate_fixture(data)["input"][1]["time_den"], 100)

    def test_engine_receipt_must_attest_requested_seed_input_and_configuration(self):
        requested = fixture()
        receipt = {"fixture_id": requested["id"], "ticks_completed": requested["ticks"],
                   "fixture": copy.deepcopy(requested), "setup_ticks": 60,
                   "configuration": {"interpolation_fps": 20, "match_refresh_rate": 0, "mouse": 0, "time_sync": 0}}
        replay.validate_requested_fixture(receipt, requested)
        for altered in (dict(receipt, fixture=dict(requested, seed=11)),
                        dict(receipt, fixture=dict(requested, input=[])),
                        dict(receipt, setup_ticks=61), dict(receipt, configuration={})):
            with self.subTest(receipt=altered), self.assertRaises(replay.ReplayError):
                replay.validate_requested_fixture(altered, requested)

    def test_higher_simulation_rate_rejected(self):
        for rate in (30, 60, 120, 20.0, True):
            data = fixture()
            data["rate_hz"] = rate
            with self.assertRaises(replay.ReplayError):
                replay.validate_fixture(data)

    def test_message_and_actor_recipes_require_exact_supported_types(self):
        for key, value in (("message_text_id", -1), ("message_text_id", 65536),
                           ("message_text_id", 12383.0), ("spawn_ice_keese", 1),
                           ("observe_draw_state", 1), ("observe_draw_state", "true"),
                           ("observe_player_state", 1), ("spawn_cuttable_sign", "true")):
            data = fixture()
            data[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(replay.ReplayError):
                replay.validate_fixture(data)

    def test_sign_recipe_requires_observation(self):
        data = fixture()
        data["spawn_cuttable_sign"] = True
        with self.assertRaises(replay.ReplayError):
            replay.validate_fixture(data)
        data["observe_player_state"] = True
        replay.validate_fixture(data)

    def test_duplicate_or_out_of_order_event_identity_rejected(self):
        data = fixture()
        data["input"] += [copy.deepcopy(data["input"][0])]
        with self.assertRaises(replay.ReplayError):
            replay.validate_fixture(data)

    def test_input_at_final_endpoint_cannot_silently_disappear(self):
        data = fixture()
        data["input"].append({"time_num": 1, "time_den": 5, "sequence": 1,
                              "buttons": 0, "stick_x": 0, "stick_y": 0})
        with self.assertRaises(replay.ReplayError):
            replay.validate_fixture(data)

    def test_identical_frozen_world_cannot_pass_motion_fixture(self):
        data = fixture()
        data["assertions"] = [{"kind": "distance", "field": "player.position", "minimum": 1},
                              {"kind": "counter_advance", "field": "global.gameplay_frames", "amount": 4}]
        frozen = [{"tick": tick, "player": {"position": {axis: float32(0.0) for axis in "xyz"}},
                   "global": {"gameplay_frames": 60}} for tick in range(5)]
        report = replay.fixture_assertions(data, frozen)
        self.assertEqual(report["status"], "fail")
        self.assertTrue(all(item["status"] == "fail" for item in report["assertions"]))

    def test_input_pulse_assertion_requires_both_edges_once(self):
        data = fixture()
        data["assertions"] = [{"kind": "input", "tick": 2, "held": 0, "pressed": 32768, "released": 32768}]
        rows = [{"input": {"held": 0, "pressed": 0, "released": 0}} for _ in range(5)]
        self.assertEqual(replay.fixture_assertions(data, rows)["status"], "fail")
        rows[2]["input"].update(pressed=32768, released=32768)
        self.assertEqual(replay.fixture_assertions(data, rows)["status"], "pass")


class CompletionTests(unittest.TestCase):
    def setUp(self):
        base = replay.ROOT / ".test-tmp" / "native-replay"
        base.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=base)
        self.directory = Path(self.temporary.name)
        self.reference = self.directory / "reference"
        self.candidate = self.directory / "candidate"
        self.reference.mkdir()
        self.candidate.mkdir()
        self.rows = [{"schema": 1, "tick": tick, "time_q": tick * 6,
                      "player": {"position": {"x": float32(float(tick))}, "health": 48}}
                     for tick in range(5)]
        self.manifest = {"schema": 1, "status": "pass", "fixture_id": "unit-fixture", "ticks_completed": 4, "rate_hz": 20}
        for directory in (self.reference, self.candidate):
            replay.write_json(directory / "result.json", self.manifest)
            replay.write_jsonl(directory / "snapshots.jsonl", self.rows)

    def tearDown(self):
        self.temporary.cleanup()

    def test_complete_equal_run_passes_every_checkpoint(self):
        report = replay.compare_runs(self.reference, self.candidate)
        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["snapshots_compared"], 5)

    def test_first_divergence_reports_exact_tick_and_field(self):
        modified = copy.deepcopy(self.rows)
        modified[2]["player"]["health"] -= 1
        replay.write_jsonl(self.candidate / "snapshots.jsonl", modified)
        report = replay.compare_runs(self.reference, self.candidate)
        self.assertEqual((report["status"], report["tick"], report["time_q"]), ("mismatch", 2, 12))
        self.assertEqual(report["differences"][0]["field"], "$.player.health")

    def test_manifest_is_required_even_if_rows_exist(self):
        (self.candidate / "result.json").unlink()
        with self.assertRaisesRegex(replay.ReplayError, "missing completion"):
            replay.load_run(self.candidate)

    def test_partial_missing_or_mistimed_records_fail_closed(self):
        for bad_rows in (self.rows[:-1], [self.rows[0], *self.rows[2:]],
                         [dict(row, time_q=row["time_q"] + 1) for row in self.rows]):
            replay.write_jsonl(self.candidate / "snapshots.jsonl", bad_rows)
            with self.assertRaises(replay.ReplayError):
                replay.load_run(self.candidate)

    def test_failed_manifest_cannot_bless_complete_snapshots(self):
        replay.write_json(self.candidate / "result.json", dict(self.manifest, status="failed"))
        with self.assertRaises(replay.ReplayError):
            replay.load_run(self.candidate)

    def test_unknown_actions_or_actor_identities_cannot_pass_exact_repeat(self):
        unknown_action = copy.deepcopy(self.rows)
        unknown_action[2]["player"]["action"] = "unmapped"
        unknown_actor = copy.deepcopy(self.rows)
        unknown_actor[2]["actors"] = [{"identity": "untracked", "coverage": "base_actor"}]
        duplicate_actor = copy.deepcopy(self.rows)
        duplicate_actor[2]["actors"] = [{"identity": "scene1:spawn2"}, {"identity": "scene1:spawn2"}]
        for rows in (unknown_action, unknown_actor, duplicate_actor):
            for directory in (self.reference, self.candidate):
                replay.write_jsonl(directory / "snapshots.jsonl", rows)
            with self.assertRaises(replay.ReplayError):
                replay.compare_runs(self.reference, self.candidate)

    def test_player_repeated_in_base_actor_list_is_not_duplicate_identity(self):
        rows = copy.deepcopy(self.rows)
        for row in rows:
            row["player"].update(identity="scene1:spawn1", action="Player_Idle", animation={"resource": "unmapped"})
            row["actors"] = [{"identity": "scene1:spawn1", "coverage": "base_actor"}]
        replay.write_jsonl(self.candidate / "snapshots.jsonl", rows)
        self.assertEqual(len(replay.load_run(self.candidate)[1]), 5)

    def test_trace_order_mismatch_detected_with_same_snapshot_hashes(self):
        trace = [{"schema": 1, "tick": 1, "time_q": 6, "phase": "rng", "sequence": index, "call": index} for index in (0, 1)]
        replay.write_jsonl(self.reference / "trace.jsonl", trace)
        reordered = [dict(record, sequence=index) for index, record in enumerate(reversed(trace))]
        replay.write_jsonl(self.candidate / "trace.jsonl", reordered)
        report = replay.compare_runs(self.reference, self.candidate, include_trace=True)
        self.assertEqual((report["status"], report["domain"], report["tick"]), ("mismatch", "trace", 1))

    def test_empty_or_missing_trace_records_cannot_pass_verbose_gate(self):
        for trace in ([], [{"schema": 1, "sequence": 1, "tick": 0, "time_q": 0, "phase": "update"}]):
            replay.write_jsonl(self.reference / "trace.jsonl", trace)
            with self.assertRaises(replay.ReplayError):
                replay.load_trace(self.reference, 4)

    def test_host_log_contamination_is_rejected_and_preserved(self):
        trace = self.reference / "trace.jsonl"
        contaminated = ('[2026-10-02 23:03:15.194] [info] host startup log\n'
                        '{"schema":1,"sequence":0,"tick":0,"time_q":0,"phase":"initialization"}\n')
        trace.write_text(contaminated, encoding="utf-8", newline="\n")
        with self.assertRaises(replay.ReplayError):
            replay.load_trace(self.reference, 4)
        self.assertEqual(trace.read_text(encoding="utf-8"), contaminated)

    def test_common_checkpoints_are_real_tenth_second_states(self):
        result = replay.hashes_for_run(self.reference)
        common = [row["time_q"] for row in result["snapshots"] if row["common_100ms_checkpoint"]]
        self.assertEqual(common, [0, 12, 24])
        self.assertNotIn(6, common)

    def test_different_effective_configurations_are_incomparable(self):
        replay.write_json(self.reference / "result.json", dict(self.manifest, configuration={"seed": 1}))
        replay.write_json(self.candidate / "result.json", dict(self.manifest, configuration={"seed": 2}))
        with self.assertRaisesRegex(replay.ReplayError, "configuration differs"):
            replay.compare_runs(self.reference, self.candidate)

    def test_presentation_variant_allows_only_two_declared_input_fields(self):
        for directory, fps in ((self.reference, 20), (self.candidate, 120)):
            replay.write_json(directory / "result.json", dict(self.manifest,
                configuration={"interpolation_fps": fps, "time_sync": 0}, fixture={"seed": 42, "presentation_fps": fps}))
        self.assertEqual(replay.compare_runs(self.reference, self.candidate, allow_presentation_difference=True)["status"], "pass")
        manifest = replay.read_json(self.candidate / "result.json")
        manifest["fixture"]["seed"] = 43
        replay.write_json(self.candidate / "result.json", manifest)
        with self.assertRaisesRegex(replay.ReplayError, "fixture differs"):
            replay.compare_runs(self.reference, self.candidate, allow_presentation_difference=True)

    def test_cli_negative_control_fails_then_restored_copy_passes(self):
        # Executes the real comparator command in fresh Python subprocesses.
        import argparse
        import contextlib
        import io
        destination = self.directory / "negative"
        with contextlib.redirect_stdout(io.StringIO()):
            status = replay.negative_test(argparse.Namespace(run=self.reference, output=destination, tick=2))
        self.assertEqual(status, 0)
        report = replay.read_json(destination / "negative-test.json")
        self.assertEqual((report["negative_exit_code"], report["restored_exit_code"]), (1, 0))
        self.assertEqual(report["negative_comparison"]["tick"], 2)
        self.assertEqual(replay.load_run(destination)[1], self.rows)


if __name__ == "__main__":
    unittest.main()
