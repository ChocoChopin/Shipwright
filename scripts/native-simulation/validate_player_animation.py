"""Run the asset-free production Player animation queue checks in the game executable."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

from run_corpus import ROOT, ReplayError, file_digest, local_output, write_json


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "x64/Release/soh.exe")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = local_output(args.output)
    work, temp = output / "work", output / "tmp"
    work.mkdir()
    temp.mkdir()
    executable = args.exe.resolve(strict=True)
    command = [str(executable), "--native-sim-animation-queue-test"]
    receipt = {"status": "started", "command": command, "executable_sha256": file_digest(executable),
               "scope": "Private engine animation queue only; no asset, game, window, or high-rate integration claim."}
    destination = output / "animation-result.json"
    write_json(destination, receipt)
    options = {}
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options["startupinfo"] = startup
    try:
        with (output / "process.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(command, cwd=work, env=dict(os.environ, TEMP=str(temp), TMP=str(temp)),
                                    stdout=log, stderr=subprocess.STDOUT, timeout=30, check=False, **options)
    except (OSError, subprocess.SubprocessError) as error:
        receipt.update(status="infrastructure-error", error=str(error))
        write_json(destination, receipt)
        return 2
    text = (output / "process.log").read_text(encoding="utf-8", errors="replace")
    summary = re.search(r"Player animation queue: PASS; (\d+) checks; 0 failures", text)
    unchanged = file_digest(executable) == receipt["executable_sha256"]
    no_game_files = not any(work.iterdir())
    passed = result.returncode == 0 and summary is not None and unchanged and no_game_files
    receipt.update(status="pass" if passed else "fail", exit_code=result.returncode,
                   checks=int(summary.group(1)) if summary else 0, executable_unchanged=unchanged,
                   no_game_files=no_game_files, process_log_sha256=file_digest(output / "process.log"))
    write_json(destination, receipt)
    print(text, end="")
    print(f"Player animation queue gate: {receipt['status']}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ReplayError) as error:
        print(f"Player animation infrastructure error: {error}", file=sys.stderr)
        sys.exit(2)
