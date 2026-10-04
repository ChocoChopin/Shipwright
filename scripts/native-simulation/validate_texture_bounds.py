"""Compile/run synthetic native checks of the production texture row copier.

Uses the baseline Windows compiler and project-local outputs, without game assets.
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
    source = ROOT / "scripts/native-simulation/native/texture_bounds.cpp"
    header = ROOT / "libultraship/include/fast/TextureCopy.h"
    executable = output / "texture_bounds.exe"
    environment = os.environ.copy()
    (output / "tmp").mkdir()
    environment.update(TEMP=str(output / "tmp"), TMP=str(output / "tmp"),
                       TEXTURE_SETUP=str(setup), TEXTURE_SOURCE=str(source),
                       TEXTURE_INCLUDE=str(ROOT / "libultraship/include"),
                       TEXTURE_EXE=str(executable), TEXTURE_OBJECT=str(output / "texture_bounds.obj"))
    wrapper = output / "run.cmd"
    wrapper.write_text('@echo off\ncall "%TEXTURE_SETUP%" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                       'cl.exe /std:c++20 /EHsc /W4 /WX /O2 /I"%TEXTURE_INCLUDE%" '
                       '"%TEXTURE_SOURCE%" /Fe:"%TEXTURE_EXE%" /Fo:"%TEXTURE_OBJECT%"\n'
                       'if errorlevel 1 exit /b %errorlevel%\n"%TEXTURE_EXE%"\nexit /b %errorlevel%\n',
                       encoding="utf-8")
    receipt = {"schema": 1, "status": "started", "source_sha256": file_digest(source),
               "production_header_sha256": file_digest(header), "toolchain_setup": str(setup),
               "scope": "Synthetic resource row extent/copy checks; engine validation is a separate gate."}
    destination = output / "texture-bounds-result.json"
    write_json(destination, receipt)
    try:
        with (output / "process.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(["cmd.exe", "/d", "/c", str(wrapper)], cwd=output, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=120, check=False)
    except (OSError, subprocess.SubprocessError) as error:
        receipt.update(status="infrastructure-error", error=str(error))
        write_json(destination, receipt)
        print(f"Native texture infrastructure error: {error}")
        return 2
    text = (output / "process.log").read_text(encoding="utf-8", errors="replace")
    unchanged = file_digest(source) == receipt["source_sha256"] and file_digest(header) == receipt["production_header_sha256"]
    passed = result.returncode == 0 and "Texture bounds: PASS; 24 checks" in text and unchanged
    receipt.update(status="pass" if passed else "fail", exit_code=result.returncode,
                   process_log_sha256=file_digest(output / "process.log"), checks=24, inputs_unchanged=unchanged,
                   executable_sha256=file_digest(executable) if executable.is_file() else None)
    write_json(destination, receipt)
    print(text, end="")
    print(f"Native texture gate {receipt['status']}: {destination}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(f"Texture test infrastructure error: {error}", file=sys.stderr)
        sys.exit(2)
