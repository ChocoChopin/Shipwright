"""Compile/run native Player temporal and scheduler contracts with local artifacts."""
from __future__ import annotations

import argparse
import os
import re
from pathlib import Path
import subprocess
import sys

from run_corpus import ROOT, ReplayError, file_digest, local_output, write_json


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        raise ReplayError("This regression requires the native Windows baseline toolchain")
    output = local_output(args.output)
    setup = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / (
        "Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat")
    if not setup.is_file():
        raise ReplayError(f"Baseline toolchain setup not found: {setup}")
    source = ROOT / "scripts/native-simulation/native/player_temporal.cpp"
    header = ROOT / "soh/soh/PlayerTemporalCore.hpp"
    scheduler_header = ROOT / "soh/soh/PlayerSchedulerCore.hpp"
    motion_header = ROOT / "soh/soh/PlayerMotionCore.hpp"
    camera_header = ROOT / "soh/soh/PlayerCameraCore.h"
    contact_header = ROOT / "soh/soh/PlayerContactCore.hpp"
    include = ROOT / "build/x64/vcpkg/installed/x64-windows-static/include"
    json_header = include / "nlohmann/json.hpp"
    executable = output / "player_temporal.exe"
    temp = output / "tmp"
    temp.mkdir()
    environment = dict(os.environ, TEMP=str(temp), TMP=str(temp),
                       PURITY_SETUP=str(setup), PURITY_SOURCE=str(source),
                       PURITY_INCLUDE=str(ROOT / "soh/soh"), PURITY_JSON_INCLUDE=str(include),
                       PURITY_EXE=str(executable), PURITY_OBJECT=str(output / "player_temporal.obj"))
    wrapper = output / "run.cmd"
    wrapper.write_text('@echo off\ncall "%PURITY_SETUP%" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                       'cl.exe /std:c++20 /EHsc /W4 /WX /O2 /I"%PURITY_INCLUDE%" /I"%PURITY_JSON_INCLUDE%" '
                       '"%PURITY_SOURCE%" /Fe:"%PURITY_EXE%" /Fo:"%PURITY_OBJECT%"\n'
                       'if errorlevel 1 exit /b %errorlevel%\n"%PURITY_EXE%"\nexit /b %errorlevel%\n',
                       encoding="utf-8")
    inputs = {str(path.relative_to(ROOT)): file_digest(path) for path in (source, header, scheduler_header, motion_header, camera_header, contact_header, json_header)}
    receipt = {"schema": 1, "status": "started", "inputs": inputs, "toolchain_setup": str(setup),
               "scope": "Native temporal primitives and lifecycle contracts; standalone, no game or assets."}
    destination = output / "temporal-result.json"
    write_json(destination, receipt)
    with (output / "process.log").open("w", encoding="utf-8") as log:
        try:
            result = subprocess.run(["cmd.exe", "/d", "/c", str(wrapper)], cwd=output, env=environment,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=120, check=False)
        except (OSError, subprocess.SubprocessError) as error:
            receipt.update(status="infrastructure-error", error=str(error))
            write_json(destination, receipt)
            return 2
    text = (output / "process.log").read_text(encoding="utf-8", errors="replace")
    unchanged = all(file_digest(ROOT / path) == digest for path, digest in inputs.items())
    summary = re.search(r"Player temporal: PASS; (\d+) checks", text)
    checks = int(summary.group(1)) if summary else 0
    passed = result.returncode == 0 and checks >= 95 and unchanged
    receipt.update(status="pass" if passed else "fail", checks=checks, exit_code=result.returncode,
                   inputs_unchanged=unchanged, process_log_sha256=file_digest(output / "process.log"),
                   executable_sha256=file_digest(executable) if executable.is_file() else None)
    write_json(destination, receipt)
    print(text, end="")
    print(f"Player temporal regression: {receipt['status']}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(f"Player temporal infrastructure error: {error}", file=sys.stderr)
        sys.exit(2)
