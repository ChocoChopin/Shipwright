# Pass 4C: bounded Player/sign contact bridge

**Current pilot executable:** see [PASS4C_PILOT_FIX.md](PASS4C_PILOT_FIX.md) for the
subsequent ordinary Navi/fidget admission and persistent-status fix. The build
identity and results below are the preserved original Pass4C checkpoint.

Complete: automated qualification and human-test build, 2026-10-09.
Started at `2e51b2dccf296fb939361d102164ff3fde342c73`; implementation checkpoint
`de35c8f8fb4c334e3224740069dd9d2f2ac3bfcb` is pushed. The subsequent handoff
commit changes documentation only. Human gameplay acceptance remains pending.
Authoritative Player60/120 sword contact with the admitted world20 sign works.
Canonical Player20 retains legacy contact. This is not general combat support.

## Implementation contract

- Capture registered authoritative sign cylinders after world actor work, before
  committing the first late Player pose; reuse held geometry between world ticks.
- Scene, world and monotonic actor generations bind copied geometry and masks.
  Actor addresses are live lookup keys only; events/proxies contain no pointers.
- Changed active sword sweeps alone produce candidates, in quad/triangle order.
  No high-rate insertion into global collision lists.
- Reserve by scene/Player/scope, attack epoch, authored opportunity, target
  generation and element group. Retain reservations through the attack after
  delivery to prevent subsequent observations from delivering again.
- Deliver due events after legacy AT at the next world collision boundary,
  before Player and target updates. Both sides respond at world20. Target cooldown,
  piece spawning, RNG and effects keep the existing actor update slot.
- Sign cut animation comes from the producing event only for bridged hits;
  canonical contacts continue reading live Player animation.
- Scope/scene/actor invalidation rejects pending events explicitly. Completed
  delivery data remains valid through the synchronous world actor traversal.
- Exclude sign body overlap at current/predicted Player positions, unknown shapes,
  actors, hostile contacts, alternate melee, dynamic surfaces and reciprocal OC.

## Code and lifetime

`PlayerContactCore.hpp::ContactReservations` owns 32 bounded reservations with
monotonic sequence, explicit reserve/commit/reject, and attack-lifetime dedup.
`PlayerContactBridge.cpp` owns `Capture`, `Detect`, `Consume`, `Invalidate`,
`Scene`, `Created`, `Destroyed`, and the C admission/sweep/animation helpers.
Proxies copy cylinder/position, scene/world/target generations, parts/cooldown,
AC masks/flags and material/effect. Collider/element identity is the admitted
sign cylinder/group zero. Events additionally capture Player/scope, epoch,
opportunity, step/end-time, quad, damage, hit position and producing animation.
Delivery ColliderInfo is stable through the same world traversal only.

`PlayerTemporal.cpp` wires actor/scene/scope lifetimes, world capture, committed
Player detection, and `collision.after_at` consumption. `z_player_lib.c` marks
only changed active sweeps and admits sign OC only when both current and predicted
Player cylinders are disjoint (including a two-unit margin). The Player actor
gate accepts only bridge-managed hit flags. `z_en_kanban.c` substitutes captured
animation only for bridged delivery; cutting/spawning/cooldown code is unchanged.
`SohMenuSettings.cpp` exposes selection and live status. NativeSimulationTest,
six contact fixtures, runner accounting, CLI controls and native core tests provide
focused verification. `compare_contact_audit.py` projects retained historical
traces into compact canonical contact receipts; it generates no new full trace.

Whole-profile fallback withholds remaining high-rate starts, preserves queued
logical input and latches Player20 at the shared boundary. Pending contacts are
rejected on invalidation; already committed deliveries survive their world actor
traversal. Unknown/hostile actors or shapes, reciprocal body OC, dynamics, water,
damage, unsupported items/actions, spin/multi-hit, projectiles, parries and grabs
remain excluded. Original20 never runs bridge detection. No world actor was retimed.

## Focused results

All paths below are ignored local evidence under `build/pass4c-final/`.
One run per case; reruns addressed concrete fixture/tooling changes, not a matrix.

| Gate | Result / receipt |
| --- | --- |
| Ordinary sign120 / sign60 | Each one horizontal cut (type2), one commit, zero pending; 210 / 105 Player steps, 35 target updates each (`contact/`) |
| Z-sign120 / Z-sign60 | Each correct real friendly target, one vertical cut (type1), one commit, zero pending; 210 / 105 Player steps, 35 target updates each |
| Repeated observation120 | Two quad observations, one reservation/commit/cut, one duplicate suppressed; 210 Player steps |
| Target invalidation120 | One reservation rejected, zero commits/cuts/pending; 26 high steps then canonical fallback, 5 target updates before removal |
| Canonical slash/sign/Z-sign/combo | Four exact semantic comparisons, 360 world transactions against retained Pass4A references (`canonical/`) |
| Strict canonical Z-sign contact | 101 common boundaries, 3,052 ordered phase/registration/contact events exact against retained Pass4B trace (`canonical/contact-audit.json`) |
| Direct Player presentation purity | Two extra helper emissions per packet; packet/commands/live state unchanged, including 56 packets in mixed-rate invalidation |
| Original draw/RNG + HUD warning | Both exact, 180 world transactions (`original/`, `hud/`) |
| Rendering independence | Z-sign Player120/render120 matches Player120/render20 at all 36 snapshots; 210 final-build high steps (`render-comparison.json`) |
| Native core / animation-input / Python | 237 checks / 35 checks / 152 tests |
| Online validator / CLI | 22 cases, 66 typed hashes, two SHA vectors, bounded ring / 48 CLI checks (`online/`, `cli/`) |
| Graceful mismatch / purity controls | Expected normal exit2; mismatch at boundary6 retains four world states and 32 substeps; deliberate semantic mutation detected (`mismatch/`, `purity-control/`) |
| Ordinary startup | Visible game window, scene initialized, 12-second observation, normal close exit0 (`startup/smoke.json`) |

Six primary contact cases total 210 world transactions and 866 high Player steps;
five committed cuts and one rejected event. Target cooldown and actor/collision/
blink/scripts/environment/HUD/message/audio world guards pass online. The repeated
observation runtime case proves both-quad dedup; portable core checks separately
prove suppression across distinct Player step IDs and completed reservations,
plus independent new opportunities/generations. It does not claim sustained
geometric overlap across many live substeps.

Source review covered ordering, canonical bypass, duplicate delivery, stale
generations, deferred pointer ownership, held geometry, world guards, cooldown,
and fallback. No further runtime edits were needed after focused acceptance.

Omitted deliberately: historical giant cross-products/repetitions, runtime30,
all-ten canonical rerun, startup stress, unrelated texture/TLUT/coverage reruns
(renderer and dependency pins unchanged), general enemy/combat QA, and benchmarks.
Higher-rate state need not equal canonical pose bit-for-bit; original canonical
arithmetic/order and semantic hashes remain the reference.

## Diagnostics, cost and retained failures

Entire pass diagnostic inventory: approximately **3.2 MB**, including failure
buffers and build logs (`footprint.json`, excluding assets/native build products).
Primary successful outputs are **11,652–12,801 bytes per case**. No successful
full snapshots/substep traces were generated. The canonical contact reference
projection is 10,962 bytes; the pre-existing full reference remains unchanged.
Cleanup manifest records no evidence deletion; runner asset releases are recorded
in receipts. One current build and the existing canonical reference are retained.
Excluded asset hardlink logical sizes are not additional physical disk usage.

The ordinary120 bridge made 52 cylinder queries across 18 active-query steps:
11.0 microseconds total, maximum 3.4 microseconds for a detection call on this
machine. This bounds query overhead only, not full engine/validator cost or a
gameplay frame benchmark. The high-rate run completed its cadence/world guards;
interactive feel/performance still requires human testing.

Retained normal failures (none were native crashes):

- `build/pass4c-01/sign120`: separate targeting-fixture location hit existing nearby
  OC exclusion. Recipe moved to already-qualified ready-sword location; gate retained.
- `build/pass4c-02/contact/player-contact-invalidate-120`: expected target removal
  correctly caused fallback; fixture incorrectly required uninterrupted high rate.
- `build/pass4c-03/invalidation`: engine rejection checks passed, but wrapper
  incorrectly expected 210 purity packets instead of 56 after fallback. Accounting
  now uses actual high world transactions plus canonical transactions.

All historical failures remain preserved. No new native crash occurred in Pass4C.

## Source/build identities

Final executable: `x64/Release/soh.exe`, SHA256
`5d4a5ef4c853e67d03802e074bf5a74906efabfa2087989ddb5500c83a0529ee`.
Native source content is checkpoint `de35c8f8fb4c334e3224740069dd9d2f2ac3bfcb`;
embedded build version still says starting `2e51b2d` because compilation preceded
the commit. Do not mistake that embedded label for the tested source identity.
Final build invocation/log: `build/pass4c-final/audit-build/`.

Primary six contact cases used executable
`4891c7e27644d2259d56dfef78f787b3548e66a48ddb0dd675109b2631ad8d77`
(`engine-build/`). Final build adds only compact canonical audit instrumentation;
bridge/scheduler/selector gameplay source is identical. Their evidence is reused
explicitly. Final executable passed canonical/broad/CLI/online/controls/startup
and the Z-sign120/render120 comparison above. No docs-only rebuild is needed.

Pins unchanged: libultraship `9280b17ddc504da6630892a46440e86be41ac571`;
torch `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`.
Asset SHA256: oot.o2r `a058767a2f4f8f415b099a5c5189a9bf974a068f331b88131afea8df24e6a997`;
soh.o2r `51c8b166b913fdc745901fc48a2ca6261e480e3e8c0715ae0f2d946adb982712`;
gamecontrollerdb `e606134678e6b3fdbdec9081289a1f0ba3e25d53b59b2aa3cdfe69bf491ad18f`.
No assets, binaries, traces or build products are committed.

Rebuild from repository root using the existing configured Windows toolchain:

```powershell
$env:TEMP="$PWD\.tmp"
$env:TMP=$env:TEMP
$env:PostBuildEventUseInBuild='false'
& 'C:/Users/Chopin/AppData/Local/Programs/Python/Python312/python.exe' -B scripts/native-simulation/baseline.py build --jobs 4
```

## Human test handoff

Launch the current executable with its asset/config working directory:

```powershell
Start-Process -FilePath 'C:\Users\Chopin\Documents\ChatGPT\Harkinian\x64\Release\soh.exe' -WorkingDirectory 'C:\Users\Chopin\Documents\ChatGPT\Harkinian\build\x64\soh'
```

1. Load a child-Link save with Kokiri sword and default Deku shield. Use ordinary
   dry static ground in Kokiri Forest. This does not create/overwrite a personal save.
2. Open F1, **Settings → Graphics**. Disable Vsync and keep render FPS fixed at120
   (or another fixed supported value) across comparisons.
3. Set **Experimental Player Hz** to **Original / 20 Hz**, draw the sword, then
   select60 or120. The setting is `gDeveloperTools.NativePlayerHz` in the local
   configuration. Live status shows requested/effective Player Hz, world20, render
   FPS and fallback reason. Check effective rate rather than assuming admission.
4. Compare idle camera/control, immediate B with sword ready, repeated ordinary
   slashes, movement/turning into slash and static-wall interaction.
5. Face an ordinary cuttable sign without pressing Link's body into it. Try normal
   slash, then Z-targeted slash; inspect the cut and confirm target acquisition.
6. Compare Original20/60/120 with rendering unchanged. If unsupported action or
   profile latches fallback, choose Original then the desired high rate to rearm.

Unsupported equipment/actions, body pushing, water/dynamic geometry and hostile
combat can legitimately remain Player20. Automated fixtures prepare a controlled
scene; this handoff does not claim any arbitrary save/location is admitted.
Human play and subjective feel have **not** been tested. The next step is the
user's bounded pilot feedback; no additional implementation pass is required
before trying admitted authoritative120 sword/sign contact. Further combat or
world conversion needs a separately scoped request. Stop here.
