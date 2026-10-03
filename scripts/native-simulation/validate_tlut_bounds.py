"""Compile/run synthetic native checks of the production TLUT copy helper on Windows.

Uses the baseline's Visual Studio BuildTools installation and project-local output.
Does not launch the game, load assets, install tools or deliberately crash a process.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

from run_corpus import ROOT, ReplayError, file_digest, local_output, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        raise ReplayError("This native gate requires the Windows baseline toolchain")
    output = local_output(args.output)
    setup = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / (
        "Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat")
    if not setup.is_file():
        raise ReplayError(f"Baseline Visual Studio environment script not found: {setup}")
    source = ROOT / "scripts/native-simulation/native/tlut_bounds.cpp"
    header = ROOT / "libultraship/include/fast/TlutCopy.h"
    executable = output / "tlut_bounds.exe"
    environment = os.environ.copy()
    for name in ("TEMP", "TMP"):
        environment[name] = str(output / "tmp")
    (output / "tmp").mkdir()
    # Values are passed as environment variables and always quoted in batch.
    environment.update(TLUT_SETUP=str(setup), TLUT_SOURCE=str(source),
                       TLUT_INCLUDE=str(ROOT / "libultraship/include"),
                       TLUT_EXE=str(executable), TLUT_OBJECT=str(output / "tlut_bounds.obj"))
    wrapper = output / "run.cmd"
    wrapper.write_text('@echo off\ncall "%TLUT_SETUP%" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                       'cl.exe /std:c++20 /EHsc /W4 /WX /O2 /I"%TLUT_INCLUDE%" '
                       '"%TLUT_SOURCE%" /Fe:"%TLUT_EXE%" /Fo:"%TLUT_OBJECT%"\n'
                       'if errorlevel 1 exit /b %errorlevel%\n"%TLUT_EXE%"\nexit /b %errorlevel%\n',
                       encoding="utf-8")
    receipt = {"schema": 1, "status": "started", "source_sha256": file_digest(source),
               "production_header_sha256": file_digest(header), "toolchain_setup": str(setup),
               "scope": "Synthetic native resource extent/copy/cache-token checks; no renderer or GPU acceptance."}
    destination = output / "tlut-bounds-result.json"
    write_json(destination, receipt)
    try:
        with (output / "process.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(["cmd.exe", "/d", "/c", str(wrapper)], cwd=output, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=120, check=False)
    except (OSError, subprocess.SubprocessError) as error:
        receipt.update(status="infrastructure-error", error=str(error))
        write_json(destination, receipt)
        print(f"Native TLUT infrastructure error: {error}")
        return 2
    text = (output / "process.log").read_text(encoding="utf-8", errors="replace")
    unchanged = file_digest(source) == receipt["source_sha256"] and file_digest(header) == receipt["production_header_sha256"]
    passed = result.returncode == 0 and "TLUT bounds: PASS; 31 checks" in text and unchanged
    receipt.update(status="pass" if passed else "fail", exit_code=result.returncode,
                   process_log_sha256=file_digest(output / "process.log"), checks=31, inputs_unchanged=unchanged,
                   executable_sha256=file_digest(executable) if executable.is_file() else None)
    write_json(destination, receipt)
    print(text, end="")
    print(f"Native TLUT gate {receipt['status']}: {destination}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(f"TLUT test infrastructure error: {error}", file=sys.stderr)
        sys.exit(2)
