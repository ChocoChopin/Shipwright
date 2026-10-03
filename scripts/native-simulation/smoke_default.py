"""Bounded Windows ordinary-startup smoke, without native replay arguments.

Stages local assets and a muted config in a fresh ignored workspace. Observes
only the child process's game window, requests a graceful close after 12 seconds,
and records hashes/log evidence. This is not interactive gameplay acceptance.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from run_corpus import ROOT, ReplayError, file_digest, local_output, provenance, read_json, write_json


class OwnedWindows:
    """Enumerate/close only known game-window classes belonging to one live child."""

    def __init__(self, process_id: int) -> None:
        self.process_id = process_id
        self.api = ctypes.WinDLL("user32", use_last_error=True)
        self.callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        self.api.EnumWindows.argtypes = [self.callback_type, wintypes.LPARAM]
        self.api.EnumWindows.restype = wintypes.BOOL
        self.api.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        self.api.GetWindowThreadProcessId.restype = wintypes.DWORD
        self.api.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        self.api.GetWindowTextW.restype = ctypes.c_int
        self.api.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        self.api.GetClassNameW.restype = ctypes.c_int
        self.api.IsWindowVisible.argtypes = [wintypes.HWND]
        self.api.IsWindowVisible.restype = wintypes.BOOL
        self.api.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        self.api.PostMessageW.restype = wintypes.BOOL

    def owns(self, window: int) -> bool:
        owner = wintypes.DWORD()
        self.api.GetWindowThreadProcessId(window, ctypes.byref(owner))
        return owner.value == self.process_id

    def snapshot(self) -> list[dict]:
        found: list[dict] = []

        @self.callback_type
        def collect(window: int, unused: int) -> bool:
            if self.owns(window):
                title = ctypes.create_unicode_buffer(1024)
                window_class = ctypes.create_unicode_buffer(256)
                self.api.GetWindowTextW(window, title, len(title))
                self.api.GetClassNameW(window, window_class, len(window_class))
                # These are the D3D and SDL game-window classes in libultraship.
                if window_class.value in ("N64GAME", "SDL_app") and "Ship of Harkinian" in title.value:
                    found.append({"handle": int(window), "title": title.value, "class": window_class.value,
                                  "visible": bool(self.api.IsWindowVisible(window))})
            return True

        if not self.api.EnumWindows(collect, 0):
            raise ctypes.WinError(ctypes.get_last_error())
        return found

    def close(self, window: int) -> bool:
        # Recheck ownership immediately before WM_CLOSE; never target another process.
        return self.owns(window) and bool(self.api.PostMessageW(window, 0x0010, 0, 0))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "x64" / "Release" / "soh.exe")
    parser.add_argument("--assets", type=Path, default=ROOT / "build" / "x64" / "soh")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=12, help="Observation time, 1..30 seconds")
    parser.add_argument("--close-timeout", type=float, default=8, help="Graceful exit wait, 1..20 seconds")
    args = parser.parse_args()
    if os.name != "nt":
        raise ReplayError("This startup smoke uses native Windows process/window observation")
    if not 1 <= args.seconds <= 30 or not 1 <= args.close_timeout <= 20:
        raise ReplayError("Observation and close timeouts must stay within their documented bounds")
    executable = args.exe.resolve(strict=True)
    asset_directory = args.assets.resolve(strict=True)
    assets = {name: (asset_directory / name).resolve(strict=True) for name in ("oot.o2r", "soh.o2r")}
    controller_db = asset_directory / "gamecontrollerdb.txt"
    if controller_db.is_file():
        assets[controller_db.name] = controller_db.resolve()
    directory = local_output(args.output)
    work, temporary = directory / "work", directory / "tmp"
    work.mkdir()
    temporary.mkdir()
    staging = {}
    for name, source in assets.items():
        try:
            os.link(source, work / name)
            staging[name] = "hardlink"
        except OSError:
            shutil.copyfile(source, work / name)
            staging[name] = "copy"
    # ConsoleVariable::LoadFromPath reads this nesting as gSettings.Volume.Master.
    # AudioSettings reads an integer percentage, so this must be 0 rather than 0.0.
    config = {"CVars": {"gSettings": {"Volume": {"Master": 0}}}}
    config_file = work / "shipofharkinian.json"
    write_json(config_file, config)
    write_json(directory / "initial-config.json", config)
    command = [str(executable)]
    receipt = {"schema": 1, "status": "started", "mode": "ordinary-startup-smoke",
               "interactive_gameplay_acceptance": False, "command": command, "cwd": str(work),
               "observation_seconds": args.seconds, "close_timeout_seconds": args.close_timeout,
               "asset_staging": staging, "initial_config_sha256": file_digest(config_file),
               "provenance": provenance(executable, assets), "windows": [], "close_requests": [],
               "forced_termination": False}
    write_json(directory / "smoke.json", receipt)
    environment = os.environ.copy()
    environment.update(TEMP=str(temporary), TMP=str(temporary))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    started = time.monotonic()
    process = None
    try:
        with (directory / "process.log").open("w", encoding="utf-8") as log:
            process = subprocess.Popen(command, cwd=work, env=environment, stdout=log,
                                       stderr=subprocess.STDOUT, startupinfo=startup)
            receipt["process_id"] = process.pid
            windows = OwnedWindows(process.pid)
            observed = set()
            while process.poll() is None and time.monotonic() - started < args.seconds:
                for window in windows.snapshot():
                    identity = (window["handle"], window["title"])
                    if identity not in observed:
                        receipt["windows"].append(dict(window, observed_seconds=time.monotonic() - started))
                        observed.add(identity)
                time.sleep(0.25)
            receipt["alive_at_observation_end"] = process.poll() is None
            if process.poll() is None:
                for window in windows.snapshot():
                    receipt["close_requests"].append(dict(window, posted=windows.close(window["handle"])))
                try:
                    process.wait(timeout=args.close_timeout)
                except subprocess.TimeoutExpired:
                    receipt["forced_termination"] = True
                    process.kill()
                    process.wait(timeout=5)
            receipt["exit_code"] = process.returncode
    except (OSError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
    finally:
        if process is not None and process.poll() is None:
            receipt["forced_termination"] = True
            process.kill()
            process.wait(timeout=5)
            receipt["exit_code"] = process.returncode
    receipt["host_seconds"] = time.monotonic() - started
    logs = [directory / "process.log", *sorted((work / "logs").glob("*.log"))]
    receipt["logs"] = [{"path": str(path.relative_to(directory)), "bytes": path.stat().st_size,
                        "sha256": file_digest(path)} for path in logs if path.is_file()]
    lines = [line for path in logs if path.is_file()
             for line in path.read_text(encoding="utf-8", errors="replace").splitlines()]
    receipt["scene_initialization_lines"] = [line for line in lines if "Scene Init - sceneNum:" in line]
    receipt["startup_lines"] = [line for line in lines if "Starting Ship of Harkinian version" in line]
    try:
        final_config = read_json(config_file)
        receipt["configured_master_volume"] = final_config.get("CVars", {}).get("gSettings", {}).get(
            "Volume", {}).get("Master")
        receipt["final_config_sha256"] = file_digest(config_file)
    except (OSError, ReplayError, AttributeError) as error:
        receipt["configuration_error"] = str(error)
    receipt["executable_unchanged"] = file_digest(executable) == receipt["provenance"]["executable"]["sha256"]
    receipt["assets_unchanged"] = all(
        file_digest(path) == receipt["provenance"]["assets"][name]["sha256"] and
        file_digest(work / name) == receipt["provenance"]["assets"][name]["sha256"]
        for name, path in assets.items())
    volume = receipt.get("configured_master_volume")
    passed = (receipt.get("alive_at_observation_end") and receipt["windows"] and receipt["startup_lines"]
              and receipt["scene_initialization_lines"] and receipt["close_requests"]
              and all(item["posted"] for item in receipt["close_requests"])
              and not receipt["forced_termination"] and receipt.get("exit_code") == 0
              and type(volume) is int and volume == 0
              and receipt["executable_unchanged"] and receipt["assets_unchanged"] and "error" not in receipt)
    receipt["status"] = "pass" if passed else "fail"
    write_json(directory / "smoke.json", receipt)
    print(json.dumps({"status": receipt["status"], "receipt": str(directory / "smoke.json"),
                      "exit_code": receipt.get("exit_code"), "windows": len(receipt["windows"]),
                      "scene_initializations": len(receipt["scene_initialization_lines"]),
                      "interactive_gameplay_acceptance": False}))
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ReplayError, subprocess.SubprocessError) as error:
        print(json.dumps({"status": "infrastructure-error", "error": str(error)}), file=sys.stderr)
        raise SystemExit(2)
