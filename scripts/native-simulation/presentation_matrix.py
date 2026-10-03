"""Repeat canonical simulation at 60/120 presentation FPS and with tracing off.

Requires a passing canonical corpus produced by the same executable and assets.
Each case runs three fresh processes. No fixture other than presentation_fps is
changed, and every semantic snapshot remains an exact bitwise comparison.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import sys

import run_corpus as replay

PRESENTATION_FIXTURES = ("animation-sword", "hud-countdown", "draw-rng-keese")
REPEATS = 3


def check_presentation_events(directory: Path, ticks: int, fps: int) -> dict:
    rows = replay.load_trace(directory, ticks)
    events = [row for row in rows if row.get("kind") == "presentation-count" and row.get("measuring") is True]
    expected = fps // 20
    tick_sequence = [row["tick"] for row in events]
    observed = sorted({row.get("value") for row in events}, key=repr)
    passed = tick_sequence == list(range(ticks)) and all(
        type(row.get("value")) is int and row["value"] == expected for row in events)
    return {"status": "pass" if passed else "mismatch", "presentation_fps": fps,
            "simulation_hz": 20, "expected_events": ticks, "observed_events": len(events),
            "expected_count_per_transaction": expected, "observed_counts": observed,
            "total_requested_display_list_replays": sum(row["value"] for row in events if type(row.get("value")) is int),
            "tick_sequence_complete": tick_sequence == list(range(ticks)),
            "measurement": "Measured presentation-count events before RunCommands; this does not attest GPU completion."}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True, help="Passing canonical 20-FPS corpus root")
    parser.add_argument("--output", type=Path, required=True, help="Fresh ignored repository-local matrix directory")
    parser.add_argument("--exe", type=Path, default=replay.ROOT / "x64" / "Release" / "soh.exe")
    parser.add_argument("--assets", type=Path, default=replay.ROOT / "build" / "x64" / "soh")
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args(argv)
    if args.timeout <= 0:
        raise replay.ReplayError("Timeout must be positive")
    reference = args.reference.resolve(strict=True)
    executable = args.exe.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    reference_receipt = replay.read_json(reference / "corpus_result.json")
    if reference_receipt.get("status") != "pass" or reference_receipt.get("rate_hz") != 20:
        raise replay.ReplayError("Reference must be a passing canonical 20-Hz corpus")
    expected_executable_hash = reference_receipt["provenance"]["executable"]["sha256"]
    if replay.file_digest(executable) != expected_executable_hash:
        raise replay.ReplayError("Presentation matrix requires exactly the reference executable bytes")
    for name, identity in reference_receipt["provenance"]["assets"].items():
        if replay.file_digest(assets / name) != identity["sha256"]:
            raise replay.ReplayError(f"Presentation matrix asset differs from reference: {name}")
    fixture_status = {item["id"]: item["status"] for item in reference_receipt["fixtures"]}
    canonical = {}
    for fixture_id in (*PRESENTATION_FIXTURES, "startup-idle"):
        if fixture_status.get(fixture_id) != "pass":
            raise replay.ReplayError(f"Reference fixture did not pass: {fixture_id}")
        declared = replay.validate_fixture(replay.read_json(replay.FIXTURES / (fixture_id + ".json")))
        recorded = replay.read_json(reference / fixture_id / "fixture.json")
        if replay.digest(declared) != replay.digest(recorded) or declared.get("presentation_fps", 20) != 20:
            raise replay.ReplayError(f"Canonical fixture differs from reference or is not 20 presentation FPS: {fixture_id}")
        manifest, _ = replay.load_run(reference / fixture_id / "run-001" / "output")
        replay.validate_requested_fixture(manifest, declared)
        # The canonical side must actually carry the requested verbose evidence.
        replay.load_trace(reference / fixture_id / "run-001" / "output", declared["ticks"])
        canonical[fixture_id] = declared

    output = replay.local_output(args.output)
    inputs = output / "inputs"
    inputs.mkdir()
    temporary = output / "tmp"
    temporary.mkdir()
    env = os.environ.copy()
    env.update(TEMP=str(temporary), TMP=str(temporary))
    receipt = {"schema": 1, "status": "started", "reference": str(reference), "output": str(output),
               "source_head": replay.git_capture("rev-parse", "HEAD"), "executable_sha256": expected_executable_hash,
               "reference_receipt_sha256": replay.file_digest(reference / "corpus_result.json"),
               "repeats_per_case": REPEATS, "simulation_hz": 20,
               "allowed_presentation_input_changes": ["fixture.presentation_fps", "configuration.interpolation_fps"],
               "semantic_comparison": "Every complete snapshot exactly; no tolerance, field removal or interpolation",
               "cases": []}
    replay.write_json(output / "matrix_result.json", receipt)
    cases = [(fixture_id, fps, True) for fps in (60, 120) for fixture_id in PRESENTATION_FIXTURES]
    cases.append(("startup-idle", 20, False))
    exit_code = replay.PASS
    for fixture_id, fps, tracing in cases:
        label = f"{fixture_id}-fps{fps}" if tracing else "startup-idle-trace-disabled"
        variant = copy.deepcopy(canonical[fixture_id])
        if tracing:
            variant["presentation_fps"] = fps
        fixture_path = inputs / (label + ".json")
        replay.write_json(fixture_path, variant)
        replay.validate_fixture(variant)
        # This assertion describes the derivation itself, independently of the
        # engine and comparison manifests checked by the underlying runner.
        restored = copy.deepcopy(variant)
        restored["presentation_fps"] = canonical[fixture_id].get("presentation_fps", 20)
        if replay.digest(restored) != replay.digest(canonical[fixture_id]):
            raise replay.ReplayError("Presentation fixture derivation changed an unrelated field")
        case_root = output / label
        command = [sys.executable, "-B", str(Path(replay.__file__).resolve()), "run", "--exe", str(executable),
                   "--assets", str(assets), "--fixture", str(fixture_path), "--output", str(case_root),
                   "--repeats", str(REPEATS), "--timeout", str(args.timeout)]
        if tracing:
            command.append("--trace")
        case = {"id": label, "fixture_id": fixture_id, "presentation_fps": fps, "trace_enabled": tracing,
                "status": "started", "command": command, "fixture_sha256": replay.file_digest(fixture_path),
                "comparisons": []}
        receipt["cases"].append(case)
        replay.write_json(output / "matrix_result.json", receipt)
        print(f"Presentation matrix {len(receipt['cases'])}/{len(cases)}: {label}", flush=True)
        try:
            # run_corpus owns each engine timeout, isolated cwd, output hashes,
            # exact input attestation, behavioral assertions and repeat gate.
            completed = subprocess.run(command, cwd=replay.ROOT, env=env, check=False)
            case["runner_exit_code"] = completed.returncode
            case_receipt = replay.read_json(case_root / "corpus_result.json")
            if completed.returncode != replay.PASS or case_receipt.get("status") != "pass":
                case.update(status=case_receipt.get("status", "infrastructure-error"),
                            failure_receipt=str(case_root / "corpus_result.json"))
                exit_code = max(exit_code, replay.MISMATCH if completed.returncode == replay.MISMATCH else replay.INFRASTRUCTURE)
            else:
                case["status"] = "pass"
                reference_run = reference / fixture_id / "run-001" / "output"
                for repetition in range(1, REPEATS + 1):
                    run = case_root / fixture_id / f"run-{repetition:03d}" / "output"
                    comparison = replay.compare_runs(reference_run, run, allow_presentation_difference=tracing)
                    comparison["repetition"] = repetition
                    if tracing:
                        comparison["presentation_events"] = check_presentation_events(run, variant["ticks"], fps)
                        if comparison["presentation_events"]["status"] != "pass":
                            comparison["status"] = "mismatch"
                    elif (run / "trace.jsonl").exists():
                        raise replay.ReplayError("Trace-disabled case unexpectedly emitted trace.jsonl")
                    replay.write_json(run / "canonical-comparison.json", comparison)
                    case["comparisons"].append(comparison)
                    if comparison["status"] != "pass":
                        case["status"] = "mismatch"
                        exit_code = max(exit_code, replay.MISMATCH)
                if replay.file_digest(fixture_path) != case["fixture_sha256"]:
                    raise replay.ReplayError("Derived fixture changed during the matrix case")
        except (OSError, replay.ReplayError, subprocess.SubprocessError) as error:
            case.update(status="infrastructure-error", error=str(error))
            exit_code = replay.INFRASTRUCTURE
        replay.write_json(output / "matrix_result.json", receipt)
        print(f"{label}: {case['status']}", flush=True)

    if replay.file_digest(executable) != expected_executable_hash or replay.file_digest(
            reference / "corpus_result.json") != receipt["reference_receipt_sha256"]:
        receipt["error"] = "Executable or canonical reference receipt changed during the matrix"
        exit_code = replay.INFRASTRUCTURE
    receipt.update(status={replay.PASS: "pass", replay.MISMATCH: "mismatch",
                           replay.INFRASTRUCTURE: "infrastructure-error"}[exit_code],
                   cases_completed=sum(case["status"] == "pass" for case in receipt["cases"]),
                   cases_requested=len(cases), processes_requested=len(cases) * REPEATS)
    replay.write_json(output / "matrix_result.json", receipt)
    print(f"Presentation matrix {receipt['status']}: {output / 'matrix_result.json'}", flush=True)
    return exit_code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, replay.ReplayError, subprocess.SubprocessError) as error:
        print(json.dumps({"schema": 1, "status": "infrastructure-error", "error": str(error)}, indent=2))
        raise SystemExit(replay.INFRASTRUCTURE)
