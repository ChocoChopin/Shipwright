"""Strict, read-only acceptance of the bounded pre-extraction HUD/message corpus.

Tick n is the transaction committed by snapshot n+1. Expected boundaries below
come from the legacy source and pinned 0x1043 control/glyph metadata, not from a
candidate's self-reported success. No engine or asset payload is loaded here.
"""
from __future__ import annotations

import argparse
import copy
from fractions import Fraction
from pathlib import Path
import re
import struct
import sys

import analyze_couplings as coupling

runner = coupling.runner
EXPECTED = frozenset(("hud-zero", "hud-zero-input", "hud-warning", "hud-old-digit", "hud-message-gate",
                      "message-pages-natural", "message-pages-skip", "message-fade-observed"))
PHASES = ("input_poll", "update_begin", "update_end", "draw_begin", "draw.actors.begin", "draw.actors.end",
          "draw.interface.begin", "draw.interface.end", "draw.message.begin", "draw.message.end", "draw_end",
          "audio_control_begin", "audio_control_end", "audio.begin", "audio.end", "presentation.begin",
          "presentation.end", "transaction_end")
TIMER_FIELDS = ("main_next_second", "main_state_timer", "sub_next_second", "sub_state_timer", "digits",
                "timer_x", "timer_y", "env_hazard", "env_hazard_active")
FNV_OFFSET = "cbf29ce484222325"


class EvidenceError(runner.ReplayError):
    pass


def require(condition, label, detail=None):
    if not condition:
        raise EvidenceError(label + (": " + str(detail) if detail is not None else ""))


def equal(actual, expected, label):
    require(runner.digest(actual) == runner.digest(expected), label,
            runner.first_differences(expected, actual, limit=3))


def is_hex(value, size):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{" + str(size) + r"}", value) is not None


def check_provenance(provenance):
    require(isinstance(provenance, dict), "recorded source/build/asset provenance required")
    require(is_hex(provenance.get("source_head"), 40) and is_hex(provenance.get("tracked_diff_sha256"), 64),
            "recorded source and tracked-diff identity required")
    require(isinstance(provenance.get("branch"), str) and provenance["branch"], "recorded branch required")
    assets = provenance.get("assets")
    require(isinstance(assets, dict) and {"oot.o2r", "soh.o2r"} <= assets.keys() and
            assets.keys() <= {"oot.o2r", "soh.o2r", "gamecontrollerdb.txt"}, "recorded asset set")
    for label, record in [("executable", provenance.get("executable")), *assets.items()]:
        require(isinstance(record, dict) and is_hex(record.get("sha256"), 64) and
                type(record.get("bytes")) is int and record["bytes"] > 0 and
                isinstance(record.get("path"), str) and Path(record["path"]).is_absolute(),
                "recorded file identity", label)
    # These are immutable receipts. The original executable path may now hold
    # another build, so analysis must not replace its recorded identity by a rehash.


def check_invocation(corpus, name, repeat, provenance, invocation, fixture_hash):
    directory = corpus / name / f"run-{repeat:03d}"
    expected = [provenance["executable"]["path"], "--native-sim-test", str(corpus / name / "fixture.json"),
                "--output", str(directory / "output"), "--trace"]
    equal(invocation.get("command"), expected, "invocation executable/fixture/output/trace binding")
    equal(invocation.get("cwd"), str(directory / "work"), "invocation isolated working directory")
    equal(invocation.get("fixture_sha256"), fixture_hash, "invocation fixture byte identity")
    staging = invocation.get("asset_staging")
    require(isinstance(staging, dict) and staging.keys() == provenance["assets"].keys() and
            all(mode in ("copy", "hardlink") for mode in staging.values()), "invocation staged asset set")
    require(invocation.get("status") == "pass" and type(invocation.get("exit_code")) is int and
            invocation["exit_code"] == 0, "invocation completion status")
    require(type(invocation.get("snapshot_count")) is int and invocation["snapshot_count"] > 1 and
            is_hex(invocation.get("sequence_sha256"), 64), "invocation output identity")


def view(state):
    require(state["draw_state"]["schema"] == 1, "draw observation schema")
    return {"global": copy.deepcopy(state["global"]), "hud": copy.deepcopy(state["draw_state"]["hud"]),
            "message": copy.deepcopy(state["draw_state"]["message"]), "input": copy.deepcopy(state["input"]),
            "player": {key: copy.deepcopy(state["player"][key]) for key in
                       ("action", "melee_state", "melee_animation", "state_flags")}}


def apply_trace_patch(state, change):
    # nlohmann::json::diff treats -0.0 and +0.0 as numerically equal. It emits
    # their changed IEEE bits, but omits the redundant readable-value change.
    # Recover that companion only for an explicit zero-sign bit replacement;
    # do not normalize nonzero values or repair mismatched engine snapshots.
    parent = change["path"].rsplit("/", 1)[0]
    prior = coupling.lookup(state, parent) if change["path"].endswith("/bits") else None
    signed_zero = (change["op"] == "replace" and isinstance(prior, dict) and
                   prior.get("bits") in ("00000000", "80000000") and
                   change.get("value") in ("00000000", "80000000") and
                   change["value"] != prior["bits"] and
                   type(prior.get("value")) is float and prior["value"] == 0.0)
    if signed_zero:
        require(struct.pack("!f", prior["value"]).hex() == prior["bits"],
                "invalid preexisting signed-zero companion")
    state = coupling.apply_patch(state, change)
    if signed_zero:
        state = coupling.apply_patch(state, {"op": "replace", "path": parent + "/value",
                                            "value": -0.0 if change["value"] == "80000000" else 0.0})
    return state


def reconstruct(snapshots, events):
    """Require every phase, apply every patch, and bind transactions to snapshots."""
    state = copy.deepcopy(snapshots[0])
    measured = [event for event in events if event.get("measuring") is True]
    require(all(type(event.get("measuring")) is bool for event in events), "trace measuring flag")
    ticks = len(snapshots) - 1
    phase_lists = {tick: [] for tick in range(ticks)}
    frames = {}
    current_phase = None
    current_tick = -1
    for index, event in enumerate(measured):
        tick = event["tick"]
        require(tick in phase_lists, "extra measured tick", tick)
        if tick != current_tick:
            require(tick == current_tick + 1 and event["kind"] == "phase" and event.get("site") == "input_poll",
                    "measured transaction start", tick)
            current_tick = tick
        if event["kind"] == "phase":
            name = event.get("site")
            require(event["phase"] == name, "phase event name", event)
            phase_lists[tick].append(name)
            current_phase = name
            frames[tick, name] = view(state)
            if name == "transaction_end":
                committed = dict(state, tick=tick + 1, time_q=(tick + 1) * 6)
                equal(committed, snapshots[tick + 1], f"phase reconstruction snapshot {tick + 1}")
                state = copy.deepcopy(snapshots[tick + 1])
        else:
            require(event["phase"] == current_phase, "event outside declared phase", event)
            if event["kind"] == "phase_mutations":
                require(index + 1 < len(measured), "trailing mutation without phase")
                following = measured[index + 1]
                require(following["kind"] == "phase" and following["tick"] == tick and
                        following.get("site") == event.get("until"), "mutation end phase", event.get("until"))
                require(isinstance(event.get("changes"), list) and event["changes"], "empty mutation record")
                for change in event["changes"]:
                    state = apply_trace_patch(state, change)
    for tick, phases in phase_lists.items():
        equal(phases, list(PHASES), f"complete ordered phases tick {tick}")
    return frames


def timer_values(row):
    return {"state": row["global"]["timer_state"], "seconds": row["global"]["timer_seconds"],
            **{key: copy.deepcopy(row["hud"][key]) for key in TIMER_FIELDS}}


def timer_step(before):
    """Bounded legacy main-down oracle; unsupported sibling states fail closed."""
    h, g = before["hud"], before["global"]
    result = timer_values(before)
    require(g["subtimer_state"] == 0 and h["env_hazard_active"] == 0 and h["env_hazard"] == 0,
            "countdown profile excludes subtimer/environment hazard")
    require(h["game_mode"] == 0, "countdown fixture requires ordinary gameplay mode")
    require(result["state"] in (0, 5, 6, 7, 8, 10), "unsupported timer state", result["state"])
    sounds = []
    if not h["timer_gate_open"] or h["no_ui"] or h["pause_debug_state"]:
        return result, sounds
    state = result["state"]
    if state == 5:
        result.update(state=6, main_state_timer=20, main_next_second=20)
    elif state == 6:
        result["main_state_timer"] -= 1
        if result["main_state_timer"] == 0:
            result.update(state=7, main_state_timer=20)
    elif state in (7, 8):
        target_y = 54 if h["health_capacity"] > 0xA0 else 46
        if state == 7:
            divisor = result["main_state_timer"]
            require(divisor > 0, "MOVE divider must be positive")
            result["timer_x"][0] -= int((result["timer_x"][0] - 26) / divisor)
            result["timer_y"][0] -= int((result["timer_y"][0] - target_y) / divisor)
            result["main_state_timer"] -= 1
            if result["main_state_timer"] == 0:
                result.update(state=8, main_state_timer=20)
                result["timer_x"][0], result["timer_y"][0] = 26, target_y
        if result["state"] == 8:
            result["timer_y"][0] = target_y
        if h["countdown_length_gate_open"]:
            result["main_next_second"] -= 1
            if result["main_next_second"] == 0:
                result["seconds"] = max(0, result["seconds"] - 1)
                result["main_next_second"] = 20
                seconds, old_digit = result["seconds"], result["digits"][4]
                if seconds == 0:
                    result["state"] = 10
                elif seconds > 60:
                    if old_digit == 1:
                        sounds.append(0x4804)
                elif seconds >= 11:
                    if old_digit & 1:
                        sounds.append(0x4819)
                else:
                    sounds.append(0x481A)
    elif state == 10:
        result["state"] = 0
    if result["state"] not in (0, 10):
        minutes, seconds = divmod(result["seconds"], 60)
        result["digits"] = [minutes // 10, minutes % 10, 10, seconds // 10, seconds % 10]
    return result, sounds


def sound_events(events, phase, kind="audio-sfx-request"):
    return [(event["tick"], event["value"]) for event in events if event.get("measuring") is True and
            event["kind"] == kind and event["phase"] == phase]


def check_hud(fixture, snapshots, events, frames):
    if "hud_timer_seconds" not in fixture:
        return {}
    expected_sounds, stopped, gates = [], [], []
    for tick in range(fixture["ticks"]):
        before, after = frames[tick, "draw.interface.begin"], frames[tick, "draw.interface.end"]
        expected, sounds = timer_step(before)
        equal(timer_values(after), expected, f"HUD transition/counters/digits tick {tick}")
        equal(timer_values(frames[tick, "update_begin"]), timer_values(before), f"HUD ownership before draw {tick}")
        equal(timer_values(after), timer_values(frames[tick, "transaction_end"]), f"HUD ownership after draw {tick}")
        expected_sounds.extend((tick, value) for value in sounds)
        if not before["hud"]["timer_gate_open"]:
            gates.append(tick)
        if after["global"]["timer_state"] == 10:
            stopped.append(tick)
    equal(sound_events(events, "draw.interface.begin"), expected_sounds, "ordered HUD sound requests")
    equal(sound_events(events, "draw.interface.begin", "audio-sfx-queued"), expected_sounds, "ordered HUD sound queue")
    if fixture["hud_timer_seconds"] == 1:
        require(len(stopped) == 1, "STOP must survive exactly one eligible HUD transaction", stopped)
        tick = stopped[0]
        require(frames[tick + 1, "update_begin"]["global"]["timer_state"] == 10 and
                frames[tick + 1, "draw.interface.end"]["global"]["timer_state"] == 0, "STOP next-update/OFF boundary")
        equal(snapshots[-1]["global"]["timer_seconds"], 0, "countdown reached zero")
    if fixture["id"] == "hud-message-gate":
        require(gates and max(gates) >= 40 and stopped[0] > max(gates), "real message gate suspended countdown")
    else:
        equal(gates, [], "unexpected ordinary countdown gate")
        if fixture["hud_timer_seconds"] == 1:
            equal(stopped, [40], "zero canonical boundary")
    if fixture["id"] == "hud-zero-input":
        stopped_update = frames[41, "update_end"]["player"]
        equal(stopped_update["melee_state"], 0, "STOP suppresses B melee action")
        upper = [row["player"]["upper_animation"]["resource"] for row in snapshots]
        equal(upper[42], upper[41], "STOP suppresses B item-equipping animation")
        equal(upper[43], upper[41], "release does not defer the suppressed item edge")
        equal(upper[44], "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_fighter2free",
              "B after OFF starts the source-selected sword-equipping upper animation")
        require(upper[44] != upper[43], "allowed B must change item-equipping animation")
    return {"stop_ticks": stopped, "gate_ticks": gates, "sound_requests": expected_sounds}


def check_inputs(fixture, snapshots, events):
    rows = [event for event in events if event.get("measuring") is True and event["kind"] == "input_event"]
    expected = fixture["input"]
    equal(len(rows), len(expected), "each input transition applied once")
    for actual, item in zip(rows, expected):
        time = Fraction(item["time_num"], item["time_den"])
        tick = (time.numerator * 20 + time.denominator - 1) // time.denominator
        equal([actual["input_sequence"], actual["input_time_num"], actual["input_time_den"], actual["tick"],
               actual["applied_time_q"], actual["buttons"]],
              [item["sequence"], item["time_num"], item["time_den"], tick, tick * 6, item["buttons"]], "input event identity")
        require(actual["phase"] == "input_poll", "input application phase")
    held, index = 0, 0
    for tick in range(fixture["ticks"]):
        pressed = released = 0
        while index < len(expected) and Fraction(expected[index]["time_num"], expected[index]["time_den"]) <= Fraction(tick, 20):
            next_held = expected[index]["buttons"]
            pressed |= next_held & ~held
            released |= held & ~next_held
            held, index = next_held, index + 1
        sample = snapshots[tick + 1]["input"]
        equal([sample["held"], sample["pressed"], sample["released"]], [held, pressed, released], f"single-use input edges {tick}")


def message_expected(fixture_id, ticks):
    skip = fixture_id == "message-pages-skip"
    boundaries = [(0, 2), (8, 3), (9, 4), (10, 6)]
    boundaries += [(16, 4), (17, 6), (18, 53), (22, 54), (24, 0)] if skip else [
        (37, 52), (40, 4), (41, 6), (76, 53), (80, 54), (82, 0)]
    return {tick: next(mode for start, mode in reversed(boundaries) if tick >= start) for tick in range(ticks)}


def check_message(fixture, snapshots, events, frames):
    if fixture.get("message_text_id") == 0x305F:
        boundaries = [(0, 2), (8, 3), (9, 4), (10, 6), (12, 53), (72, 54), (74, 0)]
        for tick in range(fixture["ticks"]):
            expected = next(mode for start, mode in reversed(boundaries) if tick >= start)
            message = frames[tick, "draw.message.end"]["message"]
            equal(message["msgMode"], expected, f"legacy quick-fade boundary {tick}")
            if 12 <= tick < 72:
                equal(message["stateTimer"], 72 - tick, f"fade countdown ignores A edge {tick}")
        equal(sound_events(events, "draw.message.begin"), [], "quick-fade has no draw audio ingress")
        return {"done_tick": 12, "close_tick": 72, "none_tick": 74}
    if fixture.get("message_text_id") != 0x1043:
        return {}
    ticks = fixture["ticks"]
    expected = message_expected(fixture["id"], ticks)
    skip = fixture["id"] == "message-pages-skip"
    close = 22 if skip else 80
    first_page_end, second_page_start, second_page_end = (16, 17, 18) if skip else (37, 41, 76)
    for tick in range(ticks):
        before, after = frames[tick, "draw.message.begin"]["message"], frames[tick, "draw.message.end"]["message"]
        equal(after["msgMode"], expected[tick], f"message mode boundary {tick}")
        icon = before["msgMode"] in (52, 53) and before["msgLength"] > 0
        equal(after["stateTimer"], (before["stateTimer"] + int(icon)) & 255, f"message icon-owned timer {tick}")
        if tick in (close, close + 1, close + 2):
            equal(after["stateTimer"], close + 2 - tick, f"message close timer {tick}")
        if 10 <= tick < second_page_start:
            equal(after["textBoxNum"], 1, f"first decoded page ordinal {tick}")
        if second_page_start <= tick < close + 2:
            equal(after["textBoxNum"], 2, f"second decoded page ordinal {tick}")
        if before["decodedBufferValid"]:
            require(before["decodedTextLen"] in (27, 35), "selected decoded page length")
    if skip:
        require(frames[15, "update_end"]["message"]["textboxSkipped"] == 1, "B skip flag")
        equal([tick for tick in range(ticks) if frames[tick, "draw.message.end"]["message"]["msgMode"] == 52], [],
              "skipped page must not wait for a page edge")
    expected_nonzero = [(second_page_end, 0x482E)]
    equal([(tick, value) for tick, value in sound_events(events, "draw.message.begin") if value],
          expected_nonzero, "message END sound at late authority boundary")
    if skip:
        zero_ticks = list(range(10, 16)) + [17]
    else:
        zero_ticks = [tick for tick in range(10, 37) if tick not in (15, 21, 25, 29)] + [37] + [
            tick for tick in range(41, 76) if tick not in (43, 47, 55, 61, 64, 68)]
    equal([(tick, value) for tick, value in sound_events(events, "draw.message.begin") if not value],
          [(tick, 0) for tick in zero_ticks], "message glyph/control zero-valued audio ingress")
    update_sounds = sound_events(events, "update_begin")
    for tick, value in ([(close, 0x4808)] if skip else [(40, 0x4818), (close, 0x4808)]):
        equal(update_sounds.count((tick, value)), 1, "single page/close input sound")
    equal(snapshots[-1]["global"]["message_mode"], 0, "message closed")
    return {"first_page_boundary": first_page_end, "second_page_start": second_page_start,
            "second_page_boundary": second_page_end, "close_tick": close, "none_tick": close + 2}


def check_paint(fixture, frames):
    summary = []
    plain = fixture.get("message_text_id") == 0x1043
    for tick in range(fixture["ticks"]):
        before = frames[tick, "draw.message.begin"]["message"]
        after = frames[tick, "draw.message.end"]["message"]
        paint = after["paint"]
        equal([paint["observed"], paint["entryMode"], paint["entryTextDrawPos"], paint["entryEndType"]],
              [1, before["msgMode"], before["textDrawPos"], before["textboxEndType"]], f"paint entry-state latch {tick}")
        equal(paint["drawFrame"], frames[tick, "draw.message.begin"]["global"]["state_frames"], f"paint CPU draw identity {tick}")
        require(paint["asciiPathComplete"] == 1 and paint["displayListLinked"] == 1, "admitted ASCII paint path")
        fingerprint = paint["glyphFingerprint"]
        require(isinstance(fingerprint, str) and len(fingerprint) == 16, "paint fingerprint shape")
        int(fingerprint, 16)
        if paint["glyphCount"] == 0:
            equal(fingerprint, FNV_OFFSET, "empty glyph fingerprint")
        else:
            require(fingerprint != FNV_OFFSET, "nonempty glyph fingerprint")
        icon_fingerprint = paint["iconFingerprint"]
        require(isinstance(icon_fingerprint, str) and len(icon_fingerprint) == 16, "icon fingerprint shape")
        int(icon_fingerprint, 16)
        require((icon_fingerprint == FNV_OFFSET) == (paint["iconCount"] == 0), "icon appearance fingerprint")
        if plain:
            draw_text = before["msgMode"] in (6, 52, 53)
            page = before["textBoxNum"]
            terminator, nonglyphs = (27, {5, 11, 15, 19}) if page == 1 else (35, {2, 6, 14, 20, 23, 27})
            count = sum(i not in nonglyphs for i in range(min(before["textDrawPos"], terminator))) if draw_text else 0
            expected_icon = int(before["msgLength"] > 0 and before["msgMode"] in (52, 53))
            equal(paint["glyphCount"], count, f"glyph prefix from entry cursor {tick}")
            equal([paint["entryIconBranch"], paint["iconCount"], paint["iconType"]],
                  [expected_icon, expected_icon, (0 if before["msgMode"] == 52 else 1) if expected_icon else -1],
                  f"entry-mode icon presence/type {tick}")
        elif fixture.get("message_text_id") == 0x305F:
            # QUICKTEXT rewrites the traversal bound at tick 10 without drawing
            # its preceding glyphs; fade appearance starts on the next call.
            equal(paint["glyphCount"], 17 if 11 <= tick < 72 else 0, f"quick-fade glyph emission {tick}")
            equal([paint["entryIconBranch"], paint["iconCount"], paint["iconType"]], [0, 0, -1],
                  f"quick-fade must not select an icon {tick}")
        summary.append({"tick": tick, **paint})
    return {"cpu_draws": len(summary), "paint_sequence_sha256": runner.digest(summary),
            "maximum_glyph_count": max(row["glyphCount"] for row in summary),
            "icon_draws": sum(row["iconCount"] for row in summary)}


def check_hud_paint(fixture, frames):
    projected_offsets = set()
    emitted = 0
    for tick in range(fixture["ticks"]):
        before, after = frames[tick, "draw.interface.begin"], frames[tick, "draw.interface.end"]
        h, paint = after["hud"], after["hud"]["paint"]
        equal([paint["observed"], paint["draw_frame"]], [1, before["global"]["state_frames"]], "HUD paint identity")
        visible = bool(before["hud"]["timer_gate_open"] and not h["no_ui"] and not h["pause_debug_state"] and
                       h["game_mode"] == 0 and after["global"]["timer_state"] not in (0, 10))
        equal([paint["clock_count"], paint["digit_count"], paint["timer_id"]],
              [1, 5, 0] if visible else [0, 0, -1], f"HUD actual emission/zero/gate visibility {tick}")
        if visible:
            emitted += 1
            equal(paint["digit_values"], h["digits"], "HUD emitted digit selection")
            equal([paint["clock_width"], paint["clock_height"], paint["clock_s"], paint["clock_t"]],
                  [16, 16, 1024, 1024], "HUD clock geometry")
            equal(paint["clock_y"], h["timer_y"][0] + 2, "HUD clock vertical placement")
            equal(paint["digit_x"], [paint["clock_x"] + offset for offset in (16, 25, 34, 42, 51)], "HUD digit spacing")
            equal(paint["digit_y"], [h["timer_y"][0]] * 5, "HUD digit vertical placement")
            equal(paint["digit_width"], [9, 9, 8, 9, 9], "HUD digit widths")
            equal(paint["digit_height"], [250] * 5, "HUD digit height argument")
            equal(paint["digit_s"], [880] * 5, "HUD horizontal texture step")
            equal(paint["digit_t"], [880] * 5, "HUD vertical texture step")
            equal([paint["digit_r"], paint["digit_g"], paint["digit_b"], paint["digit_a"]],
                  [255, 50, 0, 255] if after["global"]["timer_seconds"] < 10 else [255, 255, 255, 255],
                  "HUD timer color")
            projected_offsets.add(paint["clock_x"] - h["timer_x"][0])
    require(len(projected_offsets) <= 1, "HUD projection offset changes in pinned window")
    return {"visible_cpu_draws": emitted, "projection_offsets": sorted(projected_offsets)}


def observe(output, fixture):
    manifest, snapshots = runner.load_run(output)
    runner.validate_requested_fixture(manifest, fixture)
    require(fixture.get("observe_draw_state") is True, "fixture must opt in to draw-state observation")
    events = runner.load_trace(output, fixture["ticks"])
    frames = reconstruct(snapshots, events)
    check_inputs(fixture, snapshots, events)
    hud = check_hud(fixture, snapshots, events, frames)
    message = check_message(fixture, snapshots, events, frames)
    paint = check_paint(fixture, frames)
    hud_paint = check_hud_paint(fixture, frames)
    require(runner.fixture_assertions(fixture, snapshots)["status"] == "pass", "fixture behavior assertions")
    return {"status": "pass", "output": str(output), "fixture_id": fixture["id"], "ticks": fixture["ticks"],
            "snapshots": len(snapshots), "trace_records": len(events), "snapshots_sha256": runner.digest(snapshots),
            "trace_sha256": runner.digest(events), "hud": hud, "message": message, "paint": paint, "hud_paint": hud_paint}


def check_corpus_set(corpus, receipt, paths):
    require(receipt.get("schema") == 1 and receipt.get("status") == "pass" and receipt.get("rate_hz") == 20,
            "passing canonical corpus receipt required")
    require(type(receipt.get("repeats")) is int and receipt["repeats"] == 3, "exactly three repeats required")
    check_provenance(receipt.get("provenance"))
    equal(receipt.get("output"), str(corpus), "corpus receipt output binding")
    records = receipt.get("fixtures", [])
    ids = [record.get("id") for record in records]
    require(len(ids) == len(EXPECTED) and set(ids) == EXPECTED, "complete exact draw-state fixture set", ids)
    expected = {corpus / name / f"run-{repeat:03d}" / "output" for name in EXPECTED for repeat in range(1, 4)}
    equal(sorted(str(path) for path in paths), sorted(str(path) for path in expected), "complete exact output set")
    for record in records:
        require(record.get("status") == "pass" and len(record.get("runs", [])) == 3 and
                all(run.get("status") == "pass" and run.get("exit_code") == 0 for run in record["runs"]),
                "all invocation receipts must pass", record.get("id"))


def analyze(corpus):
    receipt = runner.read_json(corpus / "corpus_result.json")
    paths = sorted(path.parent for path in corpus.rglob("result.json") if path.parent.name == "output")
    check_corpus_set(corpus, receipt, paths)
    directories = set(corpus.glob("*/run-*"))
    require(directories == {path.parent for path in paths}, "unexpected/incomplete repeat directories")
    records = {record["id"]: record for record in receipt["fixtures"]}
    results = []
    for name in sorted(EXPECTED):
        fixture_path = corpus / name / "fixture.json"
        fixture = runner.validate_fixture(runner.read_json(fixture_path))
        equal(fixture, runner.read_json(runner.FIXTURES / "draw-state" / f"{name}.json"), "source fixture binding")
        for repeat in range(1, 4):
            invocation = records[name]["runs"][repeat - 1]
            output = corpus / name / f"run-{repeat:03d}" / "output"
            try:
                check_invocation(corpus, name, repeat, receipt["provenance"], invocation,
                                 runner.file_digest(fixture_path))
                equal(runner.read_json(output.parent / "invocation.json"), invocation,
                      "persisted and aggregate invocation receipts")
                result = observe(output, fixture)
                hashes = runner.hashes_for_run(output)
                equal([invocation["snapshot_count"], invocation["sequence_sha256"]],
                      [result["snapshots"], hashes["sequence_sha256"]], "invocation observed output identity")
            except (OSError, KeyError, IndexError, TypeError, ValueError) as error:
                result = {"status": "fail", "output": str(output), "fixture_id": name, "error": str(error)}
            results.append(result)
    completeness = coupling.corpus_completeness(corpus, receipt, paths, results)
    return {"schema": 1, "status": "pass" if completeness["status"] == "pass" and
            all(result["status"] == "pass" for result in results) else "fail", "corpus": str(corpus),
            "provenance": receipt.get("provenance"), "completeness": completeness, "runs": results,
            "scope": "Canonical source-derived HUD/message phase, input, audio-ingress and CPU paint observations; no extraction or GPU-completion claim."}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    corpus = args.corpus.resolve(strict=True)
    output = args.output or corpus / "draw_state_result.json"
    runner.local_output(output.resolve().parent, create=False)
    try:
        result = analyze(corpus)
    except (OSError, KeyError, IndexError, TypeError, ValueError) as error:
        result = {"schema": 1, "status": "fail", "corpus": str(corpus), "error": str(error)}
    runner.write_json(output, result)
    print(f"Draw-state analysis {result['status']}: {output}")
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
