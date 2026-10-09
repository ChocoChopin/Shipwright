"""Bounded file-command QA driver and temporal receipt checks; no game math."""
import json
import os
from pathlib import Path
import subprocess
import time
from run_corpus import ReplayError, read_json, file_digest

def compare_temporal(reference: Path, candidate: Path) -> dict:
    """Exact separate stream; no ignored fields or cross-rate tolerances."""
    before = file_digest(reference / "temporal.jsonl")
    after = file_digest(candidate / "temporal.jsonl")
    result = {"status":"pass" if before == after else "mismatch",
              "reference_sha256":before,"candidate_sha256":after}
    first_steps, next_steps = reference/"player-steps.jsonl", candidate/"player-steps.jsonl"
    if first_steps.exists() or next_steps.exists():
        result["player_steps_match"] = (first_steps.exists() and next_steps.exists() and
                                        file_digest(first_steps) == file_digest(next_steps))
        if not result["player_steps_match"]: result["status"] = "mismatch"
    return result

def publish_command(pending: Path, destination: Path, deadline: float) -> None:
    # Windows readers may briefly deny delete sharing while parsing the previous
    # command. Keep the same atomic payload/sequence and retry within this run's
    # existing timeout; never turn a transient publication lock into a game fault.
    while True:
        try:
            os.replace(pending,destination)
            return
        except PermissionError:
            if time.monotonic() >= deadline:
                raise ReplayError("QA atomic command publication remained locked until timeout")
            time.sleep(.01)

def drive_steps(process, output: Path, timeout: float) -> dict:
    deadline = time.monotonic() + timeout
    sent = 0
    hold_checks = 0
    while process.poll() is None:
        if time.monotonic() >= deadline:
            raise subprocess.TimeoutExpired(process.args, timeout)
        state_file = output / "qa-state.json"
        try:
            state = json.loads(state_file.read_text(encoding="utf-8"))
        except (FileNotFoundError, json.JSONDecodeError, PermissionError):
            time.sleep(.01)
            continue
        tick = state.get("tick")
        if state.get("status") == "paused" and tick == sent and state.get("sequence") == sent:
            # Hold at the first three boundaries long enough to prove no implicit
            # catch-up/input consumption. Native code separately checks live state.
            if sent < 3:
                time.sleep(.12)
                again = json.loads(state_file.read_text(encoding="utf-8"))
                if again != state:
                    raise ReplayError("QA boundary changed while no step was granted")
                hold_checks += 1
            pending = output / "qa-command.pending.json"
            pending.write_text(json.dumps({"sequence":sent+1,"tick":tick,"operation":"step"}),encoding="utf-8")
            publish_command(pending,output/"qa-command.json",deadline)
            sent += 1
        time.sleep(.01)
    return {"steps_sent":sent,"hold_checks":hold_checks}

def validate_temporal(output: Path, fixture: dict, single_step: bool) -> dict:
    result = read_json(output/"temporal-result.json")
    rows = [json.loads(line) for line in (output/"temporal.jsonl").read_text().splitlines()]
    ticks = fixture["ticks"]
    if result.get("status") != "pass" or len(rows) != ticks+1 or result.get("single_step") != single_step:
        raise ReplayError("Incomplete temporal diagnostics")
    first = rows[0]
    player_hz = fixture.get("player_hz",20)
    if player_hz != 20:
        if single_step:
            raise ReplayError("High-rate QA stepping acceptance is not implemented yet")
        per_world, quanta = player_hz//20, 120//player_hz
        for i,row in enumerate(rows):
            expected_hz = player_hz if i else 20
            if (row["tick"] != i or row["fixture_time_q"] != 6*i or not row["okay"] or
                row["effective_player_hz"] != expected_hz or row["world_hz"] != 20 or
                row["player_high_rate_admitted"] != bool(i) or row["contact_bridge_active"] or
                row["contact_queue_count"] != 0 or row["time_q"] != first["time_q"]+6*i or
                row["world_step_id"] != first["world_step_id"]+i or
                row["player_time_q"] != first["player_time_q"]+6*i or
                row["player_step_id"] != first["player_step_id"]+per_world*i or row["queued_input_samples"]):
                raise ReplayError(f"High-rate boundary invariant failed at tick {i}: {row.get('high_rate_rejection')}")
        steps = [json.loads(line) for line in (output/"player-steps.jsonl").read_text().splitlines()]
        if len(steps) != ticks*per_world:
            raise ReplayError("Incomplete high-rate Player intervals")
        for index,step in enumerate(steps):
            state = step["temporal"]
            if (step["tick"] != index//per_world or not state["okay"] or
                state["effective_player_hz"] != player_hz or
                state["player_step_id"] != first["player_step_id"]+index+1 or
                state["player_interval_start_q"] != first["time_q"]+index*quanta or
                state["player_interval_end_q"] != first["time_q"]+(index+1)*quanta or
                state["world_step_id"] != first["world_step_id"]+index//per_world or
                step["world_gameplay_frames"] != steps[0]["world_gameplay_frames"]+index//per_world):
                raise ReplayError(f"Player interval order/world multiplication at step {index}")
        if result["canonical_time_q"] != ticks*6 or result["canonical_transaction_id"] != ticks:
            raise ReplayError("World transaction count drift")
        latency = None
        if "expected_attack_edge_q" in fixture:
            edge = fixture["expected_attack_edge_q"]
            due = ((edge+quanta-1)//quanta)*quanta
            if edge % 6 == 0 or due % 6 == 0:
                raise ReplayError("Latency fixture must exercise an intermediate Player boundary")
            attacks = [step for step in steps if step["temporal"]["attack_epoch"] > first["attack_epoch"]]
            if not attacks or attacks[0]["temporal"]["player_interval_start_q"] != first["time_q"]+due:
                raise ReplayError("B edge did not begin attack on the next intermediate Player interval")
            attack = attacks[0]["temporal"]
            if (attack["last_input"]["pressed"] & 16384) == 0 or attack["consumed_input_edges"] != 1:
                raise ReplayError("Attack did not consume the B edge exactly once")
            latency = {"edge_q":edge,"attack_start_q":due,"latency_q":due-edge,
                       "next_world_q":((edge//6)+1)*6}
        return {"status":"pass","rows":len(rows),"player_steps":len(steps),"b_edge_latency":latency,
                "sha256":file_digest(output/"temporal.jsonl"),
                "player_steps_sha256":file_digest(output/"player-steps.jsonl"),"single_step":False}
    for i,row in enumerate(rows):
        if (row["tick"] != i or row["fixture_time_q"] != 6*i or not row["okay"] or
            row["effective_player_hz"] != 20 or row["world_hz"] != 20 or row["player_high_rate_admitted"] or
            row["contact_bridge_active"] or row["contact_queue_count"] != 0 or
            row["time_q"] != first["time_q"]+6*i or row["world_step_id"] != first["world_step_id"]+i):
            raise ReplayError(f"Temporal canonical invariant failed at tick {i}")
        if fixture.get("observe_player_state") and (
            row["player_time_q"] != first["player_time_q"]+6*i or
            row["player_step_id"] != first["player_step_id"]+i or row["queued_input_samples"] != 0):
            raise ReplayError(f"Player temporal context/input invariant failed at tick {i}")
    if result["canonical_time_q"] != ticks*6 or result["canonical_transaction_id"] != ticks:
        raise ReplayError("Temporal QA canonical commit count mismatch")
    if single_step and (result["qa_commands"] != ticks or result["qa_holds"] != ticks):
        raise ReplayError("Single-step controller did not pause/grant each transaction")
    return {"status":"pass","rows":len(rows),"sha256":file_digest(output/"temporal.jsonl"),
            "single_step":single_step,"attack_epochs":sorted({r["attack_epoch"] for r in rows})}
