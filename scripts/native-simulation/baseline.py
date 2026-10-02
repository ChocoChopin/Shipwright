"""Native Windows baseline helper; no game changes, global settings, or ROM uploads.

Run with the intended Python interpreter. Generated output and caches stay local
to the workspace (upstream also uses x64/Release and a root soh.o2r copy).
Failures preserve the subprocess return code; classify their log before patching.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "x64"
EVIDENCE = ROOT / "build" / "native-simulation-evidence"


def capture(*args: str, cwd: Path = ROOT) -> str:
    result = subprocess.run(args, cwd=cwd, text=True, encoding="utf-8",
                            errors="replace", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {args}\n{result.stdout}")
    return result.stdout.strip()


def ignored(path: Path) -> None:
    subprocess.run(["git", "check-ignore", "--quiet", "--", str(path)],
                   cwd=ROOT, check=True)


def local_environment() -> dict[str, str]:
    env = os.environ.copy()
    directories = {
        "TEMP": ROOT / ".tmp",
        "TMP": ROOT / ".tmp",
        "VCPKG_DEFAULT_BINARY_CACHE": ROOT / "build" / "vcpkg-cache",
        "VCPKG_DOWNLOADS": ROOT / "build" / "vcpkg-downloads",
    }
    for name, path in directories.items():
        ignored(path / "ignore-probe")
        path.mkdir(parents=True, exist_ok=True)
        env[name] = str(path)
    env["VCPKG_DISABLE_METRICS"] = "1"
    # Do not let upstream's auto-update helper mutate a machine-global vcpkg.
    env["VCPKG_ROOT"] = str(BUILD / "vcpkg")
    return env


def run_logged(phase: str, command: list[str]) -> int:
    env = local_environment()
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    log = EVIDENCE / f"{phase}.log"
    invocation = {
        "phase": phase,
        "command": command,
        "cwd": str(ROOT),
        "source_head": capture("git", "rev-parse", "HEAD"),
        "submodules": capture("git", "submodule", "status", "--recursive").splitlines(),
        "tracked_changes": capture("git", "diff", "--name-only").splitlines(),
        "staged_changes": capture("git", "diff", "--cached", "--name-only").splitlines(),
        "environment": {key: env[key] for key in
                        ("TEMP", "TMP", "VCPKG_DEFAULT_BINARY_CACHE", "VCPKG_DOWNLOADS")},
    }
    receipt = EVIDENCE / f"{phase}-invocation.json"
    # Write a start receipt too, so an interrupted build is not mistaken for success.
    receipt.write_text(json.dumps(invocation, indent=2) + "\n", encoding="utf-8")
    print(f"Running {phase}; full log: {log}", flush=True)
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, check=False)
    invocation["exit_code"] = result.returncode
    receipt.write_text(json.dumps(invocation, indent=2) + "\n", encoding="utf-8")
    print("\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-25:]))
    print(f"{phase}: exit {result.returncode}", flush=True)
    return result.returncode


def verify_rom(source: Path, copy: bool) -> int:
    source = source.resolve(strict=True)
    with source.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha1").hexdigest()
    supported = json.loads((ROOT / "docs" / "supportedHashes.json").read_text())
    matches = [item["name"] for item in supported if item["sha1"].lower() == digest]
    extraction_config = (ROOT / "soh" / "assets" / "yml" / "config.yml").read_text()
    result = {"sha1": digest, "size": source.stat().st_size,
              "supported_names": matches,
              "torch_config_entry": f"{digest}:" in extraction_config}
    if copy:
        if not matches or not result["torch_config_entry"]:
            raise RuntimeError("ROM is not admitted by both supported hashes and extraction config")
        destination = ROOT / "roms" / "native-simulation-input.z64"
        # This helper only copies native big-endian input. It never renames byte-swapped bytes.
        with source.open("rb") as stream:
            if stream.read(4) != bytes.fromhex("80371240"):
                raise RuntimeError("Copy mode requires a native big-endian z64 ROM")
        ignored(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            with destination.open("rb") as stream:
                if hashlib.file_digest(stream, "sha1").hexdigest() != digest:
                    raise RuntimeError(f"Refusing to overwrite a different ROM: {destination}")
        elif destination != source:
            shutil.copyfile(source, destination)
        result["local_copy"] = str(destination.relative_to(ROOT))
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / "rom-verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if matches and result["torch_config_entry"] else 2


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("configure", "generate", "extract", "build", "verify-rom"))
    parser.add_argument("--rom", type=Path)
    parser.add_argument("--copy-rom", action="store_true")
    parser.add_argument("--config", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.phase == "verify-rom":
        if args.rom is None:
            parser.error("verify-rom requires --rom; no directory-wide upload or scan occurs")
        return verify_rom(args.rom, args.copy_rom)
    if os.name != "nt":
        parser.error("This wrapper targets native Windows; see upstream docs for other platforms")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.phase == "configure":
        command = ["cmake", "-S", str(ROOT), "-B", str(BUILD),
                   "-G", "Visual Studio 17 2022", "-T", "v143", "-A", "x64",
                   f"-DPython3_EXECUTABLE:FILEPATH={sys.executable}"]
    else:
        command = ["cmake", "--build", str(BUILD), "--config", args.config,
                   "--parallel", str(args.jobs)]
        if args.phase in ("generate", "extract"):
            command += ["--target", {"generate": "GenerateSohOtr", "extract": "ExtractAssets"}[args.phase]]
    return run_logged(args.phase, command)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Baseline infrastructure error: {error}", file=sys.stderr)
        raise SystemExit(1)
