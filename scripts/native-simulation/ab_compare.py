"""Launch labelled, isolated native Windows vanilla/modded comparison sessions.

Uses existing binaries; never builds, modifies source, or shares writable saves.
Assets are hardlinked on the same volume. Ordinary save copies may be supplied;
cross-build in-memory savestates are deliberately not copied.
"""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("side", choices=("vanilla", "modded"))
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--assets", type=Path, default=ROOT / "build/x64/soh")
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--seed-saves", type=Path, help="Copy an ordinary saves directory into a new runtime")
    parser.add_argument("--field", action="store_true", help="Boot an adult maxed save beside Epona in Hyrule Field")
    parser.add_argument("--existing-modded-pid", type=int, help="Keep this live session; tile it on the left")
    args = parser.parse_args()
    exe = (args.exe or (ROOT / ("build/vanilla-reference/soh.exe" if args.side == "vanilla"
                               else "x64/Release/soh.exe"))).resolve(strict=True)
    runtime = (args.runtime or ROOT / "build/ab-compare" / args.side).resolve()
    runtime.relative_to(ROOT)
    runtime.mkdir(parents=True, exist_ok=True)
    config_path = runtime / "shipofharkinian.json"
    fresh = not config_path.exists()
    config = json.loads((config_path if not fresh else args.assets / "shipofharkinian.json").read_text())
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.SetProcessDPIAware()
    width = user32.GetSystemMetrics(0) // 2
    height = min(user32.GetSystemMetrics(1) - 80, round(width * 9 / 16) + 40)
    x = width if args.side == "vanilla" else 0
    config.setdefault("Window", {}).update(Width=width, Height=height, PositionX=x, PositionY=40)
    config["Window"].setdefault("Fullscreen", {})["Enabled"] = False
    cvars = config.setdefault("CVars", {})
    settings = cvars.setdefault("gSettings", {})
    settings.update(InterpolationFPS=120, MatchRefreshRate=0)
    dev = cvars.setdefault("gDeveloperTools", {})
    dev.update(NativePlayerHz=20 if args.side == "vanilla" else 120, PlayerFrameProfile=0,
               NativeHorseRideStart=0)
    if args.field:
        # Scene-selector slot 0xFF skips InitFileMaxed. Upstream Warp uses
        # temporary slot 0xFE and therefore honors DebugSaveFileMode.
        settings["BootSequence"] = 4
        config["WarpPoints"] = {"Epona comparison": {
            "entranceId": 505, "roomNum": 0,
            "pos": {"x": -1740.0, "y": 72.0, "z": 5497.0},
            "rotY": -16384, "bootToPoint": True}}
        dev.update(BetterDebugWarpScreen=1, DebugSaveFileMode=2)
        cvars.setdefault("gGeneral", {}).update(BetterDebugWarpScreenCurrentScene=0,
            BetterDebugWarpScreenTopDisplayedScene=0, BetterDebugWarpScreenPageDownIndex=7,
            BetterDebugWarpScreenLinkAge=0, BetterDebugWarpScreenNightFlag=0)
    config_path.write_text(json.dumps(config, indent=2))
    for name in ("oot.o2r", "soh.o2r", "gamecontrollerdb.txt"):
        source, target = args.assets / name, runtime / name
        if source.exists() and not target.exists():
            os.link(source, target)
    if args.seed_saves:
        if not fresh:
            parser.error("--seed-saves requires a new runtime; existing saves are never overwritten")
        shutil.copytree(args.seed_saves.resolve(strict=True), runtime / "Save")

    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.SetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPCWSTR]
    user32.SetForegroundWindow.argtypes = [wintypes.HWND]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]

    def window(pid):
        found = []
        @callback_type
        def visit(hwnd, unused):
            owner = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == pid and user32.IsWindowVisible(hwnd):
                found.append(hwnd)
            return True
        user32.EnumWindows(visit, 0)
        return found[0] if found else None

    if args.existing_modded_pid:
        existing = window(args.existing_modded_pid)
        if not existing:
            parser.error("Existing modded window not found; refusing to replace its session")
        rect = wintypes.RECT()
        user32.GetWindowRect(existing, ctypes.byref(rect))
        if rect.right - rect.left >= user32.GetSystemMetrics(0):
            # Shipwright's default fullscreen shortcut. No gameplay input sent.
            user32.PostMessageW(existing, 0x100, 0x7A, 1 | (0x57 << 16))
            user32.PostMessageW(existing, 0x101, 0x7A, 1 | (0x57 << 16) | (3 << 30))
            time.sleep(0.5)
        user32.SetWindowTextW(existing, "MODDED — Player120")
        user32.SetWindowPos(existing, None, 0, 40, width, height, 0x14)

    environment = os.environ.copy()
    environment["SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"] = "1"
    child = subprocess.Popen([str(exe)], cwd=runtime, env=environment)
    receipt = dict(pid=child.pid, side=args.side, exe=str(exe), runtime=str(runtime),
                   sha256=hashlib.file_digest(exe.open("rb"), "sha256").hexdigest(),
                   presentation_fps=120, debug_field=args.field,
                   existing_modded_pid=args.existing_modded_pid)
    (runtime / "launch.json").write_text(json.dumps(receipt, indent=2))
    deadline = time.monotonic() + 30
    hwnd = None
    while child.poll() is None and time.monotonic() < deadline:
        hwnd = window(child.pid)
        if hwnd:
            break
        time.sleep(0.1)
    if not hwnd:
        raise RuntimeError("Comparison window did not appear; inspect its isolated runtime logs")
    user32.SetWindowTextW(hwnd, "VANILLA — upstream 9eafd15fe" if args.side == "vanilla" else "MODDED — Player120")
    user32.SetWindowPos(hwnd, None, x, 40, width, height, 0x14)
    user32.SetForegroundWindow(hwnd)
    print(json.dumps(receipt, indent=2), flush=True)


if __name__ == "__main__":
    main()
