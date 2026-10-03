"""Trust tests for draw-state evidence: corruptions must not become acceptance."""
import copy
from pathlib import Path
import unittest

import analyze_draw_state as analysis


def row():
    return {"global": {"timer_state": 5, "timer_seconds": 1, "subtimer_state": 0, "state_frames": 60},
            "hud": {"main_next_second": 0, "main_state_timer": 0, "sub_next_second": 0,
                    "sub_state_timer": 0, "digits": [0] * 5, "timer_x": [140, 0], "timer_y": [80, 0],
                    "env_hazard": 0, "env_hazard_active": 0, "timer_gate_open": 1,
                    "countdown_length_gate_open": 1, "no_ui": 0, "pause_debug_state": 0, "game_mode": 0,
                    "health_capacity": 224},
            "message": {}, "input": {},
            "player": {"action": "idle", "melee_state": 0, "melee_animation": 0, "state_flags": [0, 0, 0]}}


def provenance():
    def identity(name):
        return {"path": str(Path("build").resolve() / name), "bytes": 1234, "sha256": "a" * 64}
    return {"source_head": "b" * 40, "tracked_diff_sha256": "c" * 64, "branch": "mod/native-simulation-rates",
            "executable": identity("soh.exe"), "assets": {name: identity(name) for name in ("oot.o2r", "soh.o2r")}}


def synthetic_hud(seconds=1):
    fixture = {"id": "hud-zero" if seconds == 1 else "hud-warning", "ticks": 64 if seconds == 1 else 120,
               "hud_timer_seconds": seconds}
    current = row()
    current["global"]["timer_seconds"] = seconds
    frames, events = {}, []
    for tick in range(fixture["ticks"]):
        before = copy.deepcopy(current)
        expected, sounds = analysis.timer_step(before)
        current["global"].update(timer_state=expected.pop("state"), timer_seconds=expected.pop("seconds"))
        current["hud"].update(expected)
        for phase in ("update_begin", "draw.interface.begin"):
            frames[tick, phase] = copy.deepcopy(before)
        for phase in ("draw.interface.end", "transaction_end"):
            frames[tick, phase] = copy.deepcopy(current)
        for value in sounds:
            events.extend({"tick": tick, "value": value, "kind": kind, "phase": "draw.interface.begin", "measuring": True}
                          for kind in ("audio-sfx-request", "audio-sfx-queued"))
    return fixture, [{}, {"global": current["global"]}], events, frames


class TimerEvidenceTests(unittest.TestCase):
    def test_canonical_zero_keeps_stop_then_off_and_old_digits(self):
        fixture, snapshots, events, frames = synthetic_hud()
        result = analysis.check_hud(fixture, snapshots, events, frames)
        self.assertEqual(result["stop_ticks"], [40])
        self.assertEqual(frames[40, "draw.interface.end"]["hud"]["digits"], [0, 0, 10, 0, 1])
        self.assertEqual(frames[41, "draw.interface.end"]["global"]["timer_state"], 0)

    def test_wrong_counter_reload_is_rejected(self):
        args = synthetic_hud()
        args[3][40, "draw.interface.end"]["hud"]["main_next_second"] = 19
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_hud(*args)

    def test_new_zero_digits_cannot_replace_legacy_stale_digits(self):
        args = synthetic_hud()
        args[3][40, "draw.interface.end"]["hud"]["digits"][4] = 0
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_hud(*args)

    def test_wrong_warning_value_phase_missing_and_duplicate_are_rejected(self):
        original = synthetic_hud(12)
        self.assertEqual(analysis.check_hud(*original)["sound_requests"], [(60, 0x481A), (80, 0x481A), (100, 0x481A)])
        for change in ("value", "phase", "missing", "duplicate"):
            args = copy.deepcopy(original)
            if change == "value": args[2][0]["value"] = 0x4819
            if change == "phase": args[2][0]["phase"] = "update_begin"
            if change == "missing": args[2].pop(0)
            if change == "duplicate": args[2].append(copy.deepcopy(args[2][0]))
            with self.subTest(change=change), self.assertRaises(analysis.EvidenceError):
                analysis.check_hud(*args)

    def test_gate_freezes_all_timer_fields(self):
        before = row()
        before["hud"]["timer_gate_open"] = 0
        after, sounds = analysis.timer_step(before)
        self.assertEqual(after, analysis.timer_values(before))
        self.assertEqual(sounds, [])

    def test_old_digit_selects_over_one_minute_warning(self):
        args = synthetic_hud(72)
        self.assertEqual(analysis.check_hud(*args)["sound_requests"], [(60, 0x4804)])
        args[2][0]["tick"] = 40
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_hud(*args)

    def test_timer_target_tracks_recorded_health_capacity(self):
        before = row()
        before["global"]["timer_state"] = 8
        before["hud"].update(main_next_second=20, main_state_timer=20, health_capacity=0xA0)
        self.assertEqual(analysis.timer_step(before)[0]["timer_y"][0], 46)
        before["hud"]["health_capacity"] += 1
        self.assertEqual(analysis.timer_step(before)[0]["timer_y"][0], 54)

    @staticmethod
    def input_boundary():
        fixture, snapshots, events, frames = synthetic_hud()
        fixture["id"] = "hud-zero-input"
        final_global = snapshots[-1]["global"]
        snapshots = [{"global": final_global, "player": {"upper_animation": {"resource": "idle"}}}
                     for tick in range(65)]
        snapshots[44]["player"]["upper_animation"]["resource"] = (
            "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_fighter2free")
        frames[41, "update_end"] = copy.deepcopy(frames[41, "update_begin"])
        return fixture, snapshots, events, frames

    def test_stop_cannot_allow_early_sword_equipping(self):
        args = self.input_boundary()
        analysis.check_hud(*args)
        args[1][42]["player"]["upper_animation"]["resource"] = args[1][44]["player"]["upper_animation"]["resource"]
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_hud(*args)

    def test_off_requires_actual_sword_equipping_not_idle_or_unrelated_motion(self):
        for resource in ("idle", "unrelated-motion"):
            args = self.input_boundary()
            args[1][44]["player"]["upper_animation"]["resource"] = resource
            with self.subTest(resource=resource), self.assertRaises(analysis.EvidenceError):
                analysis.check_hud(*args)


class CompletenessTests(unittest.TestCase):
    def setUp(self):
        self.root = Path("corpus").resolve()
        self.receipt = {"schema": 1, "status": "pass", "rate_hz": 20, "repeats": 3,
                        "output": str(self.root), "provenance": provenance(), "fixtures": [
            {"id": name, "status": "pass", "runs": [{"status": "pass", "exit_code": 0}] * 3}
            for name in sorted(analysis.EXPECTED)]}
        self.paths = [self.root / name / f"run-{repeat:03d}" / "output"
                      for name in analysis.EXPECTED for repeat in range(1, 4)]

    def test_exact_complete_set_passes(self):
        analysis.check_corpus_set(self.root, self.receipt, self.paths)

    def test_missing_or_extra_output_cannot_pass(self):
        for paths in (self.paths[:-1], self.paths + [self.root / "hud-zero/run-004/output"]):
            with self.subTest(paths=len(paths)), self.assertRaises(analysis.EvidenceError):
                analysis.check_corpus_set(self.root, self.receipt, paths)

    def test_missing_fixture_failed_invocation_and_repeat_count_cannot_pass(self):
        for change in ("fixture", "invocation", "repeat"):
            receipt = copy.deepcopy(self.receipt)
            if change == "fixture": receipt["fixtures"].pop()
            if change == "invocation": receipt["fixtures"][0]["runs"][0]["status"] = "timeout"
            if change == "repeat": receipt["repeats"] = 2
            with self.subTest(change=change), self.assertRaises(analysis.EvidenceError):
                analysis.check_corpus_set(self.root, receipt, self.paths)

    def test_missing_or_malformed_provenance_cannot_pass(self):
        for change in ("missing", "hash", "size", "path", "asset", "source"):
            receipt = copy.deepcopy(self.receipt)
            p = receipt["provenance"]
            if change == "missing": receipt["provenance"] = None
            if change == "hash": p["executable"]["sha256"] = "wrong"
            if change == "size": p["executable"]["bytes"] = True
            if change == "path": p["executable"]["path"] = "relative.exe"
            if change == "asset": p["assets"].pop("oot.o2r")
            if change == "source": p["source_head"] = None
            with self.subTest(change=change), self.assertRaises(analysis.EvidenceError):
                analysis.check_corpus_set(self.root, receipt, self.paths)

    def invocation(self):
        directory = self.root / "hud-zero" / "run-001"
        return {"command": [self.receipt["provenance"]["executable"]["path"], "--native-sim-test",
                            str(self.root / "hud-zero" / "fixture.json"), "--output", str(directory / "output"), "--trace"],
                "cwd": str(directory / "work"), "fixture_sha256": "d" * 64, "status": "pass", "exit_code": 0,
                "asset_staging": {"oot.o2r": "hardlink", "soh.o2r": "copy"}, "snapshot_count": 65,
                "sequence_sha256": "e" * 64}

    def test_invocation_uses_recorded_executable_without_requiring_it_to_exist(self):
        analysis.check_invocation(self.root, "hud-zero", 1, self.receipt["provenance"], self.invocation(), "d" * 64)

    def test_purity_invocation_requires_declared_mode(self):
        invocation = self.invocation()
        invocation["command"].append("--verify-presentation-purity")
        analysis.check_invocation(self.root, "hud-zero", 1, self.receipt["provenance"], invocation, "d" * 64, True)
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_invocation(self.root, "hud-zero", 1, self.receipt["provenance"], invocation, "d" * 64)

    def test_purity_mode_rejects_missing_extra_flags_and_nonboolean_declaration(self):
        for flags, mode in (([], True), (["--verify-presentation-purity"] * 2, True),
                            (["--verify-presentation-purity", "--presentation-purity-negative-control"], True),
                            (["--verify-presentation-purity"], 1)):
            invocation = self.invocation()
            invocation["command"].extend(flags)
            with self.subTest(flags=flags, mode=mode), self.assertRaises(analysis.EvidenceError):
                analysis.check_invocation(self.root, "hud-zero", 1, self.receipt["provenance"], invocation, "d" * 64, mode)

    def test_swapped_invocation_or_different_launch_context_cannot_pass(self):
        for change in ("executable", "fixture", "output", "cwd", "trace", "fixture_hash", "asset", "exit"):
            invocation = self.invocation()
            if change == "executable": invocation["command"][0] = str(self.root / "other.exe")
            if change == "fixture": invocation["command"][2] = str(self.root / "hud-warning" / "fixture.json")
            if change == "output": invocation["command"][4] = str(self.root / "hud-zero" / "run-002" / "output")
            if change == "cwd": invocation["cwd"] = str(self.root / "hud-zero" / "run-002" / "work")
            if change == "trace": invocation["command"].pop()
            if change == "fixture_hash": invocation["fixture_sha256"] = "f" * 64
            if change == "asset": invocation["asset_staging"].pop("oot.o2r")
            if change == "exit": invocation["exit_code"] = False
            with self.subTest(change=change), self.assertRaises(analysis.EvidenceError):
                analysis.check_invocation(self.root, "hud-zero", 1, self.receipt["provenance"], invocation, "d" * 64)


class PhaseAndPaintTests(unittest.TestCase):
    @staticmethod
    def transaction():
        initial = row()
        state = {"schema": 1, "tick": 0, "time_q": 0, "global": {"marker": 0},
                 "draw_state": {"schema": 1, "hud": {}, "message": {}}, "input": {}, "player": initial["player"]}
        final = copy.deepcopy(state)
        final.update(tick=1, time_q=6)
        final["global"]["marker"] = 1
        events = []
        for phase in analysis.PHASES:
            if phase == "update_end":
                events.append({"kind": "phase_mutations", "tick": 0, "measuring": True, "phase": "update_begin",
                               "until": "update_end", "changes": [{"op": "replace", "path": "/global/marker", "value": 1}]})
            events.append({"kind": "phase", "tick": 0, "measuring": True, "phase": phase, "site": phase})
        return [state, final], events

    def test_full_phase_reconstruction_matches_snapshot(self):
        snapshots, events = self.transaction()
        self.assertEqual(len(analysis.reconstruct(snapshots, events)), len(analysis.PHASES))

    def test_missing_duplicate_or_renamed_phase_fails(self):
        snapshots, original = self.transaction()
        for change in ("missing", "duplicate", "renamed"):
            events = copy.deepcopy(original)
            if change == "missing": events.pop(-3)
            if change == "duplicate": events.insert(-1, copy.deepcopy(events[-2]))
            if change == "renamed": events[-2]["site"] = "fabricated"
            with self.subTest(change=change), self.assertRaises(analysis.EvidenceError):
                analysis.reconstruct(snapshots, events)

    def test_semantic_snapshot_perturbation_cannot_hide_behind_valid_trace(self):
        snapshots, events = self.transaction()
        snapshots[-1]["global"]["marker"] = 2
        with self.assertRaises(analysis.EvidenceError):
            analysis.reconstruct(snapshots, events)

    def test_bits_only_zero_sign_patch_reconstructs_omitted_decimal_companion(self):
        snapshots, events = self.transaction()
        snapshots[0]["global"]["height"] = {"bits": "80000000", "value": -0.0}
        snapshots[1]["global"]["height"] = {"bits": "00000000", "value": 0.0}
        mutation = next(event for event in events if event["kind"] == "phase_mutations")
        mutation["changes"].append({"op": "replace", "path": "/global/height/bits", "value": "00000000"})
        analysis.reconstruct(snapshots, events)
        snapshots[1]["global"]["height"] = {"bits": "80000000", "value": -0.0}
        with self.assertRaises(analysis.EvidenceError):
            analysis.reconstruct(snapshots, events)

    def test_nonzero_bits_decimal_mismatch_is_not_repaired(self):
        snapshots, events = self.transaction()
        snapshots[0]["global"]["height"] = {"bits": "00000000", "value": 0.0}
        snapshots[1]["global"]["height"] = {"bits": "3f800000", "value": 1.0}
        mutation = next(event for event in events if event["kind"] == "phase_mutations")
        mutation["changes"].append({"op": "replace", "path": "/global/height/bits", "value": "3f800000"})
        with self.assertRaises(analysis.runner.ReplayError):
            analysis.reconstruct(snapshots, events)

    def test_zero_sign_patch_does_not_repair_an_invalid_prior_companion(self):
        state = {"height": {"bits": "80000000", "value": 0.0}}
        with self.assertRaises(analysis.EvidenceError):
            analysis.apply_trace_patch(state, {"op": "replace", "path": "/height/bits", "value": "00000000"})

    def test_early_extra_glyph_is_rejected_even_with_matching_final_cursor(self):
        before = {"msgMode": 6, "textDrawPos": 1, "textboxEndType": 0, "textBoxNum": 1, "msgLength": 64}
        paint = {"observed": 1, "entryMode": 6, "entryTextDrawPos": 1, "entryEndType": 0,
                 "drawFrame": 60, "asciiPathComplete": 1, "displayListLinked": 1,
                 "glyphCount": 1, "glyphFingerprint": "1234567890abcdef", "iconFingerprint": analysis.FNV_OFFSET,
                 "entryIconBranch": 0, "iconCount": 0, "iconType": -1}
        frames = {(0, "draw.message.begin"): {"message": before, "global": {"state_frames": 60}},
                  (0, "draw.message.end"): {"message": {"paint": paint}}}
        fixture = {"ticks": 1, "message_text_id": 0x1043}
        analysis.check_paint(fixture, frames)
        paint["glyphCount"] = 2
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_paint(fixture, frames)

    @staticmethod
    def timer_paint():
        before = row()
        after = copy.deepcopy(before)
        after["global"]["timer_state"] = 6
        after["hud"]["digits"] = [0, 0, 10, 0, 1]
        after["hud"]["paint"] = {
            "observed": 1, "draw_frame": 60, "clock_count": 1, "digit_count": 5, "timer_id": 0,
            "clock_x": 140, "clock_y": 82, "clock_width": 16, "clock_height": 16, "clock_s": 1024,
            "clock_t": 1024, "digit_values": [0, 0, 10, 0, 1], "digit_x": [156, 165, 174, 182, 191],
            "digit_y": [80] * 5, "digit_width": [9, 9, 8, 9, 9], "digit_height": [250] * 5,
            "digit_s": [880] * 5, "digit_t": [880] * 5, "digit_r": 255, "digit_g": 50, "digit_b": 0,
            "digit_a": 255}
        return {(0, "draw.interface.begin"): before, (0, "draw.interface.end"): after}

    def test_hud_emitted_geometry_and_color_corruption_fail(self):
        original = self.timer_paint()
        analysis.check_hud_paint({"ticks": 1}, original)
        for field in ("digit_count", "clock_y", "digit_g"):
            frames = copy.deepcopy(original)
            frames[0, "draw.interface.end"]["hud"]["paint"][field] += 1
            with self.subTest(field=field), self.assertRaises(analysis.EvidenceError):
                analysis.check_hud_paint({"ticks": 1}, frames)

    def test_hud_gate_cannot_emit_frozen_timer(self):
        frames = self.timer_paint()
        frames[0, "draw.interface.begin"]["hud"]["timer_gate_open"] = 0
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_hud_paint({"ticks": 1}, frames)

    @staticmethod
    def fade_paint():
        frames = {}
        for tick in range(12):
            before = {"msgMode": 6, "textDrawPos": 30, "textboxEndType": 1}
            paint = {"observed": 1, "entryMode": 6, "entryTextDrawPos": 30, "entryEndType": 1,
                     "drawFrame": 60 + tick, "asciiPathComplete": 1, "displayListLinked": 1,
                     "glyphCount": 17 if tick == 11 else 0,
                     "glyphFingerprint": "1234567890abcdef" if tick == 11 else analysis.FNV_OFFSET,
                     "iconFingerprint": analysis.FNV_OFFSET, "entryIconBranch": 0, "iconCount": 0, "iconType": -1}
            frames[tick, "draw.message.begin"] = {"message": before, "global": {"state_frames": 60 + tick}}
            frames[tick, "draw.message.end"] = {"message": {"paint": paint}}
        return frames

    def test_fade_cannot_emit_quicktext_glyphs_one_call_early(self):
        frames = self.fade_paint()
        fixture = {"ticks": 12, "message_text_id": 0x305F}
        analysis.check_paint(fixture, frames)
        paint = frames[10, "draw.message.end"]["message"]["paint"]
        paint.update(glyphCount=17, glyphFingerprint="1234567890abcdef")
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_paint(fixture, frames)

    def test_fade_cannot_emit_a_spurious_icon(self):
        frames = self.fade_paint()
        frames[11, "draw.message.end"]["message"]["paint"].update(
            iconCount=1, iconType=1, iconFingerprint="1234567890abcdef")
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_paint({"ticks": 12, "message_text_id": 0x305F}, frames)

    def test_fade_zero_valued_audio_ingress_is_not_silently_discarded(self):
        fixture = {"ticks": 1, "message_text_id": 0x305F}
        frames = {(0, "draw.message.end"): {"message": {"msgMode": 2}}}
        analysis.check_message(fixture, [], [], frames)
        events = [{"measuring": True, "kind": "audio-sfx-request", "phase": "draw.message.begin",
                   "tick": 0, "value": 0}]
        with self.assertRaises(analysis.EvidenceError):
            analysis.check_message(fixture, [], events, frames)


if __name__ == "__main__":
    unittest.main()
