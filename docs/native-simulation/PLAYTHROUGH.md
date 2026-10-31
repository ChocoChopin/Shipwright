# Ordinary playthrough

Latest gameplay build also fixes airborne braking/steering, uphill force, ledge
yaw and analog escape, following swim/slide, pickup, void and event-clock repairs.
Focused checks pass; the extended slope reaches a real void transition, while
its different fine-step trajectory remains documented. The14 named human
scenarios include the opt-in repeating outdoor Hyrule Field Stalfos duel: high-rate motion,
animation and contacts, with tactical decisions at20. Human acceptance is pending.
The tested executable SHA256 is
`a464940151f15680e34ffac0a9a49b173b50d5ad30a202a8ba484f19f4877603`.
The launcher uses this existing current executable; it does not select a copy
from an older test folder. Build and gameplay coverage: [TESTING.md](TESTING.md).
Remaining review: [COMPATIBILITY_REVIEW.md](COMPATIBILITY_REVIEW.md).
Focused player route and prepared direct-boot menu: [PLAYTEST_ITINERARY.md](PLAYTEST_ITINERARY.md).

Double-click [Play-Player120.cmd](../../scripts/native-simulation/Play-Player120.cmd)
in Explorer to launch. It uses `py -3` when available, falls back to `python`, and
keeps the console open on failure so the error remains visible. It forwards
options, for example `Play-Player120.cmd --prepare-only`. Success adds no prompt.

From the repository root, using Python 3.11 or newer on native Windows:

```powershell
python scripts/native-simulation/play.py --prepare-only
python scripts/native-simulation/play.py
```

The launcher uses the existing `x64/Release/soh.exe` and keeps persistent config,
saves, logs, and caches in ignored `build/playthrough/`. It starts the ordinary
title sequence, followed by file selection. Create a normal save there. Close
that game before launching the same runtime again.

Only the first preparation seeds controller, graphics, enhancement, and cosmetic
preferences from `build/x64/soh/shipofharkinian.json`. It sets Player120 and render
120, disables refresh-rate matching, clears debug/cheat/warp startup state, and
keeps ordinary info/error logging. Later runs preserve the runtime's config and
saves; menu changes, including Player/render rates and startup choice, persist.
No saves or savestates are imported. Ordinary play is separate from automated
replay and does not qualify every gameplay mechanic at higher Player rates.

The three required assets (`oot.o2r`, `soh.o2r`, `gamecontrollerdb.txt`) are
hardlinked from `build/x64/soh/`; no ROM, executable, or full runtime is copied.
The native Windows portable build uses its working directory for config/saves;
the launcher sets it to the isolated runtime. Do not edit the linked assets.
Replacing a source asset with a different file requires reviewing the old link
before replacing it; the launcher refuses mismatched assets.

`--prepare-only` reports the exact executable, SHA256, runtime, config, saves,
asset source, and selected rates without launching. `--runtime build/<name>`
creates another isolated persistent playthrough; relative paths are resolved
from the repository root. Nonempty unmarked directories are refused, protecting
existing test runs and personal saves. `--exe` accepts another existing binary
inside this checkout. The launcher never builds or changes system settings.

The earlier human-accepted reference executable had SHA256
`625659423a25e8351daecf654054bb9169ba3c903b3fc1d605f5a2002495f808`.
That is historical reference identity, not the current build. To enforce the
current tested executable's identity, optionally add:

```powershell
--expected-sha256 a8bad7f202a3d2c5185d6bae7a9751eb662a493fd58e4a680f48dd54e45c84f5
```

Launching writes a compact `build/playthrough/launch.json` receipt including the
PID and actual executable hash. A PID proves process creation only. Preparation
does not prove the game started, rendered, or passed human gameplay acceptance.
