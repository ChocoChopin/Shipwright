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
    boundary_checks = single_player_grants = next_world_grants = 0
    last_grant = None
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
        high = state.get("player_hz",20) != 20
        if state.get("status") == "paused" and (high or tick == sent) and state.get("sequence") == sent:
            if high and last_grant is not None:
                before, operation = last_grant
                q = 120//state['player_hz']
                offset = before['player_offset_q']
                amount = 1 if operation == 'step_player' else (6-offset)//q
                endpoint = operation == 'next_world' or offset+q == 6
                if (state['temporal']['player_step_id'] != before['temporal']['player_step_id']+amount or
                    state['world_gameplay_frames'] != before['world_gameplay_frames']+(offset == 0) or
                    state['temporal']['world_step_id'] != before['temporal']['world_step_id']+endpoint):
                    raise ReplayError('QA Player grant repeated or omitted Player/world work')
                boundary_checks += 1
            # Hold at the first three boundaries long enough to prove no implicit
            # catch-up/input consumption. Native code separately checks live state.
            if sent < 3:
                time.sleep(.12)
                again = json.loads(state_file.read_text(encoding="utf-8"))
                if again != state:
                    raise ReplayError("QA boundary changed while no step was granted")
                hold_checks += 1
            pending = output / "qa-command.pending.json"
            command = {"sequence":sent+1,"tick":tick,"operation":"step"}
            if high:
                offset = state['player_offset_q']
                operation = 'next_world' if tick >= 2 and tick % 2 == 0 and offset == 0 else 'step_player'
                command.update(operation=operation,player_offset_q=offset)
                single_player_grants += operation == 'step_player'
                next_world_grants += operation == 'next_world'
                last_grant = state, operation
            pending.write_text(json.dumps(command),encoding="utf-8")
            publish_command(pending,output/"qa-command.json",deadline)
            sent += 1
        time.sleep(.01)
    return {"steps_sent":sent,"hold_checks":hold_checks,"boundary_checks":boundary_checks,
            "single_player_grants":single_player_grants,"next_world_grants":next_world_grants}

def validate_temporal(output: Path, fixture: dict, single_step: bool) -> dict:
    result = read_json(output/"temporal-result.json")
    rows = [json.loads(line) for line in (output/"temporal.jsonl").read_text().splitlines()]
    ticks = fixture["ticks"]
    if result.get("status") != "pass" or len(rows) != ticks+1 or result.get("single_step") != single_step:
        raise ReplayError("Incomplete temporal diagnostics")
    first = rows[0]
    player_hz = fixture.get("player_hz",20)
    if player_hz != 20:
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
            if step.get('world_opportunities') != dict.fromkeys(
                    ('actors','collision','blink','scripts','environment','hud','message','audio'),1):
                raise ReplayError(f"World opportunity guard failed at Player step {index}")
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
        if single_step:
            commands = result.get('player_commands',[])
            cursor = 0
            for seq,command in enumerate(commands,1):
                if (command['sequence'] != seq or command['tick'] != cursor//6 or
                        command['player_offset_q'] != cursor%6):
                    raise ReplayError('Player QA command did not address the next due boundary')
                if command['operation'] == 'step_player': cursor += quanta
                elif command['operation'] == 'next_world': cursor += 6-cursor%6
                else: raise ReplayError('Unexpected automated Player QA operation')
            if (cursor != ticks*6 or result['qa_commands'] != len(commands) or
                    result['qa_holds'] != len(commands)):
                raise ReplayError('Incomplete Player QA command/hold coverage')
        return {"status":"pass","rows":len(rows),"player_steps":len(steps),"b_edge_latency":latency,
                "sha256":file_digest(output/"temporal.jsonl"),
                "player_steps_sha256":file_digest(output/"player-steps.jsonl"),"single_step":single_step}
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
