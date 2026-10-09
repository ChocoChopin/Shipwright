"""Audit direct CPU helper evidence independently of display-list replay counts."""
from __future__ import annotations
import argparse
from pathlib import Path
import run_corpus as replay


def validate_purity(result: dict, fixture: dict, executable_hash: str) -> None:
    if result.get("status") != "pass" or result.get("extra_calls") != 2:
        raise replay.ReplayError("Direct helper repetition did not pass exactly two extra calls")
    if result.get("packet_bytes_checked") is not True:
        raise replay.ReplayError("Immutable packet comparison evidence missing")
    if result.get("fixture") != fixture or result.get("negative_control") is not False:
        raise replay.ReplayError("Purity fixture/control identity mismatch")
    if result.get("first_failure"):
        raise replay.ReplayError("Purity receipt contains a failure")
    if result.get("identity", {}).get("executable_sha256") != executable_hash:
        raise replay.ReplayError("Purity executable identity mismatch")
    if fixture.get("message_text_id") == 0x305F and "message" in result.get("coverage", {}):
        raise replay.ReplayError("Quicktext/fade fixture was admitted")
    if fixture.get("observe_player_state"):
        player_hz = fixture.get("player_hz", 20)
        expected_player = fixture["ticks"] * (player_hz // 20)
        if "expected_fallback_edge_q" in fixture:
            q = 120 // player_hz
            due = (fixture["expected_fallback_edge_q"] + q - 1) // q * q
            expected_player = due // q + fixture["ticks"] - (due + 5) // 6
        if result.get("coverage", {}).get("player", {}).get("measured", 0) != expected_player:
            raise replay.ReplayError("Player fixture did not exercise extracted CPU presentation at every measured pose")
    for helper, coverage in result.get("coverage", {}).items():
        if helper not in ("message", "countdown", "player"):
            raise replay.ReplayError("Unknown helper")
        for key in ("setup", "measured", "visible", "invisible", "commands"):
            value = coverage.get(key, 0)
            if type(value) is not int or value < 0:
                raise replay.ReplayError("Invalid helper coverage count")
        if coverage.get("setup", 0) + coverage.get("measured", 0) != coverage.get("visible", 0) + coverage.get("invisible", 0):
            raise replay.ReplayError("Helper coverage is inconsistent")
        minimum = 20 if helper == "player" else (13 if helper == "message" else 7)
        if result.get("admission_negatives", {}).get(helper, 0) < minimum:
            raise replay.ReplayError("Admission negative cases missing")


def audit(corpus: Path) -> dict:
    receipt = replay.read_json(corpus / "corpus_result.json")
    if receipt.get("status") != "pass" or receipt.get("verify_presentation_purity") is not True:
        raise replay.ReplayError("Expected a passing purity-enabled corpus")
    totals = {name: {key: 0 for key in ("setup", "measured", "visible", "invisible", "commands")}
              for name in ("countdown", "message", "player")}
    runs = 0
    for item in receipt["fixtures"]:
        fixture = replay.read_json(corpus / item["id"] / "fixture.json")
        for repetition in range(1, receipt["repeats"] + 1):
            directory = corpus / item["id"] / f"run-{repetition:03}" / "output"
            result = replay.read_json(directory / "purity.json")
            invocation = replay.read_json(directory.parent / "invocation.json")
            if invocation.get("purity_sha256") != replay.file_digest(directory / "purity.json"):
                raise replay.ReplayError("Purity receipt hash mismatch")
            validate_purity(result, fixture, receipt["provenance"]["executable"]["sha256"])
            for helper, counts in result["coverage"].items():
                for key in totals[helper]:
                    totals[helper][key] += counts.get(key, 0)
            runs += 1
    return {"schema": 1, "status": "pass", "runs": runs, "extra_calls": 2, "coverage": totals}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--require-both", action="store_true")
    args = parser.parse_args()
    result = audit(args.corpus)
    if args.require_both and any(result["coverage"][helper]["measured"] == 0 or
                                 result["coverage"][helper]["visible"] == 0 for helper in ("countdown", "message")):
        raise replay.ReplayError("Required helper was not exercised")
    replay.write_json(args.corpus / "purity-analysis.json", result)
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
