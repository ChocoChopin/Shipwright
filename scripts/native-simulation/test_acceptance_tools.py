"""Negative controls for evidence completeness and matrix asset identity."""
import copy
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import analyze_couplings as coupling
import presentation_matrix as matrix


class CouplingEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.manifest = {"ticks_completed": 1, "fixture_id": "unit-coupling", "fixture": {}, "coverage": "unit"}
        initial = {"rng": {name: {"calls": 0, "state": 0} for name in ("gameplay", "audio-context", "events")},
                   "audio": {"blocks": 0, "samples": 0, "clock_calls": 0, "random": 0,
                             "task_count": 0, "memory_notes": []},
                   "actors": [], "player": {"action": "idle"}}
        final = copy.deepcopy(initial)
        final["rng"]["gameplay"]["calls"] = 1
        final["rng"]["audio-context"]["calls"] = 3
        final["rng"]["events"]["calls"] = 7
        final["audio"].update(blocks=3, samples=1584, clock_calls=3, task_count=3)
        self.snapshots = [initial, final]
        self.events = [{"kind": "rng", "stream": "gameplay", "site": "Rand_ZeroOne", "actor": None,
                        "phase": "update_begin", "tick": 0, "measuring": True},
                       {"kind": "rng-draw", "phase": "update_begin", "tick": 0, "measuring": True}]
        self.events.extend(self.audio_events(0))

    @staticmethod
    def audio_events(tick):
        return [dict(event, tick=tick, phase="audio.begin", measuring=True) for _ in range(3) for event in (
            {"kind": "audio_block", "value": 528},
            {"kind": "rng", "stream": "audio-context", "site": "audio", "actor": None},
            {"kind": "rng-draw"},
        )]

    def observe(self):
        with mock.patch.object(coupling.runner, "load_run", return_value=(self.manifest, self.snapshots)), \
                mock.patch.object(coupling.runner, "load_trace", return_value=self.events), \
                mock.patch.object(Path, "exists", return_value=False):
            return coupling.observe(Path("unused-unit-output"))

    def test_consistent_rng_and_aggregate_event_counts_pass(self):
        self.assertEqual(self.observe()["status"], "pass")

    def test_entire_missing_rng_stream_cannot_hide_positive_snapshot_delta(self):
        self.events = [event for event in self.events if event.get("stream") != "gameplay"]
        result = self.observe()
        self.assertEqual(result["status"], "measurement-failure")
        self.assertFalse(result["checks"]["observed_rng_counters_match_trace"])

    def test_missing_authoritative_event_is_detected_independently_of_rng(self):
        self.events.pop(1)
        result = self.observe()
        self.assertTrue(result["checks"]["observed_rng_counters_match_trace"])
        self.assertFalse(result["checks"]["authoritative_event_counter_matches_trace"])

    def test_extra_audio_tick_fails_even_when_all_snapshot_counters_match(self):
        self.events.extend(self.audio_events(1))
        final = self.snapshots[-1]
        final["rng"]["audio-context"]["calls"] = 6
        final["rng"]["events"]["calls"] = 13
        final["audio"].update(blocks=6, samples=3168, clock_calls=6, task_count=6)
        result = self.observe()
        self.assertEqual(result["status"], "measurement-failure")
        self.assertEqual([name for name, passed in result["checks"].items() if not passed],
                         ["three_blocks_each_measured_tick"])


class PresentationEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.assets = {"oot.o2r": {"path": "reference/oot.o2r", "bytes": 12, "sha256": "a" * 64},
                       "soh.o2r": {"path": "reference/soh.o2r", "bytes": 34, "sha256": "b" * 64}}

    def test_fresh_asset_paths_preserve_identity(self):
        candidate = copy.deepcopy(self.assets)
        candidate["oot.o2r"]["path"] = "another-work-directory/oot.o2r"
        matrix.check_case_assets(candidate, self.assets)

    def test_changed_missing_or_added_case_assets_cannot_pass(self):
        changed = copy.deepcopy(self.assets)
        changed["oot.o2r"]["sha256"] = "c" * 64
        missing = copy.deepcopy(self.assets)
        del missing["soh.o2r"]
        added = dict(self.assets, extra={"bytes": 1, "sha256": "d" * 64})
        wrong_size = copy.deepcopy(self.assets)
        wrong_size["soh.o2r"]["bytes"] += 1
        for candidate in (changed, missing, added, wrong_size, None):
            with self.subTest(candidate=candidate), self.assertRaises(matrix.replay.ReplayError):
                matrix.check_case_assets(candidate, self.assets)

    def test_final_asset_rehash_detects_changes_after_last_case(self):
        with mock.patch.object(matrix.replay, "file_digest", return_value="c" * 64), \
                self.assertRaises(matrix.replay.ReplayError):
            matrix.check_asset_files(Path("unused-unit-assets"), self.assets)

    def test_between_case_executable_change_is_rejected_even_with_original_assets(self):
        candidate = {"executable": {"sha256": "c" * 64}, "assets": self.assets}
        matrix.check_case_provenance(candidate, "c" * 64, self.assets)
        with self.assertRaises(matrix.replay.ReplayError):
            matrix.check_case_provenance(candidate, "d" * 64, self.assets)

    def test_presentation_counts_require_every_tick_once_at_requested_ratio(self):
        rows = [{"kind": "presentation-count", "measuring": True, "tick": tick, "value": 3}
                for tick in range(4)]
        with mock.patch.object(matrix.replay, "load_trace", return_value=rows):
            self.assertEqual(matrix.check_presentation_events(Path("unused"), 4, 60)["status"], "pass")
        for invalid in (rows[:-1], rows + [rows[-1]], [dict(row, value=6) for row in rows]):
            with self.subTest(rows=invalid), mock.patch.object(matrix.replay, "load_trace", return_value=invalid):
                self.assertEqual(matrix.check_presentation_events(Path("unused"), 4, 60)["status"], "mismatch")


class PresentationSelectorTests(unittest.TestCase):
    def setUp(self):
        base = matrix.replay.ROOT / ".test-tmp" / "presentation-selectors"
        base.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.temporary.name)
        self.fixture = matrix.replay.read_json(matrix.replay.FIXTURES / "hud-countdown.json")

    def tearDown(self):
        self.temporary.cleanup()

    def write_fixture(self, name, value):
        path = self.root / name
        matrix.replay.write_json(path, value)
        return path

    def test_omitted_selectors_preserve_seven_original_cases(self):
        presentation, off = matrix.fixture_selections()
        self.assertEqual(tuple(presentation), matrix.PRESENTATION_FIXTURES)
        self.assertEqual(tuple(off), ("startup-idle",))
        self.assertEqual(matrix.matrix_cases(presentation, off, [60, 120]),
                         [(name, fps, True) for fps in (60, 120) for name in matrix.PRESENTATION_FIXTURES] +
                         [("startup-idle", 20, False)])

    def test_nested_selector_uses_content_identity_and_can_cover_all_rates_and_trace_off(self):
        path = self.write_fixture("draw-state/arbitrary-filename.json", self.fixture)
        presentation, off = matrix.fixture_selections([path], [path])
        self.assertEqual(tuple(presentation), (self.fixture["id"],))
        self.assertEqual(presentation[self.fixture["id"]]["path"], str(path.resolve()))
        self.assertEqual(matrix.matrix_cases(presentation, off, [20, 60, 120]),
                         [(self.fixture["id"], fps, True) for fps in (20, 60, 120)] + [(self.fixture["id"], 20, False)])

    def test_duplicate_id_in_one_selector_group_is_rejected_before_case_overwrite(self):
        first = self.write_fixture("one.json", self.fixture)
        second = self.write_fixture("nested/two.json", self.fixture)
        with self.assertRaises(matrix.replay.ReplayError):
            matrix.fixture_selections([first, second], [first])

    def test_same_id_with_conflicting_recipes_across_groups_is_rejected(self):
        first = self.write_fixture("one.json", self.fixture)
        changed = dict(self.fixture, seed=self.fixture["seed"] + 1)
        second = self.write_fixture("two.json", changed)
        with self.assertRaises(matrix.replay.ReplayError):
            matrix.fixture_selections([first], [second])

    def test_reference_requires_exact_recipe_and_canonical_presentation(self):
        matrix.check_reference_fixture(self.fixture, copy.deepcopy(self.fixture))
        for changed in (dict(self.fixture, seed=self.fixture["seed"] + 1), dict(self.fixture, presentation_fps=60)):
            with self.subTest(changed=changed), self.assertRaises(matrix.replay.ReplayError):
                matrix.check_reference_fixture(self.fixture, changed)
        noncanonical = dict(self.fixture, presentation_fps=60)
        with self.assertRaises(matrix.replay.ReplayError):
            matrix.check_reference_fixture(noncanonical, noncanonical)

    def test_duplicate_or_unadmitted_rate_cannot_overwrite_or_expand_cases(self):
        for rates in ([60, 60], [30], [True], []):
            with self.subTest(rates=rates), self.assertRaises(matrix.replay.ReplayError):
                matrix.matrix_cases({"fixture": {}}, {"fixture": {}}, rates)


if __name__ == "__main__":
    unittest.main()
