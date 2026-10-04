"""Require graceful detection of one test-only state write, never a native fault."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import run_corpus as replay


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=replay.ROOT / "x64/Release/soh.exe")
    parser.add_argument("--fixture", type=Path,
                        default=replay.ROOT / "scripts/native-simulation/fixtures/draw-state/hud-zero.json")
    args = parser.parse_args()
    root = replay.local_output(args.output)
    work, output, temp = (root / name for name in ("work", "output", "tmp"))
    for path in (work, output, temp):
        path.mkdir()
    assets = {}
    for name in ("oot.o2r", "soh.o2r", "gamecontrollerdb.txt"):
        source = replay.ROOT / "build/x64/soh" / name
        os.link(source, work / name)
        assets[name] = source
    fixture = args.fixture.resolve(strict=True)
    exe = args.exe.resolve(strict=True)
    command = [str(exe), "--native-sim-test", str(fixture), "--output", str(output),
               "--verify-presentation-purity", "--presentation-purity-negative-control"]
    env = dict(os.environ, TEMP=str(temp), TMP=str(temp))
    options = {}
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options["startupinfo"] = startup
    receipt = {"schema": 1, "status": "started", "command": command,
               "provenance": replay.provenance(exe, assets), "fixture_sha256": replay.file_digest(fixture)}
    replay.write_json(root / "control.json", receipt)
    with (root / "process.log").open("w", encoding="utf-8") as log:
        try:
            process = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT,
                                     timeout=60, check=False, **options)
        except subprocess.TimeoutExpired:
            receipt.update(status="fail", error="timeout")
            replay.write_json(root / "control.json", receipt)
            return 2
    receipt["exit_code"] = process.returncode
    passed = False
    if process.returncode == 2 and (output / "purity.json").exists():
        purity = replay.read_json(output / "purity.json")
        result = replay.read_json(output / "result.json")
        passed = (purity.get("negative_control") is True and purity.get("status") == "fail" and
                  "live state mutated" in purity.get("first_failure", "") and result.get("status") == "fail" and
                  "presentation purity" in result.get("error", ""))
        if replay.read_json(fixture).get("observe_player_state"):
            passed = passed and purity.get("first_failure", "").startswith("player:")
        receipt.update(purity=purity, native_result=result)
    receipt["status"] = "pass" if passed else "fail"
    replay.write_json(root / "control.json", receipt)
    print(f"Purity detector control: {receipt['status']}; exit {process.returncode}")
    return 0 if passed else 2


if __name__ == "__main__":
    raise SystemExit(main())
