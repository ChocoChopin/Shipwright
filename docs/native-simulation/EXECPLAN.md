# Execution plan: authoritative fixed simulation rates

This plan is for a Shipwright engine modification preserving history and canonical
20-Hz play. It proposes one architecture for 20/30/60/120 Hz. Higher-frequency
render interpolation is not completion. Pass 2 implements a bounded canonical
replay harness and observational seams; no gameplay timing conversion has occurred.
BASELINE.md records the original build, and TESTING.md/PASS2.md distinguish current
runtime evidence from unresolved reliability and future acceptance gates.

## Read first and decision authority

Read AGENTS.md, ARCHITECTURE.md, TIMING_SEMANTICS.md, TESTING.md, CONVERSION_LEDGER.md,
and KNOWN_DIVERGENCES.md. Source anchors refer to baseline
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`; re-resolve after upstream merges.
Record each claimed subsystem, prerequisite closure, model choice, tests, evidence,
and commit. Keep unrelated work moving when a Class 3 decision is deferred.

Each phase may use several reviewable commits. The phase boundaries are dependency
gates, not estimates of effort or authorization to skip validation. Ultra/High/
Medium below recommend reasoning effort for later agents; they are not measured
model guarantees. Shared primitives and complex semantics merit review by an
independent agent; mechanical actors should consume the established contracts.

## Phase 0: reproducible baseline and safe local infrastructure

**Prerequisites:** legal compatible ROM locally, supported Windows C++ toolchain,
GitHub or public Git read access. Pass 1 established this phase and architecture.

**Files:** upstream BUILDING/MODDING/supportedHashes, pinned submodules, ignore
rules, AGENTS.md, `scripts/native-simulation/`, BASELINE.md and evidence under build/.

**Work:** preserve upstream develop identity; fork and remotes; build untouched
engine source; generate soh.o2r and extract oot.o2r with current Torch; launch if
possible; record exact compiler/SDK/dependencies and assets. Separate initial build,
process-start and interactive evidence. Establish inventory and pure math oracle.

**Tests/gate:** native build and extraction exit 0 or precise external blocker;
supported SHA1 and extraction-config entry; ignored assets; clean staged source
audit; oracle green; inventory reproducible. A blocker is not a passing game gate.

**Failure modes:** vcpkg/dependency drift, VS BuildTools omitted by default vswhere,
Python selection, global temp permissions, stale asset packages, false launch claim.
Classify failures before source edits. **Effort:** High for setup diagnosis; Ultra
for this first architecture and cross-subsystem investigation.

## Phase 1: canonical observational replay, clocks, and state schema

**Prerequisites:** Phase 0 baseline compiles; usable assets; documented portable
working directory and config. Complete any Phase 0 blocker before engine changes.

**Files:** `OTRGlobals.cpp`, `src/code/graph.c`, `src/code/padmgr.c`, libultraship
controller seam, `z_play.c`, `code_800FD970.c`, GameInteractor hooks, new test module
under `soh/soh/` and local runner under scripts. Inspect submodule seams before
deciding whether a submodule change is actually necessary.

**Work:** add opt-in canonical-only fixture boot, timestamped pad injection,
controlled clocks/seeds, non-mutating semantic snapshots and first-divergence
traces. Declare integer clock/rate types with selected rate fixed at 20. Record
draw side effects, audio/RNG calls and thread ownership. Keep normal play inert
when diagnostics disabled. Use separate process per fixture, not heap savestate.

**Tests/gate:** same fixture replays bit-identically at 20 on the same binary at
least three times; canonical traces match a reference20 build carrying the same
minimal input/clock/seed/observation seams (not the later timing refactor) for movement,
pause/scene reset, sword/shield, dialogue, seeded enemy and audio/ocarina fixtures.
Fixture failures exit nonzero, report first causal record and preserve logs.
Wall-time and physical-pad changes must not affect injected runs. Snapshot schema
includes source/config/ROM IDs and excludes unstable pointers/padding.

**Failure modes:** missing statics/seed domains; draw RNG skipped; async resource
ordering; host audio occupancy enters gameplay; instrumentation changes RNG or
order; remote control mistaken for replay. **Effort:** Ultra for initial harness
design and RNG/audio boundary; High for scoped implementation and fixture work.

**Pass 2 status:** bounded implementation and validation are complete. The opt-in
engine runner, fresh-process boot, input timeline, snapshots, strict comparator
and trace instrumentation pass the 12-fixture, 36-run canonical corpus. The
21-run presentation/trace matrix, negative control, coupling analysis,
diagnostic-change comparison and ordinary-startup smoke also pass. The Python
suite passes 60 tests (29 runner, 22 math and 9 acceptance checks), and the
executable passes all 36 native CLI checks. Corpus 02 remains failed: it completed
35 of 36 runs and retained an unresolved startup AV
before measurement. Corpus 03 was intentionally interrupted for a diagnostic
capacity rebuild, not accepted as a completed corpus. See TESTING.md/PASS2.md for
the final corpus 04 and related receipts. This is not completion of every pause,
scene-reset, dialogue, combat or audio scenario required by the broader phase.

## Phase 2: preserve canonical behavior while separating cadence side effects

**Prerequisites:** Phase 1 exact 20-Hz oracle is trusted, including draw/contact data,
and startup reliability is strong enough to distinguish an extraction regression
from the unresolved intermittent failure. A single clean rerun does not close that
reliability question.

**Files:** `z_play.c`, `z_actor.c`, `z_player_lib.c`, `z_parameter.c`, `z_message_PAL.c`,
actor limb/draw callbacks, `OTRGlobals.cpp`, frame_interpolation, audio code and
libultraship presentation/control interfaces if required.

**Work:** identify and move authoritative pose/collider/culling/attachment/timer/RNG
work to explicit phases without changing canonical order. Inventory globally but
extract first for the complete dependency closure of admitted pilot fixtures;
remaining actor-specific draw extraction continues with later conversion batches.
No small fixture can establish global draw purity. Preserve prior-frame vs
current-frame contact ownership until fixtures justify a change. Retain legacy
display-list generation once per canonical transaction during transition. Define
snapshot history and interpolate presentation only. Introduce independent host
pacing, input event queue, audio sample/block budget, and pause/UI/world clock
ownership. No high-rate full world yet. Audio must advance correct real-time
sample counts when render stalls or simulation advances several steps.

**Tests/gate:** all Phase 1 canonical traces exact; presenting at 20/30/60/120 and
uncapped, or omitting GPU submission with equivalent CPU work, leaves 20-Hz world
hash/event stream unchanged within explicitly audited fixture coverage.
Sword/shield/limb attachments, culling/offscreen
actors, interface timers, dialogue, audio and ocarina verified. Event pumping must
still work during catch-up; overload never drops authoritative steps silently.

**Failure modes:** a render callback still mutates the world; one-step collider
latency moved accidentally; RNG stream reordered; duplicate audio effects;
display-list lifetime invalidation; render interpolation becomes authority.
**Effort:** Ultra for design/order review, High for bounded extractions. This extra
phase is necessary; the brief's proposed order would decouple rendering too early.

## Phase 3: rate-parametric primitives in isolated fixtures

**Prerequisites:** Phases 1-2; exact legacy branches; approved unit annotations.

**Files:** new temporal context/helpers, `z_lib.c`, `z_actor.c` movement helpers,
`z_skelanime.c`, collision helper boundaries, timer/event helpers, new unit tests.

**Work:** validated four-rate enum and 120-unit clock; rational duration/phase,
linear rate, fractional angle, exponential/affine map, marker crossing, reset
ownership. Separate impulses/displacements from rates. Use compile-time/runtime
capability gating so unconverted world logic cannot run at a selected high rate.
Use restart-required selection initially. No setting-only global frequency switch.

**Tests/gate:** native helper tests against the reference oracle at all four rates;
exact old expressions for 20; long-run no drift; 30-Hz duration jitter bounded;
constant-force and decay composition; branch/clamp/signed/reverse/loop edge cases;
simulated rate-switch requests rejected except admitted reset. C/C++ results, not
only Python tests, are required. Every helper documents who scales it.

**Failure modes:** double scaling; scalar fixed dt leaks into wall clocks; integer
truncation; naive smoothing/probability conversion; mismatched animation events;
legacy non-20 state broken. **Effort:** High; Ultra for integration/clamped-map
choices; Medium only for fully specified unit/edge-case test additions.

## Phase 4: a complete player interaction pilot

**Prerequisites:** Phase 3 helpers; Phase 2 pose/contact separation; fixture scene
has a bounded, enumerated dependency closure including required actors/effects.

**Files:** Player `z_player.c`, `z_player_lib.c`, `z_camera.c`, targeting in z_actor,
background/collision helpers, a minimal static scene, required projectile/effect
modules and CVar UI entry with experimental admission guard.

**Work:** true high-rate held input and edges, walking/running/turning/jump/fall,
camera/aim/lock-on, floor/wall/ceiling, target/combat/knockback/invulnerability,
animation/root motion. Keep scene selection constrained; never imply entire game
support from an empty room. Respect existing enhancement/config multipliers.

**Tests/gate:** all rates run recorded input; compare common 100-ms boundaries and
event traces. Distance/speed/airtime/turning/animation/hit windows satisfy declared
invariants; thin-wall/projectile/edge input tests; canonical exact; rendering-rate
matrix independent. Every difference classified, with no unexplained Class 1.

**Failure modes:** cross-module collision ordering; sword hit lifetime; input edges
replayed each render; animation pose lags; camera still per-update tuned; pilot
uses hidden unconverted actors. **Effort:** Ultra for initial Player/collision
dependency analysis, High for small state families and test-driven changes.

## Phase 5: world lifecycle and reusable actor contracts

**Prerequisites:** pilot validated; clock/pose/contact APIs stable.

**Files:** actor traversal/spawn/destruction, DynaPoly and moving platforms, scene
transitions, effects, environment, audio dispatch, save/load diagnostics and pause.

**Work:** per-actor timing state lifecycle and stable IDs, deterministic scheduling
and decision opportunities, platform/rider intervals, effect lifetimes, environment
phase, timer ownership, reset and scene barriers. Migration of saved runtime timing
state only if needed; ordinary game saves are not full snapshots. Keep the
canonical RNG stream; review each new stream proposal explicitly.

**Tests/gate:** spawn/destroy/reuse/scene/pause/resume/reset corpus; platform rider
relative motion and collision; offscreen/culling; same-rate hashes; leaked/stale
timing state detected; audio event counts invariant; admission graph auditable.

**Failure modes:** actor pointer reuse corrupts sidecar; uninitialized timer on
spawn; stale interpolation/collider on scene swap; save mutates RNG/order; category
order changes. **Effort:** High; Ultra for RNG/lifecycle boundary decisions.

## Phase 6: systematic actor conversion by dependency family

**Prerequisites:** Phase 5 contracts; ledger claims and fixtures assigned.

**Files:** `soh/src/overlays/actors/`, effects, enhancement hooks touching each family;
CONVERSION_LEDGER and machine inventory. Work small independent actor batches.

**Work order:** props/simple mechanisms -> pickups/doors/platforms -> simple NPCs
and enemies -> projectile/explosion families -> interdependent actors. Each batch
records A-I classes, math model, dependency closure, changed direct increments,
draw side effects, timers and decision/RNG cadence. Scanner hits guide review;
zero hits do not prove absence of timing semantics.

**Tests/gate per batch:** canonical exact replay; all four rate scenarios including
spawn/despawn, hits, pause, failure and reset; relevant invariants and classified
divergences; no untouched dependencies admitted to high-rate scenes. Ledger owner,
status, evidence and commit complete; review can reproduce without chat history.

**Failure modes:** regex conversion misses aliases/callbacks; mirrored helper and
caller scaling; AI probabilities multiply; actor handles crossing tables stale;
offscreen effects absent from tests. **Effort:** Medium for repetitive actors with
settled templates, High when a new semantic pattern appears, Ultra only for a
cross-family architectural ambiguity. Stop expanding a batch when scope changes.

## Phase 7: coupled gameplay systems and semantic decisions

**Prerequisites:** required actors and core phases complete; deterministic traces
cover coupled dependencies.

**Files:** bosses, Epona, swimming/climbing/ledges/hookshot, bombs, minigames,
cutscene/camera scripts, dialogue/ocarina/audio RNG, environment/day/night,
save/transition enhancements and randomizer interactions.

**Work:** complete authored event timelines, integer exact-frame tests, reverse/
loop animation, scripted camera and actor cues, puzzle/AI/RNG and boss phase
boundaries, horse/rider and projectile chains. Resolve selected Class 3 entries
with evidence; retain intentional Class 2 differences. Extend capability gate
only after each whole scene's dependency closure is validated.

**Tests/gate:** boss full phase/death/reset, minigame win/loss/time limit, ocarina
memory game, Epona mount/dismount/jump, swim/surface/dive, climb/ledge/drop,
hookshot attachment/retraction, bomb fuse/explosion chain, door/scene/cutscene skip,
day/night transition and normal save/reload matrix. Canonical exact; all four
invariants and event traces reviewed; semantic decisions explicitly documented.

**Failure modes:** hidden gameplay dependence on audio RNG or frame identity,
balancing changed under smoother math, scripts skip exact matches, timer freezes
in wrong domain, enhancers still assume one20-Hz frame. **Effort:** High by default;
Ultra for coupled RNG/script semantics or disputed balance choices.

## Phase 8: divergence audit, full corpus, and experiential acceptance

**Prerequisites:** all admitted subsystems converted; no unresolved critical Class1;
fixtures provide meaningful coverage rather than only scanner completion.

**Files:** all touched systems, documents, ledger, fixtures/runner, metrics and UI.

**Work:** independent audit of uncovered patterns, helper ownership and scope;
cross-rate/render/config matrices; long-run drift, reset/replay and performance;
maintain upstream merge discipline and source identity. Profile cost at 120 Hz,
including collision and CPU pose work, without reducing authoritative step count.
Human playtest is separate evidence for feel, readability, balance and edge cases.

**Tests/gate:** regression corpus repeatable from clean local checkout; 20 exact
under declared deterministic envelope; duration/movement/animation invariants at
all rates; classified and reviewed Class2/3 corpus; overload behavior intentional;
120-Hz performance targets documented with hardware and P95/P99 tick times;
human acceptance receipt before claiming experiential parity. Release/publication
is separately authorized; no phase result implies it.

**Failure modes:** overfitting epsilon, suppressing resolution differences by
coarsening gameplay, corpus lacks rare branches, slow machines silently lose time,
fork drift or hidden submodule changes. **Effort:** Ultra for final semantic audit,
High for failures, Medium for established corpus maintenance.

## Exact next pass

Pass 2 completes its bounded Phase 1 implementation and validation with
fresh-process debug-save fixtures, rational input events, exact step limits,
controlled RNG/audio and explicit semantic state.
The current test envelope is narrower than the full Phase 1 corpus listed above:
pause/scene transitions, NPC dialogue, complete ocarina play and full seeded combat still
need fixture coverage. TESTING.md owns measured completion, not this plan.

The next pass must first establish a stronger reliability baseline and preserve
the exact admitted canonical executable as its reference, including the known
startup failure record. Once that gate is met, begin a **bounded Phase 2 extraction
of HUD/message draw-owned state** for the countdown and notice fixtures, adding
any fixture needed by their dependency closure before moving code. Leave actor
pose/collision, enemy draw RNG, and audio scheduling at their existing seams
during that first extraction. Require exact
20-Hz snapshots and event order plus presentation-rate checks before and after
moving any work. Use Ultra reasoning for ordering/ownership design and independent
review, High for scoped implementation. Keep 30/60/120 gameplay unavailable;
rate-parametric physics and bulk actor conversion remain later phases.

If one seed seam cannot be controlled yet, report the exact first differing field
and stream; improve the fixture/instrumentation rather than starting rate conversion.
