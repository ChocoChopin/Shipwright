"""Named, disposable human playtests using the current build and ordinary input."""
import argparse
import copy
import ctypes
import json
import os
from pathlib import Path
import subprocess

import play

CATALOG = Path(__file__).with_name("play_scenarios.json")


def scenarios():
    rows = json.loads(CATALOG.read_text(encoding="utf-8"))
    names = set()
    for row in rows:
        name = row["name"]
        if not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-" for c in name) or name in names:
            raise ValueError(f"Invalid/duplicate scenario name: {name}")
        names.add(name)
        if row["age"] not in (0, 1) or len(row["items"]) != 3:
            raise ValueError(f"Invalid scenario equipment: {name}")
    return rows


def scenario_config(seed, row, rate):
    config = copy.deepcopy(seed)
    cvars = config.setdefault("CVars", {})
    cvars.setdefault("gSettings", {}).update(BootSequence=4, InterpolationFPS=120, MatchRefreshRate=0)
    developer = cvars.setdefault("gDeveloperTools", {})
    developer.update(NativePlayerHz=rate, NativePlaytestAge=row["age"],
                     NativePlaytestEntranceOnly=int("position" not in row),
                     NativeHorseRideStart=2 if row.get("mounted") else 0,
                     NativeStalfosArena=int(row.get("stalfos",False)),
                     NativeStalfosHz=rate if row.get("stalfos") and rate!=20 else 0,
                     DebugSaveFileMode=2, PlayerFrameProfile=0, ResourceLogging=0)
    for index, item in enumerate(row["items"], 1):
        developer[f"NativePlaytestItem{index}"] = item
    x, y, z = row.get("position", [0, 0, 0])
    config["WarpPoints"] = {row["name"]: {
        "entranceId": row["entrance"], "roomNum": row.get("room", 0),
        "pos": {"x": x, "y": y, "z": z}, "rotY": row.get("yaw", 0), "bootToPoint": True,
    }}
    return config


def running(pid):
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    api.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    api.OpenProcess.restype = ctypes.c_void_p
    api.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
    api.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = api.OpenProcess(0x1000, False, pid)
    if not handle:
        if ctypes.get_last_error() == 87:  # PID no longer exists
            return False
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        code = ctypes.c_uint32()
        if not api.GetExitCodeProcess(handle, ctypes.byref(code)):
            raise ctypes.WinError(ctypes.get_last_error())
        return code.value == 259
    finally:
        api.CloseHandle(handle)


def prepare(row, rate, exe_arg=Path("x64/Release/soh.exe")):
    runtime_arg = Path("build/playtests") / row["name"]
    runtime = play.inside(play.ROOT / runtime_arg, play.ROOT / "build")
    launch = runtime / "launch.json"
    if launch.exists():
        pid = json.loads(launch.read_text(encoding="utf-8")).get("pid")
        if pid and running(pid):
            raise ValueError(f"Close this scenario's running game before resetting it (PID {pid})")
    exe, runtime, receipt = play.prepare(runtime_arg, exe_arg)
    # First launch inherits the current ordinary playthrough's preferences when available.
    # Subsequent launches retain this scenario's own graphics/controller preferences.
    seed = play.read_config(runtime / play.CONFIG_NAME)
    normal = play.ROOT / "build/playthrough" / play.CONFIG_NAME
    if receipt["initialized"] and normal.exists():
        seed = play.initial_config(normal)
    config = scenario_config(seed, row, rate)
    (runtime / play.CONFIG_NAME).write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    receipt.update(scenario=row["name"], player_hz=rate, render_fps=120, boot_sequence=4,
                   temporary_debug_save=True, human_gameplay_accepted=False,
                   title=row["title"], actions=row["actions"], expected=row["expected"])
    (runtime / "scenario.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return exe, runtime, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", nargs="?")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--rate", type=int, choices=(20, 120), default=120,
                        help="Compare Player cadence while rendering stays120")
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    try:
        rows = scenarios()
        if args.list or args.scenario is None:
            for i, row in enumerate(rows, 1):
                print(f"{i:2}. {row['name']:<17} {row['title']}")
            if args.list:
                return
            choice = input("\nChoose number or name (Enter to cancel): ").strip()
            if not choice:
                return
            args.scenario = rows[int(choice)-1]["name"] if choice.isdigit() and 1 <= int(choice) <= len(rows) else choice
        row = next((row for row in rows if row["name"] == args.scenario), None)
        if row is None:
            raise ValueError(f"Unknown scenario: {args.scenario}; use --list")
        exe, runtime, receipt = prepare(row, args.rate)
        print(f"\n{row['title']} | Player{args.rate}, rendering120")
        print("Fresh temporary save on every launch; ordinary playthrough saves are separate.")
        for i, action in enumerate(row["actions"], 1):
            print(f"  {i}. {action}")
        print("Watch for: " + row["expected"])
        print(f"Executable: {exe}\nSHA256: {receipt['sha256']}")
        if not args.prepare_only:
            environment = os.environ.copy()
            environment.update(TEMP=str(runtime / ".tmp"), TMP=str(runtime / ".tmp"))
            child = subprocess.Popen([str(exe)], cwd=runtime, env=environment)
            receipt["pid"] = child.pid
            (runtime / "launch.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
            print("Close the game before relaunching this scenario.")
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(1, f"play_scenario.py: {error}\n")


if __name__ == "__main__":
    main()
