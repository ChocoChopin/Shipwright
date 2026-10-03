"""Exercise native fixture rejection before the game constructor, without assets.

Each case uses a fresh ignored workspace directory and expects native exit 2,
an initialization failure receipt, and no game/config/save initialization. This
tests the executable's parser rather than just the Python fixture validator.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from run_corpus import ROOT, ReplayError, file_digest, local_output, read_json, write_json


def base_fixture() -> dict:
    return {"schema": 1, "id": "native-validation", "category": "harness", "rate_hz": 20,
            "ticks": 1, "setup_ticks": 1, "seed": 1, "entrance": 187, "age": 1,
            "input": [{"time_num": 0, "time_den": 1, "sequence": 0, "buttons": 0,
                       "stick_x": 0, "stick_y": 0}]}


def cases() -> list[tuple[str, str, str]]:
    base = base_fixture()
    result: list[tuple[str, str, str]] = []

    def add(name: str, fixture: object, expected: str) -> None:
        result.append((name, json.dumps(fixture, allow_nan=False), expected))

    for name, key, value in (
        ("unsupported-schema", "schema", 2),
        ("unsupported-rate", "rate_hz", 30),
        ("fractional-rate", "rate_hz", 20.0),
        ("zero-ticks", "ticks", 0),
        ("fractional-ticks", "ticks", 1.5),
        ("boolean-ticks", "ticks", True),
        ("zero-setup", "setup_ticks", 0),
        ("invalid-age", "age", 2),
        ("invalid-entrance", "entrance", 0x7FFF),
        ("negative-seed", "seed", -1),
        ("oversize-seed", "seed", 0x100000000),
        ("invalid-presentation", "presentation_fps", 0),
        ("invalid-hud-timer", "hud_timer_seconds", 0),
        ("invalid-ocarina-round", "ocarina_memory_round", 3),
        ("invalid-message-id", "message_text_id", 65536),
        ("invalid-keese-recipe", "spawn_ice_keese", 1),
    ):
        fixture = copy.deepcopy(base)
        fixture[key] = value
        add(name, fixture, key)
    for name, key, value, expected in (
        ("zero-input-denominator", "time_den", 0, "time_den"),
        ("negative-input-time", "time_num", -1, "time_num"),
        ("fractional-input-time", "time_num", 0.1, "time_num"),
        ("invalid-stick", "stick_x", 128, "stick_x"),
        ("invalid-port", "port", 4, "port"),
        ("invalid-connection-type", "connected", 1, "connected"),
        ("unsupported-gyro", "gyro_x", 0, "gyro"),
    ):
        fixture = copy.deepcopy(base)
        fixture["input"][0][key] = value
        add(name, fixture, expected)
    fixture = copy.deepcopy(base)
    fixture["input"][0].update(connected=False, buttons=1)
    add("nonneutral-disconnected", fixture, "disconnected")
    fixture = copy.deepcopy(base)
    fixture["input"].append(dict(fixture["input"][0], time_num=1))
    add("duplicate-sequence", fixture, "sequence")
    fixture = copy.deepcopy(base)
    fixture["input"].extend((dict(fixture["input"][0], time_num=1, time_den=10, sequence=1),
                             dict(fixture["input"][0], time_num=1, time_den=20, sequence=2)))
    add("out-of-order-time", fixture, "timestamps")
    for name, value, expected in (
        ("invalid-position-size", {"pos": [0, 1]}, "three coordinates"),
        ("invalid-position-value", {"pos": [0, "NaN", 0]}, "coordinate"),
        ("position-out-of-bounds", {"pos": [32768, 0, 0]}, "coordinate"),
        ("yaw-out-of-bounds", {"yaw": 32768}, "yaw"),
        ("invalid-player-recipe", [], "initial_player"),
    ):
        fixture = copy.deepcopy(base)
        fixture["initial_player"] = value
        add(name, fixture, expected)
    fixture = copy.deepcopy(base)
    del fixture["input"]
    add("missing-input", fixture, "input")
    add("non-object-fixture", [], "JSON object")
    result.append(("malformed-json", "{not json}", "parse_error"))
    return result


def run_case(executable: Path, root: Path, name: str, text: str, expected: str,
             timeout: float, preserve_existing: str | None = None) -> dict:
    directory = root / name
    work, output, temporary = (directory / leaf for leaf in ("work", "output", "tmp"))
    for path in (work, output, temporary):
        path.mkdir(parents=True)
    fixture = directory / "fixture.json"
    fixture.write_text(text, encoding="utf-8", newline="\n")
    sentinel = b'{"previous_output":"must remain byte-identical"}\n'
    if preserve_existing:
        (output / preserve_existing).write_bytes(sentinel)
    command = [str(executable), "--native-sim-test", str(fixture), "--output", str(output)]
    if preserve_existing == "trace.jsonl":
        command.append("--trace")
    options = {}
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options["startupinfo"] = startup
    environment = os.environ.copy()
    environment.update(TEMP=str(temporary), TMP=str(temporary))
    started = time.monotonic()
    report = {"case": name, "command": command, "cwd": str(work), "expected_error": expected}
    with (directory / "process.log").open("w", encoding="utf-8") as log:
        try:
            process = subprocess.run(command, cwd=work, env=environment, stdout=log,
                                     stderr=subprocess.STDOUT, timeout=timeout, check=False, **options)
            report["exit_code"] = process.returncode
        except subprocess.TimeoutExpired:
            # subprocess.run kills/waits only for the child it created.
            report["error"] = "Timed out before native validation completed"
    report["host_seconds"] = time.monotonic() - started
    result_file = output / "result.json"
    no_game_artifacts = not any(work.iterdir()) and not (output / "snapshots.jsonl").exists()
    report["no_game_initialization_artifacts"] = no_game_artifacts
    preserved = True
    if preserve_existing:
        existing_file = output / preserve_existing
        preserved = existing_file.exists() and existing_file.read_bytes() == sentinel
        report["existing_output_file"] = preserve_existing
        report["existing_output_preserved"] = preserved
    if preserve_existing == "result.json":
        report["existing_result_preserved"] = preserved
        passed = report.get("exit_code") == 2 and no_game_artifacts and preserved
    else:
        try:
            receipt = read_json(result_file)
            report["native_result"] = receipt
            passed = (report.get("exit_code") == 2 and no_game_artifacts and preserved and receipt.get("status") == "fail"
                      and receipt.get("phase") == "initialization" and receipt.get("tick") == 0
                      and expected in receipt.get("error", ""))
        except (OSError, ReplayError) as error:
            report["error"] = str(error)
            passed = False
    report["status"] = "pass" if passed else "fail"
    write_json(directory / "validation.json", report)
    print(f"{name}: {report['status']}", flush=True)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "x64" / "Release" / "soh.exe")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=10)
    args = parser.parse_args()
    executable = args.exe.resolve(strict=True)
    if not 0 < args.timeout <= 60:
        raise ReplayError("Native-validation timeout must be positive and at most 60 seconds")
    output = local_output(args.output)
    identity = file_digest(executable)
    reports = [run_case(executable, output, *case, args.timeout) for case in cases()]
    reports.append(run_case(executable, output, "preserve-existing-result", "{}", "fresh directory",
                            args.timeout, preserve_existing="result.json"))
    reports.append(run_case(executable, output, "preserve-existing-trace", json.dumps(base_fixture()),
                            "fresh directory", args.timeout, preserve_existing="trace.jsonl"))
    unchanged = file_digest(executable) == identity
    passed = unchanged and all(report["status"] == "pass" for report in reports)
    receipt = {"schema": 1, "status": "pass" if passed else "fail", "executable": str(executable),
               "executable_sha256": identity, "executable_unchanged": unchanged, "cases": reports,
               "cases_passed": sum(report["status"] == "pass" for report in reports),
               "cases_total": len(reports)}
    write_json(output / "native-validation.json", receipt)
    print(f"Native fixture validation {receipt['status']}: {receipt['cases_passed']}/{len(reports)}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(json.dumps({"status": "infrastructure-error", "error": str(error)}), file=sys.stderr)
        raise SystemExit(2)
