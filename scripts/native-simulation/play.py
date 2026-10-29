"""Prepare or launch ordinary play in a persistent, repository-local runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
ASSET_NAMES = ("oot.o2r", "soh.o2r", "gamecontrollerdb.txt")
CONFIG_NAME = "shipofharkinian.json"
MARKER_NAME = ".playthrough.json"


def inside(path, parent):
    """Resolve junctions/symlinks before accepting any writable destination."""
    resolved = path.resolve()
    if resolved == parent or not resolved.is_relative_to(parent):
        raise ValueError(f"Path must be strictly inside {parent}: {resolved}")
    return resolved


def read_config(path):
    config = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(config, dict) or not isinstance(config.get("CVars", {}), dict):
        raise ValueError(f"Expected a configuration object: {path}")
    return config


def initial_config(source):
    # Seed preferences only. Do not inherit save/archive redirections, warp
    # points, debug state, cheats, randomizer settings, or open debug windows.
    seed = read_config(source)
    config = {key: seed[key] for key in ("ConfigVersion", "Window") if key in seed}
    source_cvars = seed.get("CVars", {})
    cvars = config["CVars"] = {
        key: source_cvars[key] for key in ("gSettings", "gEnhancements", "gCosmetics")
        if key in source_cvars
    }
    settings = cvars.setdefault("gSettings", {})
    settings.update(BootSequence=0, InterpolationFPS=120, MatchRefreshRate=0)
    developer = cvars["gDeveloperTools"] = {
        "NativePlayerHz": 120,
        "PlayerFrameProfile": 0,
        "NativeHorseRideStart": 0,
        "DebugEnabled": 0,
        "DebugSaveFileMode": 0,
        "ResourceLogging": 0,
        "LogLevel": 2,  # ordinary info/error logs; no trace/profile harness
    }
    # Preserve the accepted presentation option, which is not a test mode.
    source_developer = source_cvars.get("gDeveloperTools", {})
    if "DeferVisibilityReadback" in source_developer:
        developer["DeferVisibilityReadback"] = source_developer["DeferVisibilityReadback"]
    return config


def prepare(runtime_arg, exe_arg, expected_sha256=None):
    if os.name != "nt":
        raise ValueError("This launcher requires native Windows")
    # Validate the checkout and every destination before creating anything.
    for name in (".git", "AGENTS.md", "docs/native-simulation/README.md"):
        if not (ROOT / name).exists():
            raise ValueError(f"Not the expected Shipwright checkout: missing {ROOT / name}")
    build = inside(ROOT / "build", ROOT)
    runtime = inside(ROOT / runtime_arg, build)
    exe = inside(ROOT / exe_arg, ROOT)
    if not exe.is_file():
        raise ValueError(f"Missing executable: {exe}")
    assets = inside(ROOT / "build/x64/soh", build)
    if (runtime == assets or runtime.is_relative_to(assets) or assets.is_relative_to(runtime)
            or exe.is_relative_to(runtime)):
        raise ValueError("Runtime must be separate from the executable and asset source")
    for name in ASSET_NAMES:
        if not (assets / name).is_file():
            raise ValueError(f"Missing required asset: {assets / name}")
    with exe.open("rb") as stream:
        sha256 = hashlib.file_digest(stream, "sha256").hexdigest()
    if expected_sha256 and sha256 != expected_sha256.lower():
        raise ValueError(f"Executable SHA256 mismatch: expected {expected_sha256}, got {sha256}")
    marker = runtime / MARKER_NAME
    expected_marker = {"format": 1, "root": str(ROOT), "runtime": str(runtime)}
    for name in (MARKER_NAME, CONFIG_NAME, "Save", "cache", "logs", ".tmp", "launch.json", "imgui.ini"):
        inside(runtime / name, runtime)
    fresh = not marker.exists()
    if fresh:
        if runtime.exists() and any(runtime.iterdir()):
            raise ValueError(f"Refusing to adopt a nonempty, unmarked runtime: {runtime}")
        config = initial_config(assets / CONFIG_NAME)
    else:
        if json.loads(marker.read_text(encoding="utf-8")) != expected_marker:
            raise ValueError(f"Runtime marker does not match this checkout: {marker}")
        config = read_config(runtime / CONFIG_NAME)
        if (runtime / CONFIG_NAME).stat().st_nlink != 1:
            raise ValueError("Persistent configuration must not be hardlinked to another runtime")
    for name in ASSET_NAMES:
        target = runtime / name
        if target.exists() and not os.path.samefile(assets / name, target):
            raise ValueError(f"Asset differs from its source; review before replacing: {target}")

    runtime.mkdir(parents=True, exist_ok=True)
    for name in ASSET_NAMES:
        target = runtime / name
        if not target.exists():
            os.link(assets / name, target)  # no asset-copy fallback or binary copies
    if fresh:
        (runtime / CONFIG_NAME).write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        marker.write_text(json.dumps(expected_marker, indent=2) + "\n", encoding="utf-8")
    (runtime / ".tmp").mkdir(exist_ok=True)
    cvars = config.get("CVars", {})
    receipt = {
        "exe": str(exe), "sha256": sha256, "runtime": str(runtime),
        "config": str(runtime / CONFIG_NAME), "saves": str(runtime / "Save"),
        "asset_source": str(assets), "assets": list(ASSET_NAMES),
        "initialized": fresh,
        "player_hz": cvars.get("gDeveloperTools", {}).get("NativePlayerHz", 20),
        "render_fps": cvars.get("gSettings", {}).get("InterpolationFPS", 20),
        "boot_sequence": cvars.get("gSettings", {}).get("BootSequence", 0),
        "command": [str(exe)],
    }
    return exe, runtime, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=Path("build/playthrough"),
                        help="Persistent directory inside repository build/ (relative to repository)")
    parser.add_argument("--exe", type=Path, default=Path("x64/Release/soh.exe"),
                        help="Existing executable inside this repository; never rebuilt or copied")
    parser.add_argument("--expected-sha256", help="Optional executable identity gate")
    parser.add_argument("--prepare-only", action="store_true", help="Validate/setup without starting the game")
    args = parser.parse_args()
    try:
        exe, runtime, receipt = prepare(args.runtime, args.exe, args.expected_sha256)
        receipt["prepare_only"] = args.prepare_only
        if not args.prepare_only:
            environment = os.environ.copy()
            environment.update(TEMP=str(runtime / ".tmp"), TMP=str(runtime / ".tmp"))
            # No fixture, debug-warp, automated-input, or trace arguments.
            child = subprocess.Popen([str(exe)], cwd=runtime, env=environment)
            receipt["pid"] = child.pid
            (runtime / "launch.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(receipt, indent=2), flush=True)
    except (OSError, ValueError, TypeError) as error:
        parser.exit(1, f"play.py: {error}\n")


if __name__ == "__main__":
    main()
