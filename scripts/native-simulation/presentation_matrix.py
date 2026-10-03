"""Repeat canonical simulation at selected presentation FPS and with tracing off.

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


def fixture_selections(presentation_paths=None, trace_disabled_paths=None) -> tuple[dict, dict]:
    def load(paths, defaults, label):
        selected = {}
        for path in paths if paths is not None else [replay.FIXTURES / (name + ".json") for name in defaults]:
            path = Path(path).resolve(strict=True)
            fixture = replay.validate_fixture(replay.read_json(path))
            fixture_id = fixture["id"]
            if fixture_id in selected:
                raise replay.ReplayError(f"Duplicate {label} fixture identity: {fixture_id}")
            selected[fixture_id] = {"path": str(path), "sha256": replay.file_digest(path), "fixture": fixture}
        if not selected:
            raise replay.ReplayError(f"No {label} fixtures selected")
        return selected
    presentation = load(presentation_paths, PRESENTATION_FIXTURES, "presentation")
    trace_disabled = load(trace_disabled_paths, ("startup-idle",), "trace-disabled")
    for fixture_id in presentation.keys() & trace_disabled.keys():
        if replay.digest(presentation[fixture_id]["fixture"]) != replay.digest(trace_disabled[fixture_id]["fixture"]):
            raise replay.ReplayError(f"Conflicting presentation/trace-disabled fixture content: {fixture_id}")
    return presentation, trace_disabled


def check_reference_fixture(declared: dict, recorded: dict) -> None:
    if replay.digest(declared) != replay.digest(recorded) or declared.get("presentation_fps", 20) != 20:
        raise replay.ReplayError(f"Canonical fixture differs from reference or is not 20 presentation FPS: {declared['id']}")


def matrix_cases(presentation: dict, trace_disabled: dict, rates: list[int]) -> list[tuple]:
    if not rates or len(set(rates)) != len(rates) or any(type(rate) is not int or rate not in (20, 60, 120) for rate in rates):
        raise replay.ReplayError("Presentation FPS must be distinct values selected from 20, 60 and 120")
    return [(fixture_id, fps, True) for fps in rates for fixture_id in presentation] + \
           [(fixture_id, 20, False) for fixture_id in trace_disabled]


def check_asset_files(assets: Path, expected: dict) -> None:
    for name, identity in expected.items():
        if replay.file_digest(assets / name) != identity["sha256"]:
            raise replay.ReplayError(f"Presentation matrix asset differs from reference: {name}")


def check_case_assets(actual: dict, expected: dict) -> None:
    """Bind each process cohort to the original assets, including between cases."""
    if not isinstance(actual, dict) or actual.keys() != expected.keys() or any(
            not isinstance(actual[name], dict) or
            any(actual[name].get(field) != identity[field] for field in ("sha256", "bytes"))
            for name, identity in expected.items()):
        raise replay.ReplayError("Presentation matrix case assets differ from the canonical reference")


def check_case_provenance(actual: dict, executable_hash: str, expected_assets: dict) -> None:
    if not isinstance(actual, dict) or not isinstance(actual.get("executable"), dict) or \
            actual["executable"].get("sha256") != executable_hash:
        raise replay.ReplayError("Presentation matrix case executable differs from the canonical reference")
    check_case_assets(actual.get("assets"), expected_assets)


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
    parser.add_argument("--fail-fast", action="store_true", help="Preserve the first failed case and stop before launching another")
    parser.add_argument("--fixture", type=Path, action="append", help="Repeatable presentation fixture path; defaults to sword, HUD and Keese")
    parser.add_argument("--trace-disabled-fixture", type=Path, action="append", help="Repeatable trace-disabled fixture path; defaults to startup-idle")
    parser.add_argument("--presentation-fps", type=int, choices=(20, 60, 120), action="append", help="Repeatable presentation rate; defaults to 60 and 120")
    args = parser.parse_args(argv)
    if args.timeout <= 0:
        raise replay.ReplayError("Timeout must be positive")
    presentation, trace_disabled = fixture_selections(args.fixture, args.trace_disabled_fixture)
    selected = {**presentation, **trace_disabled}
    rates = args.presentation_fps if args.presentation_fps is not None else [60, 120]
    cases = matrix_cases(presentation, trace_disabled, rates)
    reference = args.reference.resolve(strict=True)
    executable = args.exe.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    reference_receipt = replay.read_json(reference / "corpus_result.json")
    if reference_receipt.get("status") != "pass" or reference_receipt.get("rate_hz") != 20:
        raise replay.ReplayError("Reference must be a passing canonical 20-Hz corpus")
    expected_executable_hash = reference_receipt["provenance"]["executable"]["sha256"]
    if replay.file_digest(executable) != expected_executable_hash:
        raise replay.ReplayError("Presentation matrix requires exactly the reference executable bytes")
    expected_assets = reference_receipt["provenance"]["assets"]
    check_asset_files(assets, expected_assets)
    fixture_status = {item["id"]: item["status"] for item in reference_receipt["fixtures"]}
    canonical = {}
    for fixture_id, selection in selected.items():
        if fixture_status.get(fixture_id) != "pass":
            raise replay.ReplayError(f"Reference fixture did not pass: {fixture_id}")
        declared = selection["fixture"]
        recorded = replay.read_json(reference / fixture_id / "fixture.json")
        check_reference_fixture(declared, recorded)
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
               "fail_fast": args.fail_fast,
               "selected_fixture_sources": selected, "presentation_fps": rates,
               "allowed_presentation_input_changes": ["fixture.presentation_fps", "configuration.interpolation_fps"],
               "semantic_comparison": "Every complete snapshot exactly; no tolerance, field removal or interpolation",
               "cases": []}
    replay.write_json(output / "matrix_result.json", receipt)
    exit_code = replay.PASS
    for fixture_id, fps, tracing in cases:
        label = f"{fixture_id}-fps{fps}" if tracing else f"{fixture_id}-trace-disabled"
        variant = copy.deepcopy(canonical[fixture_id])
        if tracing:
            variant["presentation_fps"] = fps
        fixture_path = inputs / (label + ".json")
        replay.write_json(fixture_path, variant)
        replay.validate_fixture(variant)
        # This assertion describes the derivation itself, independently of the
        # engine and comparison manifests checked by the underlying runner.
        restored = copy.deepcopy(variant)
        if "presentation_fps" in canonical[fixture_id]:
            restored["presentation_fps"] = canonical[fixture_id]["presentation_fps"]
        else:
            restored.pop("presentation_fps", None)
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
                check_case_provenance(case_receipt.get("provenance"), expected_executable_hash, expected_assets)
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
        if args.fail_fast and exit_code != replay.PASS:
            receipt["stop_reason"] = f"Stopped after failed case: {label}"
            break

    if replay.file_digest(executable) != expected_executable_hash or replay.file_digest(
            reference / "corpus_result.json") != receipt["reference_receipt_sha256"]:
        receipt["error"] = "Executable or canonical reference receipt changed during the matrix"
        exit_code = replay.INFRASTRUCTURE
    try:
        check_asset_files(assets, expected_assets)
    except (OSError, replay.ReplayError) as error:
        receipt["error"] = str(error)
        exit_code = replay.INFRASTRUCTURE
    receipt.update(status={replay.PASS: "pass", replay.MISMATCH: "mismatch",
                           replay.INFRASTRUCTURE: "infrastructure-error"}[exit_code],
                   cases_completed=sum(case["status"] == "pass" for case in receipt["cases"]),
                   cases_attempted=len(receipt["cases"]),
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
