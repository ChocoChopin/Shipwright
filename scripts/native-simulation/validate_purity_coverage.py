"""Compile/run the production purity counter regression with repository-local artifacts."""
from __future__ import annotations

import argparse
import os
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
    source = ROOT / "scripts/native-simulation/native/purity_coverage.cpp"
    header = ROOT / "soh/soh/NativeSimulationPresentationCoverage.hpp"
    include = ROOT / "build/x64/vcpkg/installed/x64-windows-static/include"
    json_header = include / "nlohmann/json.hpp"
    executable = output / "purity_coverage.exe"
    temp = output / "tmp"
    temp.mkdir()
    environment = dict(os.environ, TEMP=str(temp), TMP=str(temp),
                       PURITY_SETUP=str(setup), PURITY_SOURCE=str(source),
                       PURITY_INCLUDE=str(ROOT / "soh/soh"), PURITY_JSON_INCLUDE=str(include),
                       PURITY_EXE=str(executable), PURITY_OBJECT=str(output / "purity_coverage.obj"))
    wrapper = output / "run.cmd"
    wrapper.write_text('@echo off\ncall "%PURITY_SETUP%" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
                       'cl.exe /std:c++20 /EHsc /W4 /WX /O2 /I"%PURITY_INCLUDE%" /I"%PURITY_JSON_INCLUDE%" '
                       '"%PURITY_SOURCE%" /Fe:"%PURITY_EXE%" /Fo:"%PURITY_OBJECT%"\n'
                       'if errorlevel 1 exit /b %errorlevel%\n"%PURITY_EXE%"\nexit /b %errorlevel%\n',
                       encoding="utf-8")
    inputs = {str(path.relative_to(ROOT)): file_digest(path) for path in (source, header, json_header)}
    receipt = {"schema": 1, "status": "started", "inputs": inputs, "toolchain_setup": str(setup),
               "scope": "Production first-call and accumulated helper coverage; standalone, no game or assets."}
    destination = output / "purity-coverage-result.json"
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
    passed = result.returncode == 0 and "Purity coverage: PASS; 20 checks" in text and unchanged
    receipt.update(status="pass" if passed else "fail", checks=20, exit_code=result.returncode,
                   inputs_unchanged=unchanged, process_log_sha256=file_digest(output / "process.log"),
                   executable_sha256=file_digest(executable) if executable.is_file() else None)
    write_json(destination, receipt)
    print(text, end="")
    print(f"Purity coverage regression: {receipt['status']}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(f"Purity coverage infrastructure error: {error}", file=sys.stderr)
        sys.exit(2)
