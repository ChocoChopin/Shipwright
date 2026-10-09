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

## Next boundary

Pass 4C is reserved for generation-bound authored contact opportunities and
world20 target-response integration plus interactive target-contact qualification.
Do not begin it as part of this pass.
