# Pass 4B: gated fixed Player scheduler and dependency integration

Status: complete under the user's lean, online-validation engineering policy.
The internal non-contact Player profile executes at 60/120 Hz; broad gameplay
support, public selection and the world contact bridge are not claimed.
Starting source: `6153eb451a37dfc6b3bfa6f6edebece85d9e08e8`, clean branch
`mod/native-simulation-rates`, origin `ChocoChopin/Shipwright`. Verified dependency
pins: libultraship `9280b17ddc504da6630892a46440e86be41ac571`, torch
`2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. Both submodule worktrees are clean.

## Scope and implementation sequence

The checkpoint sections below are chronological evidence, not competing current
status statements. The final accepted scope and lean-policy changes follow.

## Final online harness and handoff

Validated source checkpoint: `cc75c8f694dfdb505140663152ab507d0357dd2a`, pushed
to `ChocoChopin/Shipwright`, `mod/native-simulation-rates`. The final handoff adds
this documentation and regenerated inventory; gameplay source remains the accepted
`a98fd0bc617154288c963a65dc1e9ba6f77b52f1` milestone apart from a comment correction.
The diagnostic change was required engineering work, not merely archive deletion.

`NativeSimulationValidation.hpp` owns typed streaming SHA-256 and online fixture
assertion accumulators. `WriteSnapshot` hashes the complete existing semantic
closure at each world boundary and checks supplied reference hashes immediately.
`NativeSimTest_PlayerStepCommitted` uses allocation-free `PlayerTemporal_Observe`
and typed observations for motion/animation/control/held-target/pose checks.
`CheckPose` checks sword warm-up, no-motion and retained sweep history; registration
and contact hooks reject unbridged high-rate events in process. Trace-disabled
hooks no longer construct large Player JSON objects that would be discarded.
`analyze_player_rates.py` accepts the bound online receipt; legacy explicit-detail
analysis remains available. `run_corpus.py` normally reads only small receipts.
Historical reference JSONL is migrated once to versioned hashes, preserving the
original file hash. Existing goldens were not replaced with candidate values.

Normal success contains checkpoints, assertion/validation metrics and sparse
button events. Four world-state JSON trees and 32 typed Player observations stay
in memory; the latter ring is 106,768 bytes. Failure dumps only this bounded recent
history, not an invented complete run. Full history remains explicit diagnostic
output. Distinct-value assertions saturate at the requested threshold with a
saturation marker; their pass/fail criterion is unchanged. No gameplay logic or
rate acceptance was changed to reduce logging.

Focused qualification (`build/pass4b-26/`):

| Gate | Result |
| --- | --- |
| Asset-free native/Python parity | `online-unit-final`: 22 passing/failing assertion cases covering all ten kinds and nonpositive distinct thresholds; 66 exact typed hashes, two SHA-256 known vectors, bounded ring overwrite order, two graceful CLI controls |
| Python/tooling | 151/151; `python-tests.log` |
| Player120 movement/turn/slash | One run, 50 world transactions / 300 Player intervals; exact online reference hashes and direct purity; `compact-high`; 14,076 total output bytes |
| Canonical Z-sign | One run, 100 world transactions; exact unchanged Pass 4A hashes and direct purity; `compact-canonical`; 18,095 total output bytes |
| Held-target Player60 validator branch | One run, 35 world transactions / 105 Player intervals; target acquisition, 63 held-target comparisons and world guards pass; `compact-target`; 7,834 total output bytes. No new canonical hash baseline is claimed for this case. |
| Detailed-analyzer parity | `analyzer-parity.json`: 20 motion/animation/control/pose/sword metrics match the previously accepted detailed analyzer across the two high-rate cases. The target world-update counter now counts all fixture-target updates including before acquisition, rather than only post-acquisition updates. |
| Graceful online mismatch control | `compact-mismatch-control`: changed one hash in a private reference copy; normal exit 2 at boundary 6, exactly four boundary records and 32 substep records, 681,168 bytes of retained failure detail; no native fault |

The high-rate movement case previously emitted 8,002,151 bytes of snapshots alone.
It now emits 14,076 bytes for its entire output, with the same reference hash and
purity checks. The complete new qualification directory, including the deliberate
failure and build receipts, measured 1,082,970 bytes. No wall-time speedup is claimed:
startup/purity work and concurrent cleanup affect duration. Avoided serialization,
external parsing and discarded trace construction are concrete computation savings.

Engine cases used SHA-256
`75bcd259d9fd522d95423175e56945d1f9f08909ba41b8d2d4a878bde7ba6c7b`.
Final executable is 27,228,160 bytes, SHA-256
`ebfce17be684bfbb7b3b3e2c587aac9c20874df9313850f8e510360226b7279c`;
`engine-build/identity.json` binds final source file hashes and build receipts.
Later edits only reject incomplete reference arguments/stale outputs and bound
threshold bookkeeping (including negative/zero thresholds). Final asset-free
native checks passed; the engine campaign was not repeated for these changes.
The earlier ordinary startup result below is source-reused; no human gameplay.

The asset-free check initially tried captured stdout, which is unavailable for
this GUI executable; explicit receipt-file output fixed that infrastructure error.
An initial run preflight named the binary directory rather than the existing asset
directory and stopped before process launch. Neither was an engine failure.
No new native crash occurred, and every historical failure remains preserved.

Two applied successful-trace manifests (`build/lean-cleanup/pass4b-success.json`
and `project-success.json`) removed 2,698 redundant files totaling 36,122,260,345
logical bytes. The separate obsolete-build manifest removed 451,744,768 bytes.
Failures, receipts, reference hashes and the selected canonical Z-sign/high-rate
movement examples remain. `reference-hash-migration.json` identifies migrated
reference runs. Free space measured 31,699,415,040 bytes (about 29.5 GiB) after this
work; logical deletion totals are not equated to physical free-space changes.
One current build and one canonical reference build remain, plus explicitly
preserved crash-associated historical binaries.

The three focused cases above replace any further matrix or repetition campaign.
The old presentation omissions, broad regression and startup results below are
unchanged. Source inventory now covers 2,444 files / 700,633 lines, 429 actors,
zero missing sources and one preexisting unmapped source. The next authorized
boundary remains Pass 4C; no contact bridge or broader gameplay was started.

## Lean-policy supersession and prior validation identity

During final validation the user explicitly replaced the archival policy: default
one run per case; repetitions only for a concrete determinism/flakiness question;
common-boundary snapshots and event logging by default; retain full failures and
only needed representative successful traces; cap pass diagnostics at 1 GB and
target well below 500 MB. This supersedes earlier matrix/repetition and permanent-
raw-retention requirements below, without weakening gameplay assertions or exact
comparisons. The running presentation campaign was stopped by direction, not by
a native failure. No additional 60-Hz stepping cohort or full matrix was started.

The superseded logging-only rebuild gated full `player-steps.jsonl` on explicit temporal/QA
observation. Scheduler/world/target assertions remain unconditional. Default output
contains common-world-boundary snapshots and compact button events. The single
move/turn/slash120 check exactly matches the prior runtime and passes purity;
its event file has two records / 357 bytes, no substep stream. A single canonical
Z-sign run also matches the unchanged reference. Receipts: `pass4b-25/lean-high`
and `pass4b-25/lean-canonical`.

Superseded logging-only executable: 27,171,328 bytes, SHA-256
`c2d9b69a1735bbc0ccf7965610bf93324531c335ffd57277d191276a0575f83e`;
build receipt `build/pass4b-25/engine-build`. The prior validated runtime executable
is retained as the sole working canonical reference at
`build/canonical-reference/soh.exe`. Crash-associated historical binaries remain
failure evidence. The cleanup manifest `build/lean-cleanup/pass4b-success.json`
records 410 deleted successful JSONL files / 6,550,121,035 bytes, with paths,
sizes and hashes. Receipts, metrics, hashes and failures are retained. Canonical
Z-sign and high-rate movement traces are the two selected success examples for
distinct canonical-contact and Player-rate behavior. The obsolete successful
engine-build manifest records six removed backup files / 451,744,768 logical bytes.
Free space returned to 18.07 GiB after the first cleanup; physical free-space
change is not equated to logical file sizes or exclusively attributed to deletion.
At that checkpoint retained Pass 4B JSONL was 212.15 MiB; the 523.13-MiB logical
folder total additionally counted failure directories' hardlinked game assets.

The gameplay runtime source is milestone `a98fd0bc617154288c963a65dc1e9ba6f77b52f1`.
The handoff adds documentation, regenerated source inventory, purity-analysis
counting and tests, online validation/compact diagnostics, and a comment correction in
`z_camera.c`; none changes gameplay or assertion semantics. The earlier full-trace
validation build is retained in receipt form at
`build/pass4b-24/engine-build`. MSVC 19.44.35228 and the existing Windows build tree
were used. The executable is 27,170,816 bytes, SHA-256
`e10f489ee2bd07fcf2072f5a03817ee6ca2313edcf33400b3845b1ef90596f42`.
The clean-source receipt is `build/pass4b-24/high-reference/corpus_result.json`;
later receipts additionally record documentation/tooling diffs. Dependency pins
are unchanged from the starting identities above. Local asset SHA-256 identities:

- `oot.o2r`: `a058767a2f4f8f415b099a5c5189a9bf974a068f331b88131afea8df24e6a997`.
- `soh.o2r`: `51c8b166b913fdc745901fc48a2ca6261e480e3e8c0715ae0f2d946adb982712`.
- Controller database: `e606134678e6b3fdbdec9081289a1f0ba3e25d53b59b2aa3cdfe69bf491ad18f`.

No assets, binary or trace data are committed. Raw successful streams named by
cleanup manifests have been removed; historical receipts below remain valid
records of checks performed, not promises that every raw stream still exists.
The prior 75 renderer-helper
checks (texture 24, TLUT 31, coverage 20) are source-reused after rehashing every
bound input and receipt in `build/pass4b-24/helper-reuse.json`; they were not rerun.
The old 48 omitted Player presentation variants and 39 startup-stress runs remain
omitted. No runtime 30-Hz cases, full original/HUD cross-product, dynamic geometry,
target damage, hostile shield, hardware throughput benchmark, physical-controller
latency measurement, pixel/GPU validation or human gameplay is claimed.

The high-rate selector is an internal fixture property, not a public gameplay
setting. Actual Player authority does run at 60/120 Hz in the admitted profile;
world damage/contact registration is deliberately inactive. First interactive
target-contact qualification requires Pass 4C: generation-bound contact identity,
authored attack opportunities, ordered world20 response consumption and dedup,
with canonical sign/contact semantics retained and an attended sword test.
Estimate one substantive implementation pass, with a possible separate
qualification pass if those tests expose unresolved behavior. This is not a
promise of broad gameplay support.

## Reviewed runtime contract

The final gate receipts below supplement the chronological checkpoint sections.
All paths in this table are under `build/pass4b-24/` unless stated otherwise.

| Gate | Result / evidence |
| --- | --- |
| Canonical Player reference and repeatability | All ten fixtures × three runs; 2,580 measured transactions, exact semantic snapshots and full phase traces against unchanged `pass4a-02/player-canonical`; `player-canonical/corpus_result.json` |
| Canonical direct Player purity | All 30 runs; 3,756 packets including setup, 7,512 extra helper emissions, 142,728 prepared commands; `player-canonical/purity-analysis.json` |
| Strict canonical phase/contact analysis | All ten fixtures / 30 runs pass; `player-canonical/player_state_result.json`; preserved first-active warm-up, sword registration/history, shield AC-before-AT, combo 1/2/3, sign contact versus response and camera ordering |
| Python tooling and semantic oracles | 149/149; `pass4b-25/python-tests.log`; includes inventory anchors, lean cleanup boundaries, declared repetition coverage, high/mixed purity counts and graceful controls |
| Native temporal/scheduler/motion/camera contracts | 226/226; `temporal-unit/temporal-result.json` |
| Actual engine animation queue/input contracts | 35/35; `animation-input/animation-result.json` |
| Native fixture/CLI validation | 45/45; `native-cli/native-validation.json`, fail-fast, no asset/game initialization |
| Snapshot comparator control | Detects tick 3 one-bit displacement change in a diagnostic copy, then exact pass after copy restoration; `comparator-control/negative-test.json` |
| Final-build high-rate trace/reference | Nine runs: move/turn/slash120, ready-slash60, distant-target60, each three times; 360 world transactions / 1,530 Player intervals; `high-reference/corpus_result.json`, `player_rates.json`, `purity-analysis.json` |
| Source inventory | 2,443 source files, 429 actor entries, zero missing actor sources, one existing unmapped source; regenerated tracked inventory |
| Focused presentation independence | Before policy change: six exact snapshot/Player-stream comparisons at presentation60 (move/slash120 and target60 × three). After stop: one already-completed move/slash120 run at presentation120 matched common-boundary snapshots; `presentation/lean-fps120-comparison.json`. Nine native runs executed; two redundant direct comparisons and the remaining scheduled cases were omitted. `presentation/lean-policy-stop.json` distinguishes the stopped campaign from a failure. |
| Final logging-build checks | One Player120 movement/slash run and one canonical Z-sign run, exact snapshots and direct purity; `pass4b-25/lean-high`, `lean-canonical`. High run is trace-disabled and emits no detailed substep stream. |
| Focused broad regression | Five single runs / 404 canonical transactions: draw-RNG Keese, ocarina note RNG, HUD zero, HUD warning, natural message pages. All exact snapshots; HUD/message direct purity passes. `pass4b-25/lean-original`, `lean-hud-message`, `lean-hud-final`, `hud-zero-reference-comparison.json`, `hud-zero-purity.json`. |
| Graceful native purity control | One Player-only test write detected; normal exit 2, no native fault; `pass4b-25/purity-control/control.json` |
| Ordinary startup | One window and one scene initialization, graceful exit 0; `pass4b-25/startup/smoke.json`. No human gameplay or GPU/pixel acceptance. |

The initial HUD corpus stopped after its successful HUD-zero engine run because
the selected Pass 4A reference did not contain that fixture. This is retained as
a reference-selection infrastructure error. Its existing output was compared
successfully to Pass 3D's exact HUD-zero reference; the engine was not rerun.
The remaining two cases used verified existing Pass 4A references. No new native
crash occurred. The 226/35/45 native helper/CLI results above belong to the prior
runtime-identical full-trace build; their owners were unchanged by the diagnostic-
only rebuild, and they were not repeated. The final Python suite was rerun after
the tooling changes (151 tests at the online-harness handoff). Earlier high-rate QA retains its six accepted continuous/
stepped120 runs; the additional60 cohort was omitted under the new policy.

The nine final-build high-rate runs check every admitted pose, sword warm-up,
retained sweep endpoints, no-motion behavior, target held state and world20 guards.
Direct purity covers 2,121 Player packets including setup, two extra helper calls
each (4,242 extra emissions), with 80,598 prepared rendering commands. No high-rate
Player registration/contact event enters the legacy world stream.

The minimal A–H engine coverage deliberately overlaps fixtures: idle covers A;
ready-sword edge/slash covers B/C; movement/turn/slash covers D/E/G; the static wall
covers F; real distant targeting covers H. At both rates, each has three passing
runs: **30 runs, 1,170 world transactions, 5,265 Player intervals**. Exact receipts
are `pass4b-17/high-idle`, `pass4b-18/{edge-slash,high-60}`,
`pass4b-19/{movement,movement-comparison}`, `pass4b-21/wall` and
`pass4b-23/target-clear`. These are validated milestone executables, not all final-
binary runs; the nine final-build cases above provide focused requalification.
The extra movement20 cohort has three runs / 150 canonical transactions.
The six R-fallback runs in `pass4b-22/fallback-target` are separate mixed-rate
coverage: 120 world transactions, 33 committed high-rate intervals and 108
canonical intervals. Its inadequate target runs are excluded from every accepted
targeting total.

| Owner | Files / principal entry points |
| --- | --- |
| Fixed intervals and QA grants | `PlayerSchedulerCore.hpp`: `FixedPlayerClock`, `PlayerStepControl` |
| Engine integration and scoped events | `PlayerTemporal.cpp/.h`: shared `Sample`, `BeginPresentation`, `AdvanceIntermediate`, `RevokeHigh`, `InvalidateScope` |
| Input | `padmgr.c`, `player_input.h`, `NativeSimulationTestInput.cpp`: port-zero peek/consume and explicit-time replay |
| Action/movement/static collision | `z_player.c`, `player_step.h`, `PlayerMotionCore.hpp`: whole-profile predicate, `Player_AdvanceIntermediate`, ownership-gated `Player_UpdateCommon` |
| Animation | `z_skelanime.c`, `player_animation.h`: private synchronous queue, main/upper Player phase and marker adapter |
| Control camera | `z_camera.c`, `PlayerCameraCore.h`: `PlayerCamera_ProfileRejection`, `PlayerCamera_AdvanceControl` |
| Late pose and presentation | `z_player_lib.c`, `player_pose.h`, `OTRGlobals.cpp`: `Player_AdvanceIntermediatePose`, immutable packet indirection, merged host deadlines |
| Admission hooks | `GameInteractor_Hooks.cpp`; exact inert built-in identities in Mouse, ExtraTraps and RocsFeather |
| Verification | `NativeSimulationTest.cpp/.h`, `z_play.c`, Python temporal/rate analyzers, native queue/temporal tests and fixtures |

At 20 Hz the original transaction remains intact. At q=6N, admitted high-rate
Player work stays in the original Player category slot; the world actor suffix,
global animation drain, camera and canonical late-pose slot retain their order.
The rest of CPU presentation, HUD/message and audio world work completes once.
Intermediate starts are +2/+4 at 60 or +1/+2/+3/+4/+5 at 120. Each consumes input,
runs the bounded action/movement/static collision closure, drains only its Player
animation queue, updates control camera and advances extracted late pose once.
No duplicate step runs at +6. Movement consumes the prior action velocity; input
uses prior committed camera direction; root displacement stays after the queue.

The high-rate gate composes the complete Pass 3D profile with four ordinary
actions (idle, walking, friendly idle, slash), accepted upper actions/equipment,
finite motion/animation domains, default hooks, flat dry static ground and
prospective motion/surface checks. It excludes transitions/pause/freeze, all
scene DynaPoly for camera safety, reciprocal nearby AT/AC/OC partners including
signs, pushes, airborne/ledge/water, spin charge, R/shield input, targeted movement,
unsupported directional attacks, item/projectile/carry profiles and modified
camera/input/movement settings. Friendly targets must be live Kanban and aligned;
there is no damage bridge. NORMAL0 supports NORMAL/STILL/TARGET/FOLLOWTARGET.
Links House fixed-eye NORMAL supports ordinary walking only.

High-rate packets contain the evaluated matrices, selected meshes and admitted
bracelet emission already defined by Pass 3D. They carry Player/scene identity
and world-frame identity. Five packet/command slots are reserved before shared
dispatch, with a 32-KiB remaining-arena margin; the shared packet has its original
slot. All survive only the current synchronous world draw transaction. The helper
reads immutable packet bytes and writes only output/paint storage. Rendering
selects the newest committed list and captured camera matrices; it never obtains
pose from an interpolated matrix. World face/material/shadow opportunities remain
at the original outer draw cadence. No general Actor_Draw purity is claimed.

Review checked original/selected limb semantics, live IK, prior pose use, scoped
scratch, queue restoration, root units, impulse assignments, once-only input,
animation consumers, combo/blur opportunities, high-rate collider suppression,
targetPriority world ownership, world-frame parity and packet lifetime. One
review fix preserves queued input across equipment/profile scope changes; the
surface hook and nonclimbable-wall fixes are recorded below. Normal rendering
does not run authority. The current host loop never omits Player intervals to
meet FPS; an overloaded host slows down. A renderer/performance benchmark and
live-device latency measurement are not claimed. Repeated CPU pose/IK/background
queries and synchronous packet verification are obvious cost centers.
The House Unique7 yaw smoother writes unused mode scratch; the view/control
calculation uses geometric yaw directly. That scratch retains legacy arithmetic
per invocation and is not claimed as a retimed control quantity. Other camera
modes cannot consume it while this walking-only profile remains admitted.

1. Fixed Player/world interval ownership and isolated native boundary tests.
2. Closed-capability engine integration: admitted Player action and input, motion,
   static collision, owned animation queue, control camera and late pose samples.
3. Exact canonical regressions, focused high-rate fixtures, stepping/render
   independence, source review and clean pushed handoff.

Hz20 keeps the existing transaction and arithmetic. High-rate work must preserve
the shared-boundary Player category slot, world suffix, queued animation, camera,
late pose and world draw/audio ordering. Intermediate Player starts are +2/+4
for 60 Hz or +1 through +5 for 120 Hz. There is no additional Player update at
the endpoint before the next world transaction. Each committed interval owns
fresh input, action/motion/static collision, Player queue work, camera and pose.

No general world actor, enemy damage, sign-response bridge, moving geometry,
projectile, HUD/message or audio-mixer retiming is authorized. Runtime 30 remains
deferred. A schedule unit test is not evidence that the engine dependency closure
is implemented or that high-rate gameplay is admitted.

## Resource and validation discipline

Initial read-only preflight found approximately 1.07 GB free on C:. Work paused
at the user's request with no source edits or native launches. On authorized
resume the drive had approximately 19 GB free. No cleanup/deletion was performed.
The existing build tree and hardlinked fixture assets were reused. Failure
evidence remains intact; later successful-output cleanup follows the explicit
lean-policy supersession above. No evidence compression was performed.
Do not restore the historical large matrix. A new native crash stops the campaign
without deliberate reproduction, per AGENTS.md.

## Acceptance still required

All ten exact canonical Player fixtures and strict phases/contacts/purity;
native scheduler/temporal tests; high-rate idle, intermediate B edge, no-target
slash, movement/turn/attack, static wall and held-world-target camera fixtures;
world-opportunity counts; Player-step/world-boundary QA equivalence; selected
render-FPS cases; focused original/HUD, CLI, graceful controls and startup.
The isolated temporal/scheduler gate passes 131 native checks at
`build/pass4b-01/temporal-unit/temporal-result.json`. It covers all three schedules,
1,000 world intervals per rate with exact start/end and step identities, duplicate
begin/commit rejection, closed admission, mid-step request rejection, boundary
revocation, retained fallback and Player/next-world QA grants. No engine gate has
run for Pass 4B yet. No new native crash has occurred.

## Fixed-clock checkpoint

`PlayerSchedulerCore.hpp` adds `FixedPlayerClock` and `PlayerStepControl`, using
the existing temporal types. The clock cannot begin another Player step at a
completed world endpoint. Requests are only accepted outside an open world
interval. The engine adapter must establish whole-profile admission before use;
this header alone enables no gameplay mode.

Admission revocation is accepted only outside an open Player interval. It retains
committed state and withholds remaining intermediate starts until the next shared
world boundary, then latches canonical scheduling until an explicit reset. No
event queue is cleared. The engine integration must report that gap/fallback and
retain valid logical events; it must not hide it as continued 60/120-Hz execution.
This is a conservative loss-of-capability rule, not permission to skip admitted
steps for performance or rendering.

## Animation ownership checkpoint (partial implementation)

`PlayerAnimation_BeginQueue` / `PlayerAnimation_EndQueue` provide a synchronous,
caller-owned queue for intermediate Player intervals. Begin rejects a pending
world queue or nested scope before mutation. End drains only the private entries,
restores queue statics and reports overflow. The ordinary/shared-boundary path
retains the global queue and original drain slot. No root-motion rescaling occurs.

The bound main/upper Link animation adapter retains the canonical expressions.
Its dormant high-rate branch uses authored-frame gain `q / 4` for phase/morph;
actual frame intervals feed the Pass 4A crossing/opportunity contracts. Logical
SFX entries, item changes and lunges have separate consumer identities; merely
asking whether a frame crossed does not consume another event's opportunity.
Animation changes and Player scope/lifetime changes reset this bookkeeping.
No production high-rate context is dispatched yet, so these branches do not
constitute high-rate animation acceptance.

Native queue receipt: `build/pass4b-03/animation-queue/animation-result.json`,
23/23 checks, no game/assets initialized. Executable SHA-256:
`434d8cb1a33ea11e91f4b8d57fd9082eb91ae4527593b3abd2de8ea85702d5e3`.
Python tooling: 139/139 (`python -B -m unittest discover -s
scripts/native-simulation -p 'test_*.py'`), project-local TEMP/TMP.

Retained failure: `build/pass4b-02/animation-queue/animation-result.json` has
exit 0 but no captured stdout. The Windows normal-launch console redirection
hid this new test mode's result; main now preserves redirected streams for it,
as for existing replay mode. This was a test-launch/receipt failure, not a native
crash. Sandbox Git helper failures during build preflight were environment
restrictions; the same baseline build completed outside the sandbox. No gameplay
source workaround or system setting change was used for those failures.

The focused canonical checkpoint runs idle, combo and Z-sign three times each
against the retained `build/pass4a-02/player-canonical` reference. All nine runs
pass exact snapshot/full-trace comparisons in `build/pass4b-03/canonical` with
direct presentation verification and temporal observations enabled. This is a
750-transaction checkpoint. The unchanged per-run strict phase/contact observer
passes all nine runs (`focused-player-analysis.json`); the full ten-fixture gate
is not relaxed. Purity verifies 1,101 Player packets and 2,202 extra emissions,
with 41,838 baseline commands (`purity-analysis.json`). This is a
focused checkpoint, not the required final ten-fixture acceptance. No new native
crash occurred. The remaining seven Player fixtures, broad/HUD corpus, high-rate
fixtures, presentation variants, native CLI campaign, QA stepping and ordinary
startup have not been rerun for this checkpoint. No human gameplay was performed.

## Motion and owned-opportunity checkpoint (partial implementation)

`PlayerMotionCore.hpp` supplies the admitted velocity/displacement continuation
and signed-angle remainder. The canonical path still calls its original helpers.
The dormant high-rate motion path uses `q/4` for stored horizontal velocity,
`q/6` for gravity velocity increments, and the affine constant-gravity embedding
documented in TIMING_SEMANTICS.md for vertical displacement. World OC displacement
is added only at the shared boundary. Root motion remains separate, already
integrated displacement; lunge assignments remain impulses. Terminal clamp
crossings are rejected by the primitive and must be excluded before dispatch.
This does not yet qualify grounded engine motion or coupled action acceleration.

Explicit angle fields retain fractional binary-angle steps and discard residue
on external assignment, action change, or scope/lifetime reset. Linear walking
acceleration/deceleration, authored walking phase and blend increments have
bounded rate adapters; unconverted nonlinear action dependencies remain blockers.

`PeriodicPlayerOpportunity` owns six-quanta opportunities for the signed combo
window, targeting countdown, draw-owned combo extension and blur ingress. Event
resets retain their original sites. The canonical branch returns its original
per-transaction opportunity without changing these new owners. High-rate sword
and shield geometry will not append entries to legacy world collider arrays.
The contact bridge is still inactive.

World-opportunity guards now cover blink/face, Player interface, selected legacy
timers, stick history, target priority, floor audio and sequence work. This is a
partial ownership split, not permission to repeat the complete Player wrapper.
Input service, remaining action/collision ownership, camera/control, intermediate
pose dispatch and host pacing remain unconnected; production stays 20 Hz.

`build/pass4b-06/temporal-unit/temporal-result.json` passes 170 native checks,
including long-run motion endpoints, affine gravity, terminal hold and rejected
crossing, angle wrap/fractional steps, anchored periodic resets and overflow.
Earlier uncommitted motion experiments remain in `pass4b-04` and `pass4b-05`;
the scaled semi-implicit candidate was replaced by the documented affine choice
before engine high-rate admission. These primitive checks are not high-rate
gameplay evidence.

The motion checkpoint build passes 12 exact canonical runs (combo, move-attack,
shield and Z-sign, three repetitions each) against the unchanged Pass 4A reference,
with 1,140 measured transactions. Strict phase/contact observations pass all 12.
Direct purity checks 1,608 Player packets, 3,216 extra emissions and 61,104 baseline
commands. Receipts are in `build/pass4b-06/canonical`: `corpus_result.json`,
`focused-player-analysis.json`, `purity-analysis.json`. Executable SHA-256:
`734a51325b914b039ecabf24f9989dfaf6590d2de7675e906e81867b07de9dd3`.
No new native crash occurred. The remaining canonical fixtures, high-rate engine
cases, broad/HUD regressions, startup and render variants were not run for this
checkpoint. No human gameplay was performed. The prior queue/tooling results are
retained; the changed native motion/opportunity headers have their own new gate.

## Control-camera adapter in progress

`PlayerCamera_AdvanceControl` is an explicit synchronous entry point for NORMAL0
NORMAL/STILL, TARGET/Parallel1 and FOLLOWTARGET/KeepOn1. It samples authoritative
Player/held-target transforms, updates static floor/control state, invokes only
the selected bounded mode and prepares view/control vectors. It does not run the
world camera dispatcher, interface, quake, environment, debug input or cutscene
work. The shared-world camera branch retains one interface opportunity.
Production high-rate dispatch remains disabled; engine qualification is pending.

`PlayerCamera_ProfileRejection` excludes non-main/inactive cameras, other settings,
custom/free-look/debug policy, quake/distortion, pending background-camera changes,
critical health, invalid arithmetic domains, unadmitted friendly targets and all
active DynaPoly geometry. The last restriction is intentionally wider than the
Player foot/contact region because camera eye queries extend farther.

Within this entry point only, fixed-target gains in [0,1] use fractional exponential
continuation; multiplicative LERP growth uses its fractional power. Angle fractions
are owned by stable source call sites and reset on external assignment, snap,
mode/scope/lifetime changes. Legacy +0.5 angular bias is an increment per 50 ms.
Three Normal timers and the selected Parallel/KeepOn transition timer retain
six-quanta duration with fractional phase. The static slope-query scratch is
refreshed completely per Player interval rather than reusing world-frame parity.
Canonical calls keep the original arithmetic and query schedule.

Overshooting gains above 1 have no real fractional exponential. The provisional
high-rate choice uses the local increment times q/6; it is a Class 3 choice, not
an endpoint-equivalence claim. Moving-target, snap and camera collision changes
also require focused engine evidence. No high-rate camera behavior is accepted yet.

The expanded native gate passes 194 checks in `build/pass4b-08/temporal-unit`.
The prior compile-only failure is retained in `pass4b-07/temporal-unit` (test local
shadow/sign warnings under /WX) and `pass4b-07/camera-build/build.log` (incorrect
PlayState field name). Neither was a native crash. No additional fixture campaign
was started merely for the dormant camera adapter.

## Input and action ownership integration in progress

`PadMgr_PollPlayer` / `PadMgr_GetPlayerSample` provide a bounded port-zero
acquisition/peek/consume path. Replay uses the explicit Player sampling time;
the ordinary provider retains its original timestamp and site. Intermediate
sampling does not execute retrace callbacks, rumble output/control, mouse update
or controller-query timers. The consumer admits only B/Z/R with no right-stick
or gyro input; rejection leaves accumulated input and the destination unchanged.
Menu/other-port edges remain available to their world owner. Production dispatch
is still closed, so this is not evidence of intermediate attack entry yet.

`InputTimeline` can consume one port without removing other ports' due events.
Scope rebinding preserves pending logical input for the same live scene/Player,
including sequence, timestamp, availability and button data. It changes delivery
scope only; scene/player replacement is rejected and still requires a reset.
The scheduler must use this operation at capability fallback; it is not wired
to the old canonical observer's resets yet.

Head/focus/upper-body angles now have explicit field owners plus two named
scratch slots; no stack pointer is retained. Proportional high-rate smoothing
uses exponential gain with elapsed-time minimum/maximum caps and fractional
binary-angle accumulation. Canonical helpers keep their original expressions.
Clamp transitions and integer quantization remain declared Class 3 choices to
qualify with engine fixtures. Animation marker conversion rejects nonfinite or
out-of-domain authored phases before integer conversion.

Idle-animation RNG selection, world interaction-offer resets, damage/void
response, floor/ledge duration counters, the post-scene-collision hook and
collider cleanup retain world opportunities. Cylinder geometry may update at
Player cadence, but high-rate cylinder/sword/shield registration is withheld
from the legacy collider arrays. Static floor/wall queries still run per Player
interval; the eventual whole-profile gate must exclude exits, hazardous floors,
climbs and reciprocal contacts before dispatch.

The latest isolated temporal gate passes 220 checks in
`build/pass4b-10/temporal-unit`; 139 Python tooling tests pass. The engine queue/input
gate passes 35 checks in `build/pass4b-10/animation-input`. Executable SHA-256:
`a2e487c1e052918248c3a8d5124384591ae6398e1e274de559d9b06dacd30583`.

The focused canonical checkpoint passes idle, turn-attack and Z-sign three times
each against the unchanged Pass 4A reference: nine runs, 690 measured transactions.
Strict phase/contact analysis passes all nine; direct purity checks 1,041 Player
packets, 2,082 extra emissions and 39,558 baseline commands. Receipts are in
`build/pass4b-11/canonical`. The initial `pass4b-10/canonical` attempt stopped at
the known sandbox Git-helper provenance failure before launching the game; the
same command outside the sandbox used a fresh output directory. No source fix
or system setting change was made for that environment failure.

The remaining seven canonical cases, high-rate cases, broad/HUD corpus, render
variants, QA stepping and ordinary startup were not rerun for this partial
checkpoint. No human gameplay was performed.
`build/pass4b-09/engine-build/build.log` retains a compile failure from a missing
replay helper ownership parameter, corrected before runtime testing. No native
crash occurred. High-rate host dispatch, complete admission, late-pose/render
publication and engine acceptance remain unfinished.

## Scheduler integration under qualification

The internal fixture selector now connects the fixed clock to the original
shared Player slot and to intermediate Player intervals. The intermediate
closure uses port-zero input, the ownership-gated common update, a private
animation queue, bounded camera control, and explicit extracted late pose.
Five packet/command slots are reserved in the current world draw arena before
dispatch; rendering selects the latest committed packet without re-evaluating
authority. No packet is retained across world draw transactions.

This implementation is **not accepted yet**. `build/pass4b-12/high-idle` through
`build/pass4b-16/high-idle` retain graceful exit-2 admission failures, not native
crashes. The runner's generic `infrastructure-error` label does not change that
classification. After correcting the camera guard, `build/pass4b-17/high-idle`
passes all three repetitions: 360 measured authoritative Player intervals over
60 world transactions, with exact repetition receipts. Direct purity covers 477
Player packets including setup, 954 additional emissions, and 18,126 baseline
commands. Executable SHA-256:
`b7000b35324e1d64ffd5381b8284d996e6561680fd602f6265f4ad882fc641b9`.

The first no-target slash attempt in `build/pass4b-17/edge-slash` gracefully
rejected a nearby OC collider after 43 committed high-rate intervals. It also
showed that the initially sheathed weapon first consumes B for draw preparation,
so it is not a valid immediate attack-entry latency fixture. The revised fixture
prepares the sword through ordinary B input during canonical setup and moves
farther from world colliders. No contact gate was relaxed.

The revised ready-sword case passes three repetitions at each rate in
`build/pass4b-18/edge-slash` (120 Hz) and `build/pass4b-18/high-60` (60 Hz, also
three idle repetitions). B arrives at quantum 7; attack begins at quantum 7
at 120 Hz and quantum 8 at 60 Hz, both before the next world boundary at 12.
Each run records one consumed B edge and 210/105 actual Player intervals.
Together with the earlier idle120 result, these are 12 successful runs, 330 world
transactions and 1,485 measured Player intervals. Purity covers 2,193 Player
packets including setup, 4,386 extra emissions and 83,334 baseline commands.
These are isolated no-contact cases, not the complete A-H acceptance set.
The ready-sword/60-Hz executable SHA-256 is
`10665035dbf84dd18839d759698d49e862eba15a7d3df30646ce193dda86b44c`;
its build receipt is retained in `build/pass4b-18/engine-build`.

`build/pass4b-18/canonical` passes idle, slash and Z-sign three times each against
the unchanged `build/pass4a-02/player-canonical` snapshots and full traces. These
are nine focused checkpoint runs, not the final ten-fixture acceptance campaign.
All nine also pass the strict per-run phase/contact analyzer, recorded explicitly
as `player_state_subset.json`. Their 630 measured transactions cover 981 Player
packets including setup, 1,962 extra emissions and 37,278 baseline commands.

The hook audit distinguishes exact built-in callbacks from third-party hooks:
disabled mouse quickspin, inactive ExtraTraps item dispatch, and the registered
Roc's Feather item hook whose item is excluded by the complete profile. Other
callbacks still reject. Camera admission initially inspected `manualCamera`
outside its optional free-look owner; ordinary Play initialization does not
initialize that scratch field. The correction rejects free-look itself and
does not read its unused scratch or change the canonical camera path.

The temporal unit gate passes 226 checks in `build/pass4b-12/temporal-unit`.
The tooling suite passes 140 tests. New receipt checks require every intermediate
interval, stable world identity within it, and exactly one world-frame increment
per six quanta. A B-edge receipt additionally requires attack entry on the next
eligible intermediate boundary with one consumed edge. These checks are tooling
evidence separate from the focused engine results above.

## Player QA and representative world guards

`build/pass4b-19/continuous` and `build/pass4b-19/stepped` each pass three
120-Hz ready-sword slash runs. Stepped execution matches the continuous
snapshots, temporal endpoint stream and complete Player-interval stream exactly.
Each stepped run grants 108 single Player intervals and 17 next-world commands,
with 125 verified holds, three extended holds and 124 explicit successive-boundary
count comparisons. The final grant is verified by the completed native receipt.

The QA protocol accepts `step_player`, `next_world`, `pause` and `run` at a
sequence-bound `(tick, player_offset_q)` boundary. The existing canonical protocol
is unchanged. A shared-boundary Player grant includes the one due original world
transaction; intermediate grants execute no world transaction. `next_world`
finishes the current interval and stops before the next world transaction. The
automated test issues next-world commands at shared boundaries, proving exactly
one world opportunity for each such command. Inspections include live Player,
pose/contact history, animation, camera and temporal metadata. Held state is
compared before/after the wait; no restore masks a change.

Per-step world guards require exactly one actor traversal, collision boundary,
blink opportunity, script boundary, environment update, HUD boundary, message
boundary and audio boundary in the current world interval. Every intermediate
step checks that those counts remain one. The validator's graceful controls
reject a multiplied blink count or a skipped/misaddressed Player grant.
Executable SHA-256:
`c5238344a82ff627fdda52066572f53ad26325ddf3b5af6df687617f0d87f108`.

Remaining high-rate fixtures, full canonical regression, render independence,
negative admission/fallback and final source review remain outstanding.

## Movement/control evidence in progress

The combined movement/turn/slash fixture passes three repetitions at each of
20, 60 and 120 Hz (`build/pass4b-19/movement` and `movement-comparison`). Its
intermediate control edges at quanta 7, 67 and 211 change action/yaw on the next
Player boundary. The moving B edge at 91 begins attack at 91/92 for 120/60 Hz.
The read-only `analyze_player_rates.py` additionally checks actual engine
intervals: 200/78 horizontal motion intervals and 131/62 authored animation
intervals per 120/60-Hz run match the declared float32 equations exactly.
Root displacement and wall correction are explicitly outside the free-motion
equation; they are not hidden by a large tolerance. Camera direction also changes
on intermediate boundaries, and pose generation advances once per interval.

At common endpoint q=66, all three rates reach stored speed
`0.9946768879890442`. Displacements from the initial position are
10.4438943 (20 Hz), 13.2638618 (60 Hz), and 13.9265530 (120 Hz).
This input-start transient is not a constant-velocity semigroup test: the edge
is available at q=12/8/7, the action enters with zero velocity, and movement uses
the preceding action's velocity. Higher rates resolve these startup opportunities
earlier. The per-interval velocity map is exact; differing response resolution
is Class 2. Camera heading/clamp choices retain their declared Class 3 limits.
No claim is made that this moving/colliding trajectory must be bit-identical
between rates. Python tooling now passes 142 tests, including graceful
displacement, animation-phase and duplicate-pose receipt controls.

## Static wall checkpoint

`build/pass4b-21/wall` passes static-wall-60 and static-wall-120 three times
each: 330 world transactions and 1,485 measured Player intervals. Per run,
88/222 intermediate 60/120-Hz intervals observe wall contact. The ordinary
Links House fixed-eye NORMAL camera uses a bounded Unique7 adapter; B/Z reject
before dispatch because their camera transitions are outside this closure.
The prospective wall check accepts flag 0, which disables ledge climbing,
while rejecting ladder/climb/crawl/grab flags and dynamic surfaces. Unknown
`VB_SURFACE_IS_CLIMBABLE` hooks reject before the query. The earlier
`build/pass4b-20/wall` exit-2 rejection is retained, not a native crash.
Executable SHA-256:
`be3a26caa61137ed3815ac6044d7e0f74f014fb5145ade1eefc78f9e5d3d2e02`.
Build receipt: `build/pass4b-21/engine-build`. Python: 142 tests pass.
This is still a partial checkpoint: targeting, fallback, render independence,
final canonical/broad regression and source review remain required.

## Targeting and fallback checkpoint

`build/pass4b-22/fallback-target` validates unsupported R input at quantum 7 at
both rates, three repetitions each. Rejection is before the next Player update
(q=7/8), retains the pending edge and committed state, withholds remaining starts,
then consumes R once at world q=12. The canonical suffix remains latched at 20.
Equipment/profile scope changes now rebind same-Player logical input; actual
scene/Player replacement still clears it. Python negative controls reject dropped
edges, duplicated delivery and incorrect partial-interval counts.

The target runs in that first corpus did not acquire a target and are **not**
targeting acceptance despite their old runner completion receipt. An explicit
acquisition assertion in `pass4b-23/target` diagnosed static line obstruction
at (-177.11, -43.94, 1106.42); it exited normally with code 2. The clearing-position
fixture passes six runs in `build/pass4b-23/target-clear`, including exact traces,
purity, next-Player Z acquisition and held-target samples. A real Kanban is spawned
450 units away with fixture-only engine attention range 4 (700 units); default
sign range 70 conflicts with the conservative 200-unit unbridged-contact exclusion.
The fixture sets no lock-on/camera result and performs no target damage. Z at q=19
acquires through the engine at q=19/20 for Player120/60. Friendly FOLLOWTARGET
control advances between world updates; target position/focus remain held.
Source review covers the synchronous packet arena, private animation queue,
world-gated target priority, scoped input resets, static wall flags and the missing
surface hook guard. Full final compatibility/render gates are still pending.

## Next boundary

Pass 4C is reserved for generation-bound authored contact opportunities and
world20 target-response integration plus interactive target-contact qualification.
Do not begin it as part of this pass.
