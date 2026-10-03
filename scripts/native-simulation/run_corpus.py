"""Fresh-process canonical replay runner and strict semantic JSON comparator.

Only 20 Hz is admitted. Outputs are local diagnostic data, never portable saves.
Exit status: 0 passed, 1 semantic mismatch, 2 fixture/runtime infrastructure error.
"""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
from typing import Any
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).with_name("fixtures")
SCHEMA = 1
PASS, MISMATCH, INFRASTRUCTURE = 0, 1, 2


class ReplayError(ValueError):
    """Invalid fixture, incomplete output, or failed execution infrastructure."""


class CoverageError(ReplayError):
    """The completed replay did not exercise its declared fixture behavior."""


def strict_json(text: str, source: str = "JSON") -> Any:
    def pairs(items: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in items:
            if key in result:
                raise ReplayError(f"{source}: duplicate key {key!r}")
            result[key] = value
        return result

    def constant(value: str) -> None:
        raise ReplayError(f"{source}: non-finite value {value}")

    try:
        return json.loads(text, object_pairs_hook=pairs, parse_constant=constant)
    except (ValueError, TypeError) as error:
        raise ReplayError(f"{source}: {error}") from error


def read_json(path: Path) -> Any:
    return strict_json(path.read_text(encoding="utf-8"), str(path))


def validate_values(value: Any, path: str = "$") -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise ReplayError(f"{path}: non-finite authoritative value")
    if isinstance(value, dict):
        if "bits" in value and "value" in value:
            bits = value["bits"]
            if not isinstance(bits, str) or not re.fullmatch(r"(?:0x)?[0-9a-fA-F]{8}", bits):
                raise ReplayError(f"{path}.bits: expected IEEE-754 binary32 hexadecimal")
            decoded = struct.unpack("!f", bytes.fromhex(bits.removeprefix("0x")))[0]
            if not math.isfinite(decoded):
                raise ReplayError(f"{path}.bits: non-finite IEEE-754 binary32")
            decimal = value["value"]
            if isinstance(decimal, bool) or not isinstance(decimal, (int, float)):
                raise ReplayError(f"{path}.value: expected readable finite decimal")
            try:
                roundtrip = struct.pack("!f", decimal).hex()
            except (OverflowError, struct.error) as error:
                raise ReplayError(f"{path}.value: invalid binary32 decimal") from error
            if roundtrip != bits.removeprefix("0x").lower():
                raise ReplayError(f"{path}: readable decimal does not round-trip to bits")
        for key, item in value.items():
            validate_values(item, f"{path}.{key}")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            validate_values(item, f"{path}[{index}]")


def canonical_bytes(value: Any) -> bytes:
    validate_values(value)
    return json.dumps(value, sort_keys=True, ensure_ascii=False, allow_nan=False,
                      separators=(",", ":")).encode("utf-8")


def digest(value: Any) -> str:
    return hashlib.sha256(canonical_bytes(value)).hexdigest()


def file_digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    pending = path.with_suffix(path.suffix + ".pending")
    pending.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8", newline="\n")
    pending.replace(path)


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        for row in rows:
            stream.write(canonical_bytes(row).decode("utf-8") + "\n")


def integer(value: Any, label: str, minimum: int, maximum: int) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ReplayError(f"{label}: expected integer in [{minimum}, {maximum}]")
    return value


def validate_fixture(fixture: Any) -> dict[str, Any]:
    if not isinstance(fixture, dict) or type(fixture.get("schema")) is not int or fixture["schema"] != SCHEMA:
        raise ReplayError("Fixture schema must be 1")
    if not isinstance(fixture.get("id"), str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]*", fixture["id"]):
        raise ReplayError("Fixture id must be a safe lowercase filename")
    if not isinstance(fixture.get("category"), str) or not fixture["category"]:
        raise ReplayError("Fixture requires a category")
    integer(fixture.get("ticks"), "ticks", 1, 100_000)
    integer(fixture.get("seed"), "seed", 0, 0xFFFFFFFF)
    integer(fixture.get("entrance"), "entrance", 0, 0x610)
    integer(fixture.get("age", 1), "age", 0, 1)
    integer(fixture.get("setup_ticks", 60), "setup_ticks", 1, 10_000)
    integer(fixture.get("rate_hz", 20), "canonical rate_hz", 20, 20)
    integer(fixture.get("presentation_fps", 20), "presentation_fps", 20, 360)
    if "initial_player" in fixture:
        player = fixture["initial_player"]
        if not isinstance(player, dict):
            raise ReplayError("initial_player must be an object")
        integer(player.get("yaw", 0), "initial_player.yaw", -32768, 32767)
        if "pos" in player:
            pos = player["pos"]
            if not isinstance(pos, list) or len(pos) != 3 or any(
                isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value)
                or abs(value) > 32767 for value in pos):
                raise ReplayError("initial_player.pos requires three finite coordinates in [-32767, 32767]")
    if "hud_timer_seconds" in fixture:
        integer(fixture["hud_timer_seconds"], "hud_timer_seconds", 1, 3599)
    if "ocarina_memory_round" in fixture:
        integer(fixture["ocarina_memory_round"], "ocarina_memory_round", 0, 2)
    if "message_text_id" in fixture:
        integer(fixture["message_text_id"], "message_text_id", 0, 0xFFFF)
    if "spawn_ice_keese" in fixture and type(fixture["spawn_ice_keese"]) is not bool:
        raise ReplayError("spawn_ice_keese must be a boolean")
    if "observe_draw_state" in fixture and type(fixture["observe_draw_state"]) is not bool:
        raise ReplayError("observe_draw_state must be a boolean")
    timeline = fixture.get("input")
    if not isinstance(timeline, list) or not timeline:
        raise ReplayError("Fixture requires at least an initial input state")
    previous = None
    seen_sequences: set[int] = set()
    for index, event in enumerate(timeline):
        if not isinstance(event, dict):
            raise ReplayError(f"input[{index}] must be an object")
        numerator = integer(event.get("time_num"), "time_num", 0, 10**9)
        denominator = integer(event.get("time_den"), "time_den", 1, 10**9)
        sequence = integer(event.get("sequence"), "sequence", 0, 0xFFFFFFFF)
        stamp = Fraction(numerator, denominator)
        key = stamp, sequence
        if previous is not None and key <= previous:
            raise ReplayError("Input must be strictly ordered by (timestamp, sequence)")
        if sequence in seen_sequences:
            raise ReplayError("Input sequence identities must be unique")
        if previous is not None and sequence <= previous[1]:
            raise ReplayError("Input sequence identities must increase globally")
        if stamp >= Fraction(fixture["ticks"], 20):
            raise ReplayError("Input at/after the final endpoint cannot affect this run")
        integer(event.get("buttons"), "buttons", 0, 0xFFFF)
        for axis in ("stick_x", "stick_y", "right_stick_x", "right_stick_y"):
            integer(event.get(axis, 0), axis, -128, 127)
        if type(event.get("connected", True)) is not bool:
            raise ReplayError("connected must be boolean")
        if not event.get("connected", True) and any(event.get(key, 0) != 0 for key in
                ("buttons", "stick_x", "stick_y", "right_stick_x", "right_stick_y")):
            raise ReplayError("Disconnected input must supply neutral buttons and axes")
        if any(key in event for key in ("gyro_x", "gyro_y", "gyro_x_bits", "gyro_y_bits")):
            raise ReplayError("Gyro input is not admitted by fixture schema 1")
        if event.get("port", 0) != 0:
            raise ReplayError("Initial harness admits controller port 0 only")
        seen_sequences.add(sequence)
        previous = key
    if Fraction(timeline[0]["time_num"], timeline[0]["time_den"]) != 0:
        raise ReplayError("First input state must be timestamp zero")
    assertions = fixture.get("assertions", [])
    if not isinstance(assertions, list):
        raise ReplayError("assertions must be an array")
    fields = {"changes": "minimum_distinct", "distance": "minimum", "range": "minimum_span",
              "decreases": "minimum_drop", "at_least": "minimum", "counter_advance": "amount",
              "ever_bits": "mask", "ever_nonzero": None, "fall_landing": "minimum_drop", "input": None}
    for assertion in assertions:
        if not isinstance(assertion, dict) or assertion.get("kind") not in fields:
            raise ReplayError("Unknown/missing assertion kind")
        kind = assertion["kind"]
        if kind not in ("fall_landing", "input") and not isinstance(assertion.get("field"), str):
            raise ReplayError(f"{kind} assertion requires a field path")
        parameter = fields[kind]
        if parameter is not None:
            value = assertion.get(parameter, 2 if kind == "changes" else None)
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise ReplayError(f"{kind} assertion requires finite {parameter}")
            if kind in ("changes", "counter_advance", "ever_bits") and type(value) is not int:
                raise ReplayError(f"{kind} assertion requires integer {parameter}")
        if kind == "input":
            integer(assertion.get("tick"), "input assertion tick", 0, fixture["ticks"])
            if not assertion.keys() - {"kind", "tick"}:
                raise ReplayError("Input assertion requires at least one expected field")
    validate_values(fixture)
    return fixture


def first_differences(expected: Any, actual: Any, limit: int = 20, path: str = "$") -> list[dict[str, Any]]:
    """Type-sensitive, exact comparison; array order and negative zero matter."""
    differences: list[dict[str, Any]] = []

    def walk(a: Any, b: Any, field: str) -> None:
        if len(differences) >= limit:
            return
        if type(a) is not type(b):
            differences.append({"field": field, "expected": a, "actual": b, "kind": "type"})
        elif isinstance(a, dict):
            for key in sorted(a.keys() | b.keys()):
                if key not in a or key not in b:
                    differences.append({"field": f"{field}.{key}", "kind": "missing-field",
                                        "expected_present": key in a, "actual_present": key in b})
                    if len(differences) >= limit:
                        break
                else:
                    walk(a[key], b[key], f"{field}.{key}")
        elif isinstance(a, list):
            if len(a) != len(b):
                differences.append({"field": field + ".length", "expected": len(a), "actual": len(b), "kind": "length"})
            for index, (left, right) in enumerate(zip(a, b)):
                walk(left, right, f"{field}[{index}]")
        elif canonical_bytes(a) != canonical_bytes(b):
            differences.append({"field": field, "expected": a, "actual": b, "kind": "value"})

    walk(expected, actual, path)
    return differences[:limit]


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            raise ReplayError(f"{path}:{number}: empty/truncated record")
        row = strict_json(line, f"{path}:{number}")
        if not isinstance(row, dict):
            raise ReplayError(f"{path}:{number}: expected object")
        validate_values(row)
        rows.append(row)
    return rows


def load_run(directory: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    manifest_path = directory / "result.json"
    if not manifest_path.is_file():
        raise ReplayError(f"Incomplete run: missing completion manifest {manifest_path}")
    manifest = read_json(manifest_path)
    if type(manifest.get("schema")) is not int or manifest["schema"] != SCHEMA or manifest.get("status") != "pass" \
            or type(manifest.get("rate_hz")) is not int or manifest["rate_hz"] != 20:
        raise ReplayError(f"Run did not complete canonical replay successfully: {manifest_path}")
    ticks = integer(manifest.get("ticks_completed"), "ticks_completed", 1, 100_000)
    if not isinstance(manifest.get("fixture_id"), str):
        raise ReplayError("Completion manifest requires fixture_id")
    snapshots = read_jsonl(directory / "snapshots.jsonl")
    if len(snapshots) != ticks + 1:
        raise ReplayError(f"Expected initial state plus {ticks} snapshots, found {len(snapshots)}")
    for tick, snapshot in enumerate(snapshots):
        if snapshot.get("schema") != SCHEMA or type(snapshot.get("tick")) is not int or snapshot["tick"] != tick:
            raise ReplayError(f"Invalid/noncontiguous snapshot tick at record {tick}")
        if type(snapshot.get("time_q")) is not int or snapshot["time_q"] != tick * 6:
            raise ReplayError(f"Invalid 120-unit clock at tick {tick}")
        if len(snapshot.keys() - {"schema", "tick", "time_q"}) == 0:
            raise ReplayError(f"Empty semantic snapshot at tick {tick}")
        player = snapshot.get("player")
        if isinstance(player, dict) and player.get("action") == "unmapped":
            raise ReplayError(f"Unmapped Player action at tick {tick}; semantic coverage is incomplete")
        identities = set()
        for actor in snapshot.get("actors", []):
            identity = actor.get("identity")
            if not isinstance(identity, str) or identity == "untracked":
                raise ReplayError(f"Untracked actor identity at tick {tick}; semantic coverage is incomplete")
            if identity in identities:
                raise ReplayError(f"Duplicate actor identity {identity!r} in actor list at tick {tick}")
            identities.add(identity)
    return manifest, snapshots


def load_trace(directory: Path, ticks: int) -> list[dict[str, Any]]:
    rows = read_jsonl(directory / "trace.jsonl")
    if not rows:
        raise ReplayError("Requested verbose trace is empty")
    for index, row in enumerate(rows):
        if row.get("schema") != SCHEMA or type(row.get("sequence")) is not int or row["sequence"] != index:
            raise ReplayError(f"Trace sequence is incomplete/noncontiguous at record {index}")
        tick = integer(row.get("tick"), "trace tick", 0, ticks)
        if type(row.get("time_q")) is not int or row["time_q"] != tick * 6:
            raise ReplayError(f"Trace clock mismatch at record {index}")
        if not isinstance(row.get("phase"), str) or not row["phase"]:
            raise ReplayError(f"Trace phase missing at record {index}")
    return rows


def compare_runs(reference: Path, candidate: Path, include_trace: bool = False,
                 allow_presentation_difference: bool = False) -> dict[str, Any]:
    left_manifest, expected = load_run(reference)
    right_manifest, actual = load_run(candidate)
    report: dict[str, Any] = {"schema": SCHEMA, "status": "pass", "reference": str(reference),
                              "candidate": str(candidate), "snapshots_compared": 0}
    if include_trace and allow_presentation_difference:
        raise ReplayError("Presentation-variant comparison uses exact semantic snapshots; presentation traces intentionally differ")
    for field in ("fixture_id", "rate_hz", "ticks_completed"):
        if left_manifest[field] != right_manifest[field]:
            raise ReplayError(f"Incomparable manifests: {field}")
    for field in ("configuration", "fixture"):
        if field in left_manifest or field in right_manifest:
            left_input, right_input = copy.deepcopy(left_manifest.get(field)), copy.deepcopy(right_manifest.get(field))
            if allow_presentation_difference and isinstance(left_input, dict) and isinstance(right_input, dict):
                presentation_key = "interpolation_fps" if field == "configuration" else "presentation_fps"
                left_fps, right_fps = left_input.pop(presentation_key, None), right_input.pop(presentation_key, None)
                report.setdefault("allowed_presentation_configuration", {})[field + "." + presentation_key] = {
                    "reference": left_fps, "candidate": right_fps}
            if canonical_bytes(left_input) != canonical_bytes(right_input):
                raise ReplayError(f"Incomparable run inputs: manifest {field} differs")
    for left, right in zip(expected, actual):
        left_hash, right_hash = digest(left), digest(right)
        report["snapshots_compared"] += 1
        if left_hash != right_hash:
            report.update(status="mismatch", tick=left["tick"], time_q=left["time_q"],
                          expected_hash=left_hash, actual_hash=right_hash,
                          differences=first_differences(left, right))
            return report
    report["final_hash"] = digest(expected[-1])
    if include_trace:
        left_trace = load_trace(reference, left_manifest["ticks_completed"])
        right_trace = load_trace(candidate, right_manifest["ticks_completed"])
        if digest(left_trace) != digest(right_trace):
            report.update(status="mismatch", domain="trace", differences=first_differences(left_trace, right_trace))
            for left, right in zip(left_trace, right_trace):
                if digest(left) != digest(right):
                    report.update(tick=left.get("tick"), time_q=left.get("time_q"), phase=left.get("phase"))
                    break
        else:
            report["trace_records_compared"] = len(left_trace)
            report["trace_hash"] = digest(left_trace)
    return report


def local_output(path: Path, create: bool = True) -> Path:
    path = path.resolve()
    if not path.is_relative_to(ROOT) or path == ROOT:
        raise ReplayError("Generated outputs must remain in this repository workspace")
    ignored = subprocess.run(["git", "check-ignore", "--quiet", "--", str(path / "ignore-probe")], cwd=ROOT)
    if ignored.returncode:
        raise ReplayError("Output directory must be gitignored; use build/native-simulation-runs/...")
    if create:
        path.mkdir(parents=True, exist_ok=False)
    return path


def git_capture(*args: str) -> str:
    result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise ReplayError(f"git {' '.join(args)} failed: {result.stderr}")
    return result.stdout.strip()


def provenance(executable: Path, assets: dict[str, Path]) -> dict[str, Any]:
    return {"source_head": git_capture("rev-parse", "HEAD"),
            "branch": git_capture("branch", "--show-current"),
            "submodules": git_capture("submodule", "status", "--recursive").splitlines(),
            "tracked_diff_sha256": hashlib.sha256(git_capture("diff", "--binary", "HEAD").encode()).hexdigest(),
            "status": git_capture("status", "--porcelain").splitlines(),
            "untracked_files": {name: file_digest(ROOT / name) for name in
                                git_capture("ls-files", "--others", "--exclude-standard").splitlines()
                                if (ROOT / name).is_file()},
            "executable": {"path": str(executable), "bytes": executable.stat().st_size, "sha256": file_digest(executable)},
            "assets": {name: {"path": str(path), "bytes": path.stat().st_size, "sha256": file_digest(path)}
                       for name, path in assets.items()}, "python": sys.version}


def hashes_for_run(directory: Path) -> dict[str, Any]:
    manifest, rows = load_run(directory)
    hashes = [{"tick": row["tick"], "time_q": row["time_q"],
               "common_100ms_checkpoint": row["time_q"] % 12 == 0, "sha256": digest(row),
               "domains": {key: digest(value) for key, value in sorted(row.items())
                           if key not in ("schema", "tick", "time_q")}} for row in rows]
    summary = {"schema": SCHEMA, "fixture_id": manifest["fixture_id"], "snapshots": hashes,
               "sequence_sha256": digest(hashes)}
    write_json(directory / "hashes.json", summary)
    return summary


def validate_requested_fixture(manifest: dict[str, Any], fixture: dict[str, Any]) -> None:
    """Bind successful engine completion to all requested inputs, not just a label."""
    if manifest.get("fixture_id") != fixture["id"] or manifest.get("ticks_completed") != fixture["ticks"]:
        raise ReplayError("Engine completion does not match requested fixture identity/tick limit")
    if "fixture" not in manifest or digest(manifest["fixture"]) != digest(fixture):
        differences = first_differences(fixture, manifest.get("fixture"), limit=3)
        raise ReplayError(f"Engine completion fixture content differs from requested input: {differences}")
    if manifest.get("setup_ticks") != fixture.get("setup_ticks", 60):
        raise ReplayError("Engine completion setup tick count differs from requested fixture")
    configuration = manifest.get("configuration")
    expected = {"interpolation_fps": fixture.get("presentation_fps", 20), "match_refresh_rate": 0,
                "mouse": 0, "time_sync": 0}
    if not isinstance(configuration, dict) or any(
        key not in configuration or type(configuration[key]) is not int or configuration[key] != value
        for key, value in expected.items()):
        raise ReplayError("Engine completion does not attest the expected pinned configuration")


def field_value(snapshot: dict[str, Any], path: str) -> Any:
    value: Any = snapshot
    try:
        for component in path.split("."):
            value = value[int(component)] if isinstance(value, list) else value[component]
    except (KeyError, IndexError, TypeError, ValueError) as error:
        raise ReplayError(f"Assertion field is absent or invalid: {path}") from error
    return value


def fixture_assertions(fixture: dict[str, Any], snapshots: list[dict[str, Any]]) -> dict[str, Any]:
    """Prove the corpus exercised behavior, independently of repeat equality."""
    results = []
    for assertion in fixture.get("assertions", []):
        kind = assertion["kind"]
        field = assertion.get("field")
        values = [field_value(row, field) for row in snapshots] if field else []
        observed: Any
        if kind == "changes":
            observed = len({canonical_bytes(value) for value in values})
            passed = observed >= assertion.get("minimum_distinct", 2)
        elif kind == "distance":
            points = [[value[axis]["value"] for axis in ("x", "y", "z")] for value in values]
            observed = max(math.dist(points[0], point) for point in points)
            passed = observed >= assertion["minimum"]
        elif kind == "range":
            observed = max(values) - min(values)
            passed = observed >= assertion["minimum_span"]
        elif kind == "decreases":
            observed = values[0] - min(values)
            passed = observed >= assertion["minimum_drop"]
        elif kind == "at_least":
            observed = max(values)
            passed = observed >= assertion["minimum"]
        elif kind == "counter_advance":
            observed = values[-1] - values[0]
            passed = observed == assertion["amount"]
        elif kind == "ever_bits":
            observed = [row["tick"] for row, value in zip(snapshots[1:], values[1:])
                        if value & assertion["mask"] == assertion["mask"]]
            passed = bool(observed)
        elif kind == "ever_nonzero":
            observed = [row["tick"] for row, value in zip(snapshots[1:], values[1:]) if value != 0]
            passed = bool(observed)
        elif kind == "input":
            tick = integer(assertion["tick"], "assertion tick", 0, fixture["ticks"])
            expected = {key: value for key, value in assertion.items() if key not in ("kind", "tick")}
            observed = {key: snapshots[tick]["input"][key] for key in expected}
            passed = not first_differences(expected, observed)
        elif kind == "fall_landing":
            start_y = snapshots[0]["player"]["position"]["y"]["value"]
            falling = [row["tick"] for row in snapshots[1:]
                       if row["player"]["velocity"]["y"]["value"] <= -1.0 and not row["player"]["bg_flags"] & 1]
            landing = [row["tick"] for row in snapshots[1:]
                       if falling and row["tick"] > falling[0] and row["player"]["bg_flags"] & 1
                       and start_y - row["player"]["position"]["y"]["value"] >= assertion["minimum_drop"]]
            observed = {"falling_ticks": falling, "landing_ticks": landing,
                        "maximum_drop": max(start_y - row["player"]["position"]["y"]["value"] for row in snapshots)}
            passed = bool(falling and landing)
        else:
            raise ReplayError(f"Unknown fixture assertion kind: {kind}")
        results.append({"assertion": assertion, "status": "pass" if passed else "fail", "observed": observed})
    return {"schema": SCHEMA, "status": "pass" if all(item["status"] == "pass" for item in results) else "fail",
            "fixture_id": fixture["id"], "assertions": results}


def launch(executable: Path, fixture_path: Path, directory: Path, assets: dict[str, Path],
           timeout: float, trace: bool, verify_presentation_purity: bool = False) -> dict[str, Any]:
    work, output = directory / "work", directory / "output"
    work.mkdir(parents=True)
    output.mkdir()
    temporary = directory / "tmp"
    temporary.mkdir()
    links = {}
    for name, source in assets.items():
        try:
            os.link(source, work / name)
            links[name] = "hardlink"
        except OSError:
            shutil.copyfile(source, work / name)
            links[name] = "copy"
    command = [str(executable), "--native-sim-test", str(fixture_path), "--output", str(output)]
    if trace:
        command.append("--trace")
    if verify_presentation_purity:
        command.append("--verify-presentation-purity")
    env = os.environ.copy()
    env.update(TEMP=str(temporary), TMP=str(temporary))
    options: dict[str, Any] = {}
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options["startupinfo"] = startup
    receipt: dict[str, Any] = {"command": command, "cwd": str(work), "asset_staging": links,
                               "fixture_sha256": file_digest(fixture_path), "status": "started"}
    write_json(directory / "invocation.json", receipt)
    started = time.monotonic()
    with (directory / "process.log").open("w", encoding="utf-8") as log:
        try:
            completed = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT,
                                       timeout=timeout, check=False, **options)
            receipt.update(exit_code=completed.returncode, status="exited")
        except subprocess.TimeoutExpired:
            # subprocess.run kills and waits only for the child it created.
            receipt.update(status="timeout", timeout_seconds=timeout)
    receipt["host_seconds"] = time.monotonic() - started
    if receipt.get("exit_code") != 0:
        receipt["log_tail"] = "\n".join((directory / "process.log").read_text(
            encoding="utf-8", errors="replace").splitlines()[-12:])[-2400:]
        failure_manifest = output / "result.json"
        if failure_manifest.exists():
            try:
                receipt["engine_result"] = read_json(failure_manifest)
            except ReplayError:
                pass
    write_json(directory / "invocation.json", receipt)
    if receipt.get("exit_code") != 0:
        detail = receipt.get("engine_result", {}).get("error", receipt["log_tail"])
        raise ReplayError(f"Engine run failed ({receipt['status']}, exit {receipt.get('exit_code')}): {directory}\n{detail}")
    manifest, snapshots = load_run(output)
    fixture = read_json(fixture_path)
    if file_digest(fixture_path) != receipt["fixture_sha256"]:
        raise ReplayError(f"Fixture input changed during engine execution: {directory}")
    validate_requested_fixture(manifest, fixture)
    if verify_presentation_purity:
        purity = read_json(output / "purity.json")
        if (purity.get("status") != "pass" or purity.get("extra_calls") != 2 or
                purity.get("fixture") != fixture or purity.get("negative_control") is not False):
            raise ReplayError("Missing or invalid direct presentation purity evidence")
        purity["identity"] = {"executable_sha256": file_digest(executable),
                              "fixture_sha256": receipt["fixture_sha256"],
                              "source_head": git_capture("rev-parse", "HEAD"),
                              "source_diff_sha256": hashlib.sha256(
                                  git_capture("diff", "--binary", "HEAD").encode()).hexdigest()}
        write_json(output / "purity.json", purity)
    hashes = hashes_for_run(output)
    coverage = fixture_assertions(fixture, snapshots)
    write_json(output / "assertions.json", coverage)
    if coverage["status"] != "pass":
        raise CoverageError(f"Fixture behavior assertion failed: {output / 'assertions.json'}")
    receipt.update(status="pass", snapshot_count=len(hashes["snapshots"]), sequence_sha256=hashes["sequence_sha256"])
    write_json(directory / "invocation.json", receipt)
    return receipt


def run_corpus(args: argparse.Namespace) -> int:
    if args.repeats < 3:
        raise ReplayError("Repeatability acceptance requires at least three consecutive fresh processes")
    if args.timeout <= 0:
        raise ReplayError("Host timeout must be positive")
    fixture_paths = [path.resolve(strict=True) for path in (args.fixture or sorted(FIXTURES.glob("*.json")))]
    if not fixture_paths:
        raise ReplayError("No fixtures selected")
    fixture_data = [validate_fixture(read_json(path)) for path in fixture_paths]
    if len({fixture["id"] for fixture in fixture_data}) != len(fixture_data):
        raise ReplayError("Duplicate fixture ids")
    executable = args.exe.resolve(strict=True)
    reference_executable = args.reference_exe.resolve(strict=True) if args.reference_exe else None
    asset_directory = args.assets.resolve(strict=True)
    assets = {name: (asset_directory / name).resolve(strict=True) for name in ("oot.o2r", "soh.o2r")}
    controller_db = asset_directory / "gamecontrollerdb.txt"
    if controller_db.is_file():
        assets["gamecontrollerdb.txt"] = controller_db.resolve()
    output_root = args.output or ROOT / "build" / "native-simulation-runs" / (
        datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8])
    output_root = local_output(output_root)
    receipt = {"schema": SCHEMA, "status": "started", "rate_hz": 20, "repeats": args.repeats,
               "verify_presentation_purity": args.verify_presentation_purity,
               "output": str(output_root), "provenance": provenance(executable, assets), "fixtures": []}
    if reference_executable:
        receipt["reference_executable"] = {"path": str(reference_executable),
                                           "sha256": file_digest(reference_executable),
                                           "bytes": reference_executable.stat().st_size}
    if args.reference:
        reference_receipt = read_json(args.reference / "corpus_result.json")
        if reference_receipt.get("status") != "pass":
            raise ReplayError("Reference corpus must already have a passing completion receipt")
        for name, item in receipt["provenance"]["assets"].items():
            if reference_receipt["provenance"]["assets"].get(name, {}).get("sha256") != item["sha256"]:
                raise ReplayError(f"Reference corpus used different/missing asset: {name}")
    write_json(output_root / "corpus_result.json", receipt)
    exit_code = PASS
    for fixture_path, fixture in zip(fixture_paths, fixture_data):
        fixture_root = output_root / fixture["id"]
        fixture_root.mkdir()
        fixture_copy = fixture_root / "fixture.json"
        shutil.copyfile(fixture_path, fixture_copy)
        record: dict[str, Any] = {"id": fixture["id"], "category": fixture["category"], "status": "started", "runs": []}
        receipt["fixtures"].append(record)
        try:
            for repetition in range(1, args.repeats + 1):
                directory = fixture_root / f"run-{repetition:03d}"
                print(f"{fixture['id']} {repetition}/{args.repeats}: {directory}", flush=True)
                record["runs"].append(launch(executable, fixture_copy, directory, assets, args.timeout, args.trace,
                                            args.verify_presentation_purity))
                if repetition > 1:
                    comparison = compare_runs(fixture_root / "run-001" / "output", directory / "output", args.trace)
                    write_json(directory / "comparison.json", comparison)
                    if comparison["status"] != "pass":
                        record.update(status="mismatch", first_mismatch=comparison)
                        exit_code = max(exit_code, MISMATCH)
                        break
            else:
                record["status"] = "pass"
            if record["status"] == "pass" and args.reference:
                reference = args.reference.resolve() / fixture["id"] / "run-001" / "output"
                reference_fixture = args.reference.resolve() / fixture["id"] / "fixture.json"
                if digest(read_json(reference_fixture)) != digest(fixture):
                    raise ReplayError("Reference and candidate fixture content differs")
                comparison = compare_runs(reference, fixture_root / "run-001" / "output", args.trace)
                write_json(fixture_root / "reference-comparison.json", comparison)
                if comparison["status"] != "pass":
                    record.update(status="mismatch", first_mismatch=comparison)
                    exit_code = max(exit_code, MISMATCH)
            if record["status"] == "pass" and reference_executable:
                record["reference_runs"] = []
                for repetition in range(1, args.repeats + 1):
                    directory = fixture_root / "reference" / f"run-{repetition:03d}"
                    print(f"{fixture['id']} reference {repetition}/{args.repeats}: {directory}", flush=True)
                    record["reference_runs"].append(launch(reference_executable, fixture_copy, directory,
                                                            assets, args.timeout, args.trace))
                    reference = fixture_root / "reference" / "run-001" / "output"
                    if repetition > 1:
                        comparison = compare_runs(reference, directory / "output", args.trace)
                        write_json(directory / "comparison.json", comparison)
                        if comparison["status"] != "pass":
                            record.update(status="mismatch", first_mismatch=comparison)
                            exit_code = max(exit_code, MISMATCH)
                            break
                if record["status"] == "pass":
                    comparison = compare_runs(reference, fixture_root / "run-001" / "output", args.trace)
                    write_json(fixture_root / "reference-comparison.json", comparison)
                    if comparison["status"] != "pass":
                        record.update(status="mismatch", first_mismatch=comparison)
                        exit_code = max(exit_code, MISMATCH)
        except CoverageError as error:
            record.update(status="fixture-coverage-failure", error=str(error))
            if exit_code != INFRASTRUCTURE:
                exit_code = MISMATCH
        except (OSError, ReplayError, subprocess.SubprocessError) as error:
            record.update(status="infrastructure-error", error=str(error))
            exit_code = INFRASTRUCTURE
        write_json(output_root / "corpus_result.json", receipt)
        if record["status"] != "pass":
            print(f"{fixture['id']}: {record['status']}: {record.get('error', record.get('first_mismatch'))}", flush=True)
        if record["status"] != "pass" and not args.keep_going:
            break
    # Detect replacement of the binary or archive while the sequential corpus ran.
    unchanged = file_digest(executable) == receipt["provenance"]["executable"]["sha256"] and all(
        file_digest(path) == receipt["provenance"]["assets"][name]["sha256"] for name, path in assets.items())
    if not unchanged:
        receipt["error"] = "Executable or asset identity changed during corpus execution"
        exit_code = INFRASTRUCTURE
    if reference_executable and file_digest(reference_executable) != receipt["reference_executable"]["sha256"]:
        receipt["error"] = "Reference executable identity changed during corpus execution"
        exit_code = INFRASTRUCTURE
    receipt.update(status={PASS: "pass", MISMATCH: "mismatch", INFRASTRUCTURE: "infrastructure-error"}[exit_code],
                   fixtures_requested=len(fixture_data), fixtures_completed=sum(item["status"] == "pass" for item in receipt["fixtures"]),
                   categories=sorted({fixture["category"] for fixture in fixture_data}))
    write_json(output_root / "corpus_result.json", receipt)
    print(f"Corpus {receipt['status']}: {output_root / 'corpus_result.json'}", flush=True)
    return exit_code


def perturb_float(value: Any, path: str = "$") -> dict[str, Any] | None:
    """Negative control mutates copied diagnostic output, never game state/source."""
    if isinstance(value, dict):
        if "bits" in value and "value" in value:
            before = copy.deepcopy(value)
            integer_bits = int(value["bits"], 16) ^ 1
            replacement = struct.unpack("!f", integer_bits.to_bytes(4, "big"))[0]
            if math.isfinite(replacement):
                value["bits"] = f"{integer_bits:08x}"
                value["value"] = replacement
                return {"field": path, "before": before, "after": copy.deepcopy(value)}
        for key in sorted(value):
            result = perturb_float(value[key], f"{path}.{key}")
            if result:
                return result
    elif isinstance(value, list):
        for index, item in enumerate(value):
            result = perturb_float(item, f"{path}[{index}]")
            if result:
                return result
    return None


def negative_test(args: argparse.Namespace) -> int:
    source = args.run.resolve(strict=True)
    manifest, snapshots = load_run(source)
    tick = args.tick if args.tick is not None else min(3, manifest["ticks_completed"])
    integer(tick, "perturbation tick", 1, manifest["ticks_completed"])
    output = local_output(args.output)
    write_json(output / "result.json", manifest)
    changed = copy.deepcopy(snapshots)
    mutation = perturb_float(changed[tick])
    if mutation is None:
        raise ReplayError("Negative control needs at least one float-bit field")
    write_jsonl(output / "snapshots.jsonl", changed)
    command = [sys.executable, "-B", str(Path(__file__).resolve()), "compare", str(source), str(output)]
    failed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
    fail_report = strict_json(failed.stdout, "negative compare report")
    # Restore the exact original diagnostic rows and exercise the same CLI again.
    write_jsonl(output / "snapshots.jsonl", snapshots)
    restored = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
    restore_report = strict_json(restored.stdout, "restored compare report")
    passed = failed.returncode == MISMATCH and fail_report.get("tick") == tick and bool(fail_report.get("differences")) \
        and restored.returncode == PASS and restore_report.get("status") == "pass"
    receipt = {"schema": SCHEMA, "status": "pass" if passed else "fail", "tick": tick, "time_q": tick * 6,
               "mutation": mutation, "negative_exit_code": failed.returncode, "negative_comparison": fail_report,
               "restored_exit_code": restored.returncode, "restored_comparison": restore_report,
               "note": "Only a copy of diagnostic output was perturbed; original engine output is unchanged."}
    write_json(output / "negative-test.json", receipt)
    print(json.dumps(receipt, indent=2))
    return PASS if passed else INFRASTRUCTURE


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run", help="Launch >=3 fresh processes per fixture")
    run.add_argument("--exe", type=Path, default=ROOT / "x64" / "Release" / "soh.exe")
    run.add_argument("--assets", type=Path, default=ROOT / "build" / "x64" / "soh")
    run.add_argument("--fixture", type=Path, action="append")
    run.add_argument("--output", type=Path)
    run.add_argument("--repeats", type=int, default=3)
    run.add_argument("--timeout", type=float, default=120)
    run.add_argument("--trace", action="store_true")
    run.add_argument("--verify-presentation-purity", action="store_true")
    reference_options = run.add_mutually_exclusive_group()
    reference_options.add_argument("--reference", type=Path, help="Prior passing corpus root; read-only comparison")
    reference_options.add_argument("--reference-exe", type=Path, help="Reference executable with the same deterministic seams; repeat it three times too")
    run.add_argument("--keep-going", action="store_true")
    compare = commands.add_parser("compare", help="Strictly compare two completed engine output directories")
    compare.add_argument("reference", type=Path)
    compare.add_argument("candidate", type=Path)
    compare.add_argument("--trace", action="store_true")
    compare.add_argument("--allow-presentation-difference", action="store_true",
                         help="Allow only fixture.presentation_fps and configuration.interpolation_fps to differ; snapshots stay exact")
    compare.add_argument("--output", type=Path)
    validate = commands.add_parser("validate", help="Validate completion, snapshots and emit hashes")
    validate.add_argument("run", type=Path)
    negative = commands.add_parser("negative-test", help="Prove mismatch exit/tick/field, then restore and prove pass")
    negative.add_argument("run", type=Path)
    negative.add_argument("--output", type=Path, required=True)
    negative.add_argument("--tick", type=int)
    args = parser.parse_args(argv)
    if args.command == "run":
        return run_corpus(args)
    if args.command == "compare":
        report = compare_runs(args.reference, args.candidate, args.trace, args.allow_presentation_difference)
        if args.output:
            local_output(args.output.parent, create=False)
            write_json(args.output, report)
        print(json.dumps(report, indent=2))
        return PASS if report["status"] == "pass" else MISMATCH
    if args.command == "validate":
        local_output(args.run, create=False)
        summary = hashes_for_run(args.run)
        print(json.dumps({"status": "pass", "fixture_id": summary["fixture_id"],
                          "snapshots": len(summary["snapshots"]), "sequence_sha256": summary["sequence_sha256"]}, indent=2))
        return PASS
    return negative_test(args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(json.dumps({"schema": SCHEMA, "status": "infrastructure-error", "error": str(error)}, indent=2))
        raise SystemExit(INFRASTRUCTURE)
