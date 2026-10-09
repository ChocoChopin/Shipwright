"""Read completed native-simulation outputs without repairing or filtering JSONL.

This is a read-only engine-output analyzer, not an engine test or serializer.
It validates every record through the corpus runner before selecting event kinds
for measurements. Invalid runs remain explicit failures in the receipt. An
aggregate pass requires a passing, complete corpus and every expected repeat.
"""
from __future__ import annotations

import argparse
import collections
import copy
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("replay_runner", ROOT / "scripts/native-simulation/run_corpus.py")
runner = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(runner)

# The snapshot's "events" entry is the NativeSimTest_Event aggregate, not an
# independently advanced RNG stream. Its count has a separate trace contract.
AUTHORITATIVE_EVENT_KINDS = frozenset((
    "rng-seed", "rng-seed-low", "rng-seed-high", "rng-state-high", "rng-output", "rng-draw",
    "audio_block", "audio-sfx-request", "audio-sfx-queued", "audio-sequence-command", "ocarina-memory-note",
))


def pointer_parts(path):
    return [part.replace("~1", "/").replace("~0", "~") for part in path.split("/")[1:]]


def lookup(state, path):
    value = state
    try:
        for part in pointer_parts(path):
            value = value[int(part)] if isinstance(value, list) else value[part]
        return copy.deepcopy(value)
    except (KeyError, IndexError, TypeError, ValueError):
        return None


def apply_patch(state, change):
    parts = pointer_parts(change["path"])
    if not parts:
        if change["op"] in ("add", "replace"):
            return copy.deepcopy(change["value"])
        raise ValueError("unsupported root JSON patch operation")
    parent = state
    for part in parts[:-1]:
        parent = parent[int(part)] if isinstance(parent, list) else parent[part]
    key = parts[-1]
    if isinstance(parent, list):
        index = len(parent) if key == "-" else int(key)
        if change["op"] == "remove":
            parent.pop(index)
        elif change["op"] == "add":
            parent.insert(index, copy.deepcopy(change["value"]))
        elif change["op"] == "replace":
            parent[index] = copy.deepcopy(change["value"])
        else:
            raise ValueError("unsupported JSON patch operation: " + change["op"])
    elif change["op"] == "remove":
        del parent[key]
    elif change["op"] in ("add", "replace"):
        parent[key] = copy.deepcopy(change["value"])
    else:
        raise ValueError("unsupported JSON patch operation: " + change["op"])
    return state


def compact_counter(counter):
    return {str(key): count for key, count in sorted(counter.items())}


def observe(output):
    manifest, snapshots = runner.load_run(output)
    events = runner.load_trace(output, manifest["ticks_completed"])
    first, last = snapshots[0], snapshots[-1]
    measured = [event for event in events if event.get("measuring") is True]
    rng_events = [event for event in measured if event["kind"] == "rng"]
    block_events = [event for event in measured if event["kind"] == "audio_block"]
    block_ticks = collections.Counter(event["tick"] for event in block_events)
    rng_counts = collections.Counter((event["stream"], event["phase"]) for event in rng_events)
    sound_counts = collections.Counter((event["kind"], event["phase"]) for event in measured
                                       if event["kind"] in ("audio-sfx-request", "audio-sfx-queued", "audio-sequence-command"))
    rng_delta = {}
    for name in sorted(first["rng"].keys() | last["rng"].keys()):
        initial, final = first["rng"].get(name, {}), last["rng"].get(name, {})
        rng_delta[name] = {"calls": final.get("calls", 0) - initial.get("calls", 0),
                           "initial_state": initial.get("state"), "final_state": final.get("state")}
        if "draw_calls" in final:
            rng_delta[name]["draw_calls"] = final["draw_calls"] - initial.get("draw_calls", 0)
    audio_keys = ("blocks", "samples", "clock_calls", "random", "task_count")
    audio_initial = {key: first["audio"][key] for key in audio_keys}
    audio_final = {key: last["audio"][key] for key in audio_keys}
    audio_delta = {key: audio_final[key] - audio_initial[key] for key in audio_keys if key != "random"}
    stream_counts = collections.Counter(event["stream"] for event in rng_events)
    audio_checks = {
        "three_blocks_each_measured_tick": block_ticks == collections.Counter(
            {tick: 3 for tick in range(manifest["ticks_completed"])}),
        "all_blocks_528_samples": all(event["value"] == 528 for event in block_events),
        "block_counter_matches_trace": audio_delta["blocks"] == len(block_events),
        "sample_counter_matches_trace": audio_delta["samples"] == sum(event["value"] for event in block_events),
        "task_counter_matches_blocks": audio_delta["task_count"] == len(block_events),
        "audio_context_calls_match_blocks": stream_counts["audio-context"] == len(block_events),
        "clock_calls_match_audio_rng": audio_delta["clock_calls"] == stream_counts["audio-context"] + stream_counts["audio-next"],
        "observed_rng_counters_match_trace": all(
            rng_delta.get(name, {}).get("calls", 0) == stream_counts[name]
            for name in (rng_delta.keys() | stream_counts.keys()) - {"events"}),
        "authoritative_event_counter_matches_trace": rng_delta.get("events", {}).get("calls", 0) == sum(
            event["kind"] in AUTHORITATIVE_EVENT_KINDS for event in measured),
    }

    keese_ids = sorted({actor["identity"] for row in snapshots for actor in row["actors"] if actor["type"] == 19})
    keese_events = [event for event in rng_events if event["actor"] in keese_ids and event["stream"] == "gameplay"
                    and event["site"] == "Rand_ZeroOne" and event["phase"] == "draw.actors.begin"]
    keese_groups = collections.Counter((event["actor"], event["tick"]) for event in keese_events)
    keese = {"actor_type": 19, "stable_identities": keese_ids,
             "rand_zero_one_draw_calls": len(keese_events),
             "per_actor_tick": [{"identity": identity, "tick": tick, "calls": count}
                                for (identity, tick), count in sorted(keese_groups.items())],
             "ticks_with_exactly_six_calls": sum(count == 6 for count in keese_groups.values()),
             "has_measured_six_call_draw": any(count == 6 for count in keese_groups.values()),
             "snapshot_gameplay_draw_call_delta": rng_delta.get("gameplay", {}).get("draw_calls"),
             "coverage": "base actor plus observed existing limb RNG calls; no generic actor action serializer"}

    observed = copy.deepcopy(first)
    timer_changes, message_changes, collision_changes, geometry_examples = [], [], [], []
    draw_mutations, geometry_counts = collections.Counter(), collections.Counter()
    for event in measured:
        if event["kind"] != "phase_mutations":
            continue
        for change in event["changes"]:
            path = change["path"]
            item = {"tick": event["tick"], "phase": event["phase"], "until": event["until"],
                    "path": path, "old": lookup(observed, path), "new": change.get("value"), "operation": change["op"]}
            if event["phase"].startswith("draw"):
                draw_mutations[(event["phase"], path.split("/")[1])] += 1
            if path in ("/global/timer_state", "/global/timer_seconds", "/global/subtimer_state", "/global/subtimer_seconds"):
                timer_changes.append(item)
            if path in ("/global/message_mode", "/global/message_timer", "/global/text_draw_pos", "/global/text_id", "/global/ocarina_mode"):
                message_changes.append(item)
            if path.startswith("/collision/") and event["phase"] == "draw.actors.begin":
                collision_changes.append(item)
            if path.startswith(("/player/weapon_geometry", "/player/shield_quad", "/player/body_parts", "/player/left_hand")) and event["phase"] == "draw.actors.begin":
                geometry_counts[path.split("/")[2]] += 1
                if len(geometry_examples) < 16:
                    geometry_examples.append(item)
            observed = apply_patch(observed, change)

    memory_events = [event for event in events if event["kind"] == "ocarina-memory-note"]
    audio_next_events = [event for event in events if event["kind"] == "rng" and event["stream"] == "audio-next"]
    snapshot_actions = sorted({row["player"]["action"] for row in snapshots})
    identity_issues = [{"tick": row["tick"], "type": actor["type"], "identity": actor["identity"]}
                       for row in snapshots for actor in row["actors"] if actor["identity"] in ("untracked", None)]
    checks = dict(audio_checks)
    checks["player_actions_mapped"] = "unmapped" not in snapshot_actions
    checks["live_actor_identities_tracked"] = not identity_issues
    fixture = manifest["fixture"]
    if fixture.get("spawn_ice_keese"):
        checks["keese_limb_rng_exercised"] = keese["has_measured_six_call_draw"]
    if "hud_timer_seconds" in fixture:
        checks["hud_draw_countdown_exercised"] = any(item["path"] == "/global/timer_seconds" and
            item["phase"] == "draw.interface.begin" and isinstance(item["old"], int) and
            isinstance(item["new"], int) and item["new"] < item["old"] for item in timer_changes)
    if "message_text_id" in fixture:
        checks["message_draw_progression_exercised"] = any(item["phase"] == "draw.message.begin" and
            item["old"] != item["new"] for item in message_changes)
    if "ocarina_memory_round" in fixture:
        checks["three_actual_memory_notes_generated"] = len(memory_events) == 3 and all(
            event["value"] == first["audio"]["memory_notes"][index]["pitch"] and
            first["audio"]["memory_notes"][index]["length"] > 0 for index, event in enumerate(memory_events))
    if manifest["fixture_id"] == "animation-sword":
        checks["sword_draw_registration_exercised"] = any(item["path"] == "/collision/at_count" and
            isinstance(item["old"], int) and item["new"] > item["old"] for item in collision_changes)
        checks["sword_draw_geometry_exercised"] = geometry_counts["weapon_geometry"] > 0

    comparison_path = output.parent / "comparison.json"
    comparison = runner.read_json(comparison_path) if comparison_path.exists() else None
    return {"status": "pass" if all(checks.values()) else "measurement-failure", "output": str(output),
            "fixture_id": manifest["fixture_id"], "ticks": manifest["ticks_completed"],
            "snapshots": len(snapshots), "trace_records": len(events), "checks": checks,
            "final_snapshot_sha256": runner.digest(last), "snapshots_sha256": runner.digest(snapshots),
            "trace_sha256": runner.digest(events),
            "repeat_comparison": comparison, "player_actions": snapshot_actions, "identity_issues": identity_issues,
            "audio_initial": audio_initial, "audio_final": audio_final, "audio_delta": audio_delta,
            "audio_block_sample_counts": compact_counter(collections.Counter(event["value"] for event in block_events)),
            "rng_snapshot_deltas": rng_delta,
            "rng_measured_by_phase": {":".join(key): count for key, count in sorted(rng_counts.items())},
            "logical_audio_by_phase": {":".join(key): count for key, count in sorted(sound_counts.items())},
            "keese": keese, "timer_changes": timer_changes, "message_changes": message_changes,
            "draw_actor_collision_changes": collision_changes, "draw_geometry_changes": dict(geometry_counts),
            "draw_geometry_examples": geometry_examples,
            "draw_mutations_by_domain": {":".join(key): count for key, count in sorted(draw_mutations.items())},
            "memory_notes_at_snapshot_zero": first["audio"]["memory_notes"][:3],
            "memory_note_events": memory_events, "audio_next_events": audio_next_events,
            "scope": manifest["coverage"]}


def corpus_completeness(corpus, receipt, paths, runs):
    """A valid subset is diagnostic evidence, never a completed-corpus pass."""
    issues = []
    if receipt.get("status") != "pass":
        issues.append("corpus completion status is not pass")
    repeats = receipt.get("repeats")
    if type(repeats) is not int or repeats < 1:
        issues.append("corpus receipt must specify a positive repeat count")
        repeats = 0
    fixtures = receipt.get("fixtures")
    if not isinstance(fixtures, list) or not fixtures:
        issues.append("corpus receipt has no fixture list")
        fixtures = []
    expected = {}
    fixture_ids = []
    groups = []
    for fixture in fixtures:
        fixture_id = fixture.get("id") if isinstance(fixture, dict) else None
        if not isinstance(fixture_id, str) or not fixture_id or fixture_id in (".", "..") or Path(fixture_id).name != fixture_id:
            issues.append("invalid fixture identity in corpus receipt")
            continue
        if fixture_id in fixture_ids:
            issues.append("duplicate fixture identity in corpus receipt: " + fixture_id)
            continue
        fixture_ids.append(fixture_id)
        if fixture.get("status") != "pass":
            issues.append("fixture completion status is not pass: " + fixture_id)
        locations = [(corpus / fixture_id, "runs")]
        if "reference_executable" in receipt:
            locations.append((corpus / fixture_id / "reference", "reference_runs"))
        for directory, run_key in locations:
            invocations = fixture.get(run_key)
            if not isinstance(invocations, list) or len(invocations) != repeats or any(
                    not isinstance(item, dict) or item.get("status") != "pass" for item in invocations):
                issues.append(f"incomplete or failed invocation receipts: {fixture_id}/{run_key}")
            group = []
            for repeat in range(1, repeats + 1):
                path = directory / f"run-{repeat:03d}" / "output"
                expected[path] = fixture_id
                group.append(path)
            if group:
                groups.append(group)
    actual = set(paths)
    missing, unexpected = sorted(expected.keys() - actual), sorted(actual - expected.keys())
    if missing:
        issues.append("one or more expected completed outputs are missing")
    if unexpected:
        issues.append("completed outputs exist outside the expected fixture/repeat set")
    results = {Path(run["output"]): run for run in runs}
    for path, fixture_id in expected.items():
        result = results.get(path)
        if result and result.get("status") != "invalid-output" and result.get("fixture_id") != fixture_id:
            issues.append(f"output fixture identity differs from expected {fixture_id}: {path}")
    for group in groups:
        valid = [results[path] for path in group if path in results and results[path].get("status") == "pass"]
        if len(valid) != repeats:
            issues.append("not every expected repeat has valid passing measurements: " + str(group[0].parent.parent))
        elif len({(run["snapshots_sha256"], run["trace_sha256"]) for run in valid}) != 1:
            issues.append("repeat snapshot/trace hashes differ: " + str(group[0].parent.parent))
    return {"status": "pass" if not issues else "fail", "expected_fixture_ids": sorted(fixture_ids),
            "repeats": repeats, "expected_completed_runs": len(expected), "observed_completed_runs": len(paths),
            "missing_outputs": [str(path) for path in missing], "unexpected_outputs": [str(path) for path in unexpected],
            "issues": issues}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    corpus = args.corpus.resolve(strict=True)
    destination = args.output.resolve() if args.output else corpus / "measured_couplings.json"
    runner.local_output(destination.parent, create=False)
    destination.parent.mkdir(parents=True, exist_ok=True)
    corpus_receipt_path = corpus / "corpus_result.json"
    corpus_receipt = runner.read_json(corpus_receipt_path) if corpus_receipt_path.exists() else {}
    paths = sorted(path.parent for path in corpus.rglob("result.json") if path.parent.name == "output")
    runs = []
    for output in paths:
        try:
            result = observe(output)
        except (runner.ReplayError, OSError, KeyError, TypeError, ValueError) as error:
            result = {"status": "invalid-output", "output": str(output), "error": str(error)}
        runs.append(result)
        print(json.dumps({key: result[key] for key in ("status", "fixture_id", "output", "checks", "error") if key in result}), flush=True)
    completeness = corpus_completeness(corpus, corpus_receipt, paths, runs)
    receipt = {"schema": 1, "status": "pass" if completeness["status"] == "pass" and runs and
               all(run["status"] == "pass" for run in runs) else "fail",
               "corpus": str(corpus), "corpus_status": corpus_receipt.get("status"),
               "provenance": corpus_receipt.get("provenance"), "completeness": completeness,
               "completed_runs_found": len(paths), "runs": runs,
               "method": "Strict runner JSON/JSONL validation first; no log filtering or malformed-line recovery. Only completed result.json runs inspected.",
               "tick_convention": "Trace tick n names the interval committed in snapshot n+1.",
               "scope": "Observed selected semantic fields and event ingress, not complete engine state or audio hardware acceptance."}
    runner.write_json(destination, receipt)
    print(f"Measurement receipt {receipt['status']}: {destination}", flush=True)
    return 0 if receipt["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
