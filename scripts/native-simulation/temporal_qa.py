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
    return {"status":"pass" if before == after else "mismatch",
            "reference_sha256":before,"candidate_sha256":after}

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
            os.replace(pending,output/"qa-command.json")
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
