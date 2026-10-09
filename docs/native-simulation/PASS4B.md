# Pass 4B: gated fixed Player scheduler and dependency integration

Status: in progress; no high-rate acceptance or gameplay support claimed.
Starting source: `6153eb451a37dfc6b3bfa6f6edebece85d9e08e8`, clean branch
`mod/native-simulation-rates`, origin `ChocoChopin/Shipwright`. Verified dependency
pins: libultraship `9280b17ddc504da6630892a46440e86be41ac571`, torch
`2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. Both submodule worktrees are clean.

## Scope and implementation sequence

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
Use the existing build tree and hardlinked fixture assets; compress successful
JSONL evidence with hash verification and retain all reference/failure evidence.
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

QA Player stepping, remaining high-rate fixtures, full canonical regression,
render independence and final source review remain outstanding.

## Next boundary

Pass 4C is reserved for generation-bound authored contact opportunities and
world20 target-response integration plus interactive target-contact qualification.
Do not begin it as part of this pass.
