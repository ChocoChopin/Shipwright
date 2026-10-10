# Pass 4C human pilot: ordinary indicators and persistent status

## Human follow-up: render pacing and C-Up/camera controls

### Subsequent idle slowdown correction

Human testing rejected the prior build: after roughly five seconds of Player120
with no input, render FPS fell from the110s to20 or below. The short fixture's
zero-drop result did not qualify sustained live pacing.

For render FPS at least Player Hz, backend Present pacing now owns the wall clock
exactly as in ordinary interpolation; logical Player/render ordering remains
unchanged. The extra per-world waits are skipped. Previously each world anchored
a fresh50-ms minimum to actual host start; overhead accumulated against DXGI's
continuous deadline, and dropped render frames could not recover that deficit.
High-Player/low-render mode still uses the precise deadline timer.

Build passed; executable SHA256: `12fad3bbfefada364adb059415b1f143fe026bcc7b68a732280ad25ccdbc6364`.
This is a targeted pacing correction, not a human acceptance claim. No automated
UI control, game launch or regression campaign is requested for this revision.
The user will inspect the updated executable. C-Up's second retained verifier
failure (missing pose at first-person draw) remains unresolved and is not reported
as a passing first-person suite.


The previous unrestricted build was not human-accepted: the user reported
25-40 FPS with Player120, missed C-Up, and broken Z-targeting. Source fixes:

- Replace coarse Windows sleep with a process-local high-resolution waitable
  timer and a bounded final deadline check. A requested8-ms sleep measured16.54ms
  in this session. Start CPU rendering one presentation interval before its due
  time; the backend owns Present pacing. No system timer settings are changed.
- Keep the backend target at the actual presentation FPS, including Match Refresh
  Rate. A Player120/render60 transaction submits three frames, not six; telling
  DXGI to expect120 creates artificial late-frame drops.
- C-Up, Start and Navi-on-L edges survive intermediate Player consumption until
  the world input opportunity. They are not repeatedly offered to Player steps.
  B/Z/R still reach the next Player step.
- When the high-rate camera adapter cannot service a live camera profile, execute
  the normal camera dispatcher at the world slot. Previously that slot returned
  early anyway, freezing first-person and unconverted target camera modes.
- Keep ordinary rendering interpolation for world-only cameras instead of
  overriding it with a held world view. No Player eligibility gate is restored.
- The menu reports the resolved render target rather than an overridden slider.

The same render120 target before the timer correction gave
canonical20:210/210 rendered,0 drops,2.089s; Player120:145/210 rendered,65 drops,
2.778s. These compact fixture timings include harness overhead and are not a
fullscreen GPU benchmark. The limiter mismatch does not explain the user's
same-target120 report. Additional fixtures cover an intermediate C-Up tap and
Z acquisition; receipts count actual rendered/dropped frames and first-person
world-edge observations without full traces. After the timing correction,
Player120 rendered210/210 with0 drops in2.228s;210 Player poses/steps and159 held
target checks passed. The C-Up run entered camera mode6 then stopped normally
with a verifier failure: the idle/slash phase rule incorrectly expected first
person to advance its base animation. Source Player_Action_8084B1D8 deliberately
holds that animation when unk_6AD==1; the verifier now checks that hold exactly.
The failure ring is retained. C-Up rerun and startup pending.

## Subsequent user-requested unrestricted live mode

The user explicitly removed the fixture-profile eligibility requirement after
the shield check and permanent nearby-OC latch prevented live play. Selecting
60/120 now bypasses the Player profile/pose veto and button whitelist in live
play. Transient input/arena failures retry at the next world boundary; no manual
Original/120 rearm is required. Canonical20 and historical constrained fixture
mode remain unchanged. The new `unrestricted_player=1` fixture option exercises
the live policy with no shield and ordinary nearby world colliders.

Normal pause/lifetime, valid memory and queue requirements still apply. Camera
modes without a high-rate adapter keep their ordinary world camera update without
vetoing Player cadence. Draw-disabled/special draw paths retain their world pose
ownership. Terminal-velocity crossings use clamped fractional movement rather
than falling through to a full canonical displacement on each substep. This is
an unrestricted experiment, not a claim that every action/contact is converted.
The sign bridge remains sign-specific; broad combat is not newly qualified.

Validation: build passed; unrestricted no-shield idle/walk near ordinary world
colliders completed960 Player steps over160 world ticks at effective120, no latch;
unrestricted no-shield ready-sword slash completed210 steps over35 world ticks;
ordinary startup closed normally. One run each, no broad suite, no native crash.
Receipts: `build/pass4c-unrestricted/`; total diagnostics approximately164KB.
Current executable SHA256:
`7a5cdf02dcbc7a0bd602eebb58c9f51aca95a6c54081d29e171da9b048af32cf`.
Executable path/asset working directory are unchanged. Human gameplay confirmation
remains pending. The results and restricted-mode instructions below describe the
preceding fix, including its now-superseded executable hash and rearm requirement.

The human pilot exposed a gap in the controlled fixtures: ordinary Kokiri Forest
hint/fidget state was rejected by the pose mask. This correction is limited to
that admission gate, its world-owned indicators, and truthful status reporting.
Starting checkpoint: `1452ba93b068daececf03d0dfbed32c317f49bfc`.
Implemented and qualified on 2026-10-09; human replay of the fix remains pending.

## Classification and ownership

- `PLAYER_STATE2_NAVI_ALERT` (`0x00200000`) is a passive hint/UI indicator.
  Navi's world actor supplies `naviTextId`; Player talk/hint handlers set the bit;
  world interface code consumes it. Those handlers and ordinary hint clearing
  now run only at the existing world opportunity. A negative Navi text ID means
  forced conversation and remains a high-rate blocker.
- `PLAYER_STATE2_IDLE_FIDGET` (`0x10000000`) marks an ordinary idle animation,
  not a new coupled action. Existing idle selection/RNG stays world20. Animation
  advancement and authored SFX crossings use the already integrated Player
  animation adapter. Equipment/action/resource/geometry gates still apply.
- Existing talk-offer/Navi-active indicators are explicitly grouped separately
  from admitted Player flags. Every other previously rejected state2 bit stays
  rejected, including crawl/hop, DynaPoly movement/grab, enemy grab, underwater/
  dive, freeze, spin, ocarina, reflection and disabled draw.

`Player_PoseUnexpectedState2` and `Player_FormatState2` report the full hexadecimal
value, unexpected mask and symbolic bit names. No flags are masked out of live
Player state to gain admission. Production admission does not depend on tests.

`PlayerTemporal_RateStatus` exposes persistent admitted Hz, requested Hz,
admission, current rejection and a separate latched fallback/original reason.
Host-frame reset does not change admitted Hz. Admission loss, scope/lifetime
invalidation and canonical selection do; rearming remains Original then60/120.
The current rejection is refreshed at each world admission check even while
fallback is latched. Current-step timing APIs retain their transient semantics.

## Focused validation

The only new fixture seeds the ordinary saved Navi
hint timer at600; the engine produces the actual hint and fidget bits. It idles
then walks using pad input, collects compact bit masks, checks world opportunities
and persistent status across host-frame resets. It never directly sets Player
state bits. Pure bit-policy/formatter checks operate on a detached Player copy.
One ready-sword120 case, one canonical idle comparison and ordinary startup
complete this narrow gate. No matrix, broad regression or repeated success runs.

All requested checks passed, once each:

- Build: existing Windows toolchain, `baseline.py build --jobs 4`, exit0.
- Live-like idle/walk120: 160 world transactions, 960 Player steps, 52 moving
  intervals. Natural observed masks were `0`, `0x00200000`, `0x00200020` and
  `0x10200000`. The previously unexpected union was exactly `0x10200000`:
  **NAVI_ALERT and IDLE_FIDGET**. No other new state2 bits needed admission.
  Both the ordinary hint and natural fidget were observed; no Player bits injected.
  World producer guards passed, status stayed admitted120 across 160 host-frame
  resets, and final fallback was unlatched. All32 detached bit-policy checks pass;
  the diagnostic control prints `stateFlags2=0x00040000 unexpected=0x00040000
  [PLAYER_STATE2_CRAWLING]`.
- Ready-sword slash120: 35 world transactions, 210 Player steps, two history
  warm-ups and six valid sweeps; input/animation/world guards pass.
- Canonical idle20: all41 semantic checkpoints match the retained Pass4A reference
  across40 transactions. Direct repeated presentation purity passes in all three
  cases. No golden was replaced.
- Ordinary startup: one visible window, one scene initialization, normal exit0.

Receipts: `build/pass4c-pilot-fix/{high,canonical,startup,engine-build}` and
`receipt.json`. Diagnostics total approximately211KB, including build logs;
the live-like engine output is23,693 bytes. No native crashes, failed cases,
repeat runs, new full traces or evidence deletions. Broader historical campaigns
were intentionally omitted. Dependency pins and assets are unchanged.

Current executable: `C:\Users\Chopin\Documents\ChatGPT\Harkinian\x64\Release\soh.exe`.
SHA256: `f8d508aa5ee58c1bc5827ea8ec7ba53ca7a64b2f01a6cc15fc17f45d490ac8e6`.
The embedded Git label precedes the fix commit; build/source provenance is in the
receipts, not inferred from that label. Launch with asset working directory:

```powershell
Start-Process -FilePath 'C:\Users\Chopin\Documents\ChatGPT\Harkinian\x64\Release\soh.exe' -WorkingDirectory 'C:\Users\Chopin\Documents\ChatGPT\Harkinian\build\x64\soh'
```

Disable Vsync, hold render FPS constant, and select **Settings → Graphics →
Experimental Player Hz**. Choose Original once, then60/120 to clear any prior
latch. Status now separately labels requested/effective rate, admission, latch,
latest rejection and original latch reason. Future genuine blockers still fall
back; the change does not authorize arbitrary equipment/actions/geometry.

The test is a live-like engine session, not a capture of the user's earlier live
process or human gameplay acceptance. Existing unsupported profiles remain
unsupported. Launch/menu instructions remain those in [PASS4C.md](PASS4C.md).
