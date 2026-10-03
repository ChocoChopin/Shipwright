"""Characterize startup reliability with bounded sequential fresh processes.

Failures are retained, never retried or replaced. This is a startup campaign,
not a renderer fix or a substitute for the full canonical acceptance corpus.
Uses the existing replay launch/validation boundary without changing the engine.
"""
from __future__ import annotations

import argparse
import collections
import copy
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time

import run_corpus as replay

RESOURCE_EVENT = re.compile(
    r"Using D3D adapter|Reading archive|Adding Archive|Room Init|Scene Init|"
    r"Unable to load|ResourceManager|renderer|texture|TLUT|\[error\]|\[critical\]", re.IGNORECASE)
EXCEPTION = re.compile(r"Exception:\s*(0x[0-9a-fA-F]+)")


def atomic_json(path: Path, value: dict) -> None:
    pending = path.with_suffix(path.suffix + ".pending")
    replay.write_json(pending, value)
    pending.replace(path)


def make_cohorts(profile: str) -> list[dict]:
    """Construct local startup recipes; never rewrite committed oracle fixtures."""
    sources = ("gravity-fall",) if profile == "full-gravity" else ("gravity-fall", "startup-idle")
    cohorts = []
    for source_id in sources:
        source_path = replay.FIXTURES / (source_id + ".json")
        original = replay.validate_fixture(replay.read_json(source_path))
        if profile == "full-gravity":
            variants = [(20, True)]
        else:
            variants = [(fps, trace) for fps in (20, 60, 120) for trace in (True, False)]
        for fps, trace in variants:
            fixture = copy.deepcopy(original)
            if profile != "full-gravity":
                scene = "kokiri" if source_id == "gravity-fall" else "links-house"
                fixture = {key: fixture[key] for key in ("schema", "seed", "entrance", "age", "rate_hz")}
                fixture.update(id="startup-stress-" + scene, category="startup-reliability",
                               description="Fresh startup only: one setup and one measured canonical transaction.",
                               setup_ticks=1, ticks=1,
                               input=[{"time_num": 0, "time_den": 1, "sequence": 0,
                                       "buttons": 0, "stick_x": 0, "stick_y": 0}],
                               assertions=[{"kind": "counter_advance", "field": "global.gameplay_frames", "amount": 1}])
            fixture["presentation_fps"] = fps
            replay.validate_fixture(fixture)
            cohorts.append({"id": f"{fixture['id']}-fps{fps}-trace-{'on' if trace else 'off'}",
                            "fixture": fixture, "trace": trace, "source_fixture": str(source_path),
                            "source_fixture_sha256": replay.file_digest(source_path),
                            "scope": "complete gravity fixture" if profile == "full-gravity" else "startup only"})
    return cohorts


def scheduled_attempts(cohorts: list[dict], starts: int) -> list[dict]:
    # Round robin reduces confounding between one cohort and a long host-time block.
    return [{"ordinal": index + 1, "round": repetition, "cohort": cohort["id"], "status": "scheduled"}
            for index, (repetition, cohort) in enumerate(
                (repeat, cohort) for repeat in range(1, starts + 1) for cohort in cohorts)]


def progress_jsonl(path: Path, kind: str) -> dict:
    """Read a strict durable prefix for diagnosis only; malformed runs stay failed."""
    report = {"path": str(path), "exists": path.is_file(), "complete_records": 0,
              "initial_snapshot_observed": False, "measured_record_observed": False,
              "post_init_frame_observed": False, "last_record": None, "invalid_record": None}
    if not path.is_file():
        return report
    report.update(bytes=path.stat().st_size, sha256=replay.file_digest(path))
    with path.open("r", encoding="utf-8", errors="strict") as stream:
        try:
            for number, line in enumerate(stream, 1):
                try:
                    row = replay.strict_json(line, f"{path}:{number}")
                    replay.validate_values(row)
                    if not isinstance(row, dict) or type(row.get("schema")) is not int or row["schema"] != 1:
                        raise replay.ReplayError("record has no supported schema")
                    if type(row.get("tick")) is not int or row["tick"] < 0 or type(row.get("time_q")) is not int or row["time_q"] != row["tick"] * 6:
                        raise replay.ReplayError("record has invalid tick/time labels")
                    if kind == "trace" and (type(row.get("sequence")) is not int or row["sequence"] != number - 1):
                        raise replay.ReplayError("trace sequence is incomplete")
                    if kind == "snapshots" and row["tick"] != number - 1:
                        raise replay.ReplayError("snapshot sequence is incomplete")
                except (replay.ReplayError, TypeError) as error:
                    report["invalid_record"] = {"line": number, "error": str(error)}
                    break
                report["complete_records"] += 1
                report["last_record"] = {key: row[key] for key in
                    ("tick", "time_q", "sequence", "kind", "phase", "engine_frame", "measuring") if key in row}
                if kind == "snapshots":
                    report["initial_snapshot_observed"] |= row["tick"] == 0
                else:
                    report["measured_record_observed"] |= row.get("measuring") is True
                    report["post_init_frame_observed"] |= (row.get("kind") == "phase" and
                        row.get("phase") == "input_poll" and type(row.get("engine_frame")) is int and row["engine_frame"] >= 1)
        except UnicodeError as error:
            report["invalid_record"] = {"line": report["complete_records"] + 1, "error": str(error)}
    return report


def log_evidence(directory: Path) -> dict:
    paths = [directory / "process.log", *sorted((directory / "work" / "logs").glob("*.log"))]
    result = {"logs": [], "scene_callback_observed": False, "exception_codes": [],
              "tlut_handler_in_crash_log": False, "crash_metadata": []}
    for path in paths:
        if not path.is_file():
            continue
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        selected = [{"line": number, "text": line[:1800]} for number, line in enumerate(lines, 1)
                    if RESOURCE_EVENT.search(line)]
        result["logs"].append({"path": str(path), "bytes": path.stat().st_size, "sha256": replay.file_digest(path),
                               "resource_initialization_events": selected[:80], "matching_lines": len(selected),
                               "excerpts_truncated": len(selected) > 80})
        result["scene_callback_observed"] |= any("Scene Init - sceneNum:" in line for line in lines)
        result["exception_codes"].extend(code.lower() for line in lines for code in EXCEPTION.findall(line))
        result["tlut_handler_in_crash_log"] |= (any(EXCEPTION.search(line) for line in lines) and
                                                any("gfx_load_tlut_handler_rdp" in line for line in lines))
        capture = False
        excerpts = []
        for number, line in enumerate(lines, 1):
            if EXCEPTION.search(line) or line.strip() == "Traceback:" or "Native simulation graphics diagnostics" in line:
                capture = True
            if capture and len(excerpts) < 180:
                excerpts.append({"line": number, "text": line[:1800]})
        if excerpts:
            result["crash_metadata"].append({"path": str(path), "lines": excerpts,
                "scope": "Local crash diagnostics only; addresses do not enter semantic replay comparison."})
    result["exception_codes"] = sorted(set(result["exception_codes"]))
    return result


def observe_attempt(directory: Path, trace_enabled: bool, completed: bool) -> dict:
    snapshots = progress_jsonl(directory / "output" / "snapshots.jsonl", "snapshots")
    trace = progress_jsonl(directory / "output" / "trace.jsonl", "trace")
    logs = log_evidence(directory)
    measured = completed or snapshots["initial_snapshot_observed"] or trace["measured_record_observed"]
    initialized = measured or trace["post_init_frame_observed"]
    return {"scene_initialization_completed": True if initialized else None,
            "scene_callback_observed": logs["scene_callback_observed"],
            "measured_replay_began": True if measured else None,
            "progress_limit": "Unknown means no durable confirmation; buffered-output absence does not establish nonexecution.",
            "scene_completion_basis": "Completed snapshot/replay or input_poll phase after GameState_Init; scene callback log alone is insufficient.",
            "trace_requested": trace_enabled, "snapshot_progress": snapshots, "trace_progress": trace, **logs}


def verify_inputs(executable: Path, assets: dict, fixture_path: Path, provenance: dict, fixture_hash: str) -> None:
    if replay.file_digest(executable) != provenance["executable"]["sha256"] or any(
            replay.file_digest(path) != provenance["assets"][name]["sha256"] for name, path in assets.items()) or \
            replay.file_digest(fixture_path) != fixture_hash:
        raise replay.ReplayError("Executable, assets or copied fixture changed during the campaign")


def run_attempt(executable: Path, cohort: dict, directory: Path, assets: dict, timeout: float,
                reference: Path | None) -> dict:
    result = {"status": "started", "engine_completion_confirmed": False, "failure_kind": None,
              "directory": str(directory), "fixture_id": cohort["fixture"]["id"],
              "presentation_fps": cohort["fixture"]["presentation_fps"], "trace": cohort["trace"]}
    interrupted = False
    started = time.monotonic()
    try:
        invocation = replay.launch(executable, Path(cohort["fixture_path"]), directory, assets, timeout, cohort["trace"])
        if cohort["trace"]:
            replay.load_trace(directory / "output", cohort["fixture"]["ticks"])
        result.update(status="pass", engine_completion_confirmed=True)
        if reference is not None:
            comparison = replay.compare_runs(reference, directory / "output", include_trace=cohort["trace"])
            replay.write_json(directory / "comparison.json", comparison)
            result["comparison"] = comparison
            if comparison["status"] != "pass":
                result.update(status="failed", failure_kind="repeat-mismatch")
    except KeyboardInterrupt:
        # subprocess.run in the shared launcher kills/waits only its owned child.
        result.update(status="interrupted", failure_kind="interrupted", error="Campaign interrupted; attempt retained")
        interrupted = True
    except (OSError, replay.ReplayError, subprocess.SubprocessError) as error:
        result.update(status="failed", failure_kind="coverage-failure" if isinstance(error, replay.CoverageError)
                      else "invalid-output-or-launch", error=str(error))
    invocation_path = directory / "invocation.json"
    invocation = replay.read_json(invocation_path) if invocation_path.is_file() else {}
    result["invocation"] = invocation
    result["process_start_confirmed"] = "exit_code" in invocation or invocation.get("status") == "timeout"
    result["exit_code"] = invocation.get("exit_code")
    result["timed_out"] = invocation.get("status") == "timeout"
    result["host_seconds_diagnostic_only"] = invocation.get("host_seconds")
    result["attempt_host_seconds_diagnostic_only"] = time.monotonic() - started
    if result["timed_out"]:
        result["failure_kind"] = "timeout"
    elif invocation.get("exit_code") not in (None, 0):
        result["failure_kind"] = "nonzero-exit"
    result["observations"] = observe_attempt(directory, cohort["trace"], result["engine_completion_confirmed"])
    if result["observations"]["exception_codes"]:
        result.update(status="failed", failure_kind="native-exception")
    if interrupted:
        result["status"] = "interrupted"
    atomic_json(directory / "startup.json", result)
    return result


def summarize(attempts: list[dict], cohorts: list[dict]) -> dict:
    def counts(rows):
        executed = [row for row in rows if row["status"] not in ("scheduled", "started")]
        exceptions = [row for row in executed if row.get("observations", {}).get("exception_codes")]
        return {"scheduled": len(rows), "attempts_recorded": len(executed),
                "process_starts_confirmed": sum(row.get("process_start_confirmed", False) for row in executed),
                "successful_complete_replays": sum(row["status"] == "pass" for row in executed),
                "failed_or_interrupted_attempts": sum(row["status"] != "pass" for row in executed),
                "timeouts": sum(row.get("timed_out", False) for row in executed),
                "native_exception_attempts": len(exceptions),
                "tlut_fault_attempts": sum(row.get("observations", {}).get("tlut_handler_in_crash_log", False) for row in executed),
                "measured_replay_confirmed": sum(row.get("observations", {}).get("measured_replay_began") is True for row in executed),
                "failure_kinds": dict(collections.Counter(row["failure_kind"] for row in executed if row.get("failure_kind")))}
    return {"total": counts(attempts),
            "cohorts": {cohort["id"]: counts([row for row in attempts if row["cohort"] == cohort["id"]]) for cohort in cohorts}}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=replay.ROOT / "build/native-simulation-reference/soh-pass2-final.exe")
    parser.add_argument("--assets", type=Path, default=replay.ROOT / "build/x64/soh")
    parser.add_argument("--source-commit", required=True, help="Compiled runtime revision supplied by build owner; checkout HEAD is recorded separately")
    parser.add_argument("--expected-exe-sha256", help="Require reviewed executable bytes before planning or launching")
    parser.add_argument("--output", type=Path, required=True, help="Fresh ignored repository-local campaign directory")
    parser.add_argument("--profile", choices=("startup-matrix", "full-gravity"), default="startup-matrix")
    parser.add_argument("--cohort", action="append", help="Run only named cohort(s), preserving declared order")
    parser.add_argument("--starts-per-cohort", type=int, default=15)
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--plan-only", action="store_true", help="Print resolved plan and identities; do not create output or launch processes")
    args = parser.parse_args(argv)
    if os.name != "nt":
        raise replay.ReplayError("Startup stress must run as native Windows execution")
    if not 1 <= args.starts_per_cohort <= 1000 or not math.isfinite(args.timeout) or not 1 <= args.timeout <= 300:
        raise replay.ReplayError("Require 1..1000 starts per cohort and a finite 1..300-second host timeout")
    cohorts = make_cohorts(args.profile)
    if args.cohort:
        if len(set(args.cohort)) != len(args.cohort) or set(args.cohort) - {row["id"] for row in cohorts}:
            raise replay.ReplayError("Unknown or duplicated cohort selection")
        cohorts = [cohort for cohort in cohorts if cohort["id"] in args.cohort]
    executable = args.exe.resolve(strict=True)
    asset_directory = args.assets.resolve(strict=True)
    assets = {name: (asset_directory / name).resolve(strict=True) for name in ("oot.o2r", "soh.o2r")}
    if (asset_directory / "gamecontrollerdb.txt").is_file():
        assets["gamecontrollerdb.txt"] = (asset_directory / "gamecontrollerdb.txt").resolve()
    source_commit = replay.git_capture("rev-parse", "--verify", args.source_commit + "^{commit}")
    output = replay.local_output(args.output, create=False)
    if output.exists():
        raise replay.ReplayError("Campaign output must be fresh; retained failures cannot be overwritten")
    provenance = replay.provenance(executable, assets)
    if args.expected_exe_sha256 is not None and (not re.fullmatch(r"[0-9a-fA-F]{64}", args.expected_exe_sha256) or
            provenance["executable"]["sha256"] != args.expected_exe_sha256.lower()):
        raise replay.ReplayError("Executable does not match the requested SHA-256 identity")
    attempts = scheduled_attempts(cohorts, args.starts_per_cohort)
    receipt = {"schema": 1, "status": "planned" if args.plan_only else "started", "profile": args.profile,
               "created_utc": datetime.now(timezone.utc).isoformat(), "output": str(output),
               "runtime_source_commit_declared": source_commit, "provenance": provenance,
               "source_identity_limit": "Declared compiled-runtime commit and exact executable bytes are separate from checkout/tooling provenance; no embedded build-stamp inference.",
               "timeout_seconds": args.timeout, "starts_per_cohort": args.starts_per_cohort,
               "schedule": "Sequential round robin; one fresh process per scheduled attempt; no retries or replacement runs.",
               "host_time_policy": "Duration is diagnostic only; fixture ticks own authoritative simulation time.",
               "reliability_conclusion": "Characterization only. No reproduced fault does not establish a fix; failures are never discarded.",
               "cohorts": cohorts, "attempts": attempts}
    if args.plan_only:
        print(json.dumps(receipt, indent=2))
        return 0
    replay.local_output(output)
    for cohort in cohorts:
        fixture_path = output / "inputs" / (cohort["id"] + ".json")
        replay.write_json(fixture_path, cohort["fixture"])
        cohort.update(fixture_path=str(fixture_path), fixture_sha256=replay.file_digest(fixture_path))
    atomic_json(output / "stress_result.json", receipt)
    cohort_by_id = {row["id"]: row for row in cohorts}
    references = {}
    interrupted = False
    try:
        for attempt in attempts:
            cohort = cohort_by_id[attempt["cohort"]]
            verify_inputs(executable, assets, Path(cohort["fixture_path"]), provenance, cohort["fixture_sha256"])
            directory = output / cohort["id"] / f"attempt-{attempt['round']:04d}"
            attempt.update(status="started", directory=str(directory))
            atomic_json(output / "stress_result.json", receipt)
            print(f"Startup {attempt['ordinal']}/{len(attempts)}: {cohort['id']} round {attempt['round']}", flush=True)
            result = run_attempt(executable, cohort, directory, assets, args.timeout, references.get(cohort["id"]))
            attempt.update(result)
            if result["status"] == "pass" and cohort["id"] not in references:
                references[cohort["id"]] = directory / "output"
            receipt["summary"] = summarize(attempts, cohorts)
            atomic_json(output / "stress_result.json", receipt)
            print(f"  {result['status']}; exit={result['exit_code']} timeout={result['timed_out']} failure={result['failure_kind']}", flush=True)
            verify_inputs(executable, assets, Path(cohort["fixture_path"]), provenance, cohort["fixture_sha256"])
            if result["status"] == "interrupted":
                interrupted = True
                break
    except KeyboardInterrupt:
        interrupted = True
    except (OSError, replay.ReplayError, subprocess.SubprocessError) as error:
        receipt["campaign_error"] = str(error)
    receipt["summary"] = summarize(attempts, cohorts)
    total = receipt["summary"]["total"]
    receipt["status"] = "interrupted" if interrupted else "incomplete" if total["attempts_recorded"] != len(attempts) or \
        "campaign_error" in receipt else "failures-observed" if total["failed_or_interrupted_attempts"] else "all-starts-completed"
    receipt["completed_utc"] = datetime.now(timezone.utc).isoformat()
    atomic_json(output / "stress_result.json", receipt)
    print(f"Startup campaign {receipt['status']}: {output / 'stress_result.json'}", flush=True)
    return 0 if receipt["status"] == "all-starts-completed" else 130 if interrupted else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, replay.ReplayError, subprocess.SubprocessError) as error:
        print(json.dumps({"status": "infrastructure-error", "error": str(error)}, indent=2))
        sys.exit(2)
