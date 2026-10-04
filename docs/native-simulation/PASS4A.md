# Pass 4A: Player temporal core and pre-pilot controls

Status: implementation and focused acceptance complete, 2026-10-04. Starting source
`f8fe6a7fd005a508b2626de2685e8df41efafa59`; dependency pins unchanged from PASS3D.

The authorized scope is native temporal vocabulary, opportunity and lifecycle
contracts, bounded Player unit inventory, canonical pause/single-step and diagnostic
inspection. PASS3C/PASS3D own the phase order and admitted profile. No gameplay
arithmetic, cadence, animation, camera or collision is retimed. Effective Player
and world rates remain 20 Hz. Contact records are schema only, with no active bridge.

Implementation uses standalone native tests of the same header used by the
engine. Canonical metadata stays in a separate diagnostic stream so the preserved
semantic snapshots/full phase traces remain exact oracles. QA gates the complete
transaction before input and retains the last presented image while paused; it
does not use the legacy collision-suppressing frame advance.

Validation covers all ten Player fixtures with three repetitions, strict
phase/contact analysis and CPU purity, focused original/HUD regressions, native
core and Python tests, CLI/controls/startup, canonical single-step equivalence and
representative presentation independence. The final binary has focused revalidation;
the broader reviewed-build results have explicit source reuse, detailed below.
No historical large matrix was restored.

Next boundary, after a clean pushed handoff: separately authorized Player cadence,
queue ownership and input/camera/contact integration. That work does not start here.

## Native foundation checkpoint

`PlayerTemporalCore.hpp` implements distinct rate/time/duration/context types,
canonical-only capability, per-source opportunity cursors, integer duration and
signed sixth-remainder accounting, unwrapped authored-frame interval crossings,
animation marker consumption, scene/Player/scope generations, attack/window
identity, ordered input metadata and pointer-free future contact records.
No engine gameplay call uses a scaling primitive. `CanonicalControl` gates whole
transactions and refuses duplicate grants/commits. World guards require six-quanta
aligned intervals and a distinct source opportunity.

The standalone MSVC gate passed all 95 checks at
`build/pass4a-01/temporal-unit-02`. The first receipt incorrectly expected 96
despite all 95 checks passing; it remains retained as test-infrastructure failure.
No native fault occurred. Engine integration was pending at that historical
checkpoint; the final acceptance below supersedes that status.

## Engine integration and ownership

`PlayerTemporal.h/.cpp` is an observational adapter, active in ordinary execution
as well as replay. The existing scene/actor/frame observation seams now call it
before their test-enabled early return. `Play_Main` observes states that can skip
draw; Player action setup, attack setup, melee-state assignment and main Link
animation changes label their original opportunities. No sidecar value is read
back into a legacy gameplay expression. `PlayerStepContext` intervals share the
world availability timeline; `player_time_q` separately counts actual canonical
Player updates. World time advances only on the canonical collision/update
transaction, not menu/file-select time. Neither replaces `gameplayFrames`.

| Domain | Owner and rule |
|---|---|
| Player elapsed time | Six quanta per actual canonical Player update; process-monotonic, separate from world elapsed time when Player is skipped |
| World elapsed time | Six quanta at the existing collision/update opportunity; scene-bound per-source guard rejects repeated/unaligned opportunities |
| Authored animation | Signed unwrapped Q16 frame intervals in the isolated helper; live float animation math is unchanged |
| Player timers/events | Their action or authored source owns reset and eligibility; no timers converted |
| World decisions | AI, scripts, blink RNG, unrelated timers/effects/environment remain original world opportunities |
| HUD/messages | Existing late canonical authority slots; genuine game pause/menu behavior is unchanged |
| Audio | Original logical-event order and mixer wake; no new scheduling |
| Presentation | Existing replay FPS; temporal state cannot advance from helper or display-list replay |

The capability represents a requested 20/60/120 rate but returns effective 20,
world 20, and high-rate admission false through non-writable accessors. There is
no public rate setting. Runtime 30 remains invalid. The world guard diagnoses
duplicate canonical opportunities; it does not suppress or reorder old world
work. A later scheduler must guard each named source before dispatch.

## Units and primitive contracts

`player-temporal-units.json` records 34 bounded groups with fields, source anchors,
units, owner, reset, future obligation and fixtures. Its test checks categories,
unique IDs and live source anchors; this is not an AST proof of every branch.
Root motion and collision correction are already-integrated displacements.
Launch/lunge assignments are impulses. Stored angles, enums and combo ordinals
are not rates. Movement/gravity require one future continuation owner.

`unk_845`, action variables, targeting gates, invincibility,
hoverBootsTimer, smoothers and camera quantities remain mixed or branch-sensitive.
The inventory does not authorize bulk conversion of those groups. In particular,
grounded ordinary boots can carry hoverBootsTimer=19; nonzero is not admission.
Second review corrected `unk_844` from a plain duration: B release negates it and
positive 1 is an action opportunity, so its sign/latch must be split from remaining
duration. `meleeWeaponState` is encoded ordinal/window state derived from authored
phase, not a frame quantity to scale. These are inventory corrections only.
Root motion must not be scaled twice. No gameplay field uses a new scale here.

| Primitive | Units / owner / lifetime / canonical rule |
|---|---|
| SimTime, SimDuration, Add | Unsigned 120-Hz quanta; checked overflow; process timeline or explicitly owned duration; no sentinel folding |
| StepQuanta, CommonBoundary | 20/60/120 -> 6/2/1; world/common boundary six; invalid rate has no step |
| Countdown | Source-clock quanta; owning action resets; expiry fires once, disabled separate; old counters untouched |
| RateRemainder | Signed legacy integer increment to integral output plus retained sixths; one owner resets on lifetime/rate change; original Hz20 expression retained |
| WrapAngle | Unsigned binary-angle wrap of a signed integral increment; no hidden scale or reset |
| Crossings | Signed unwrapped authored Q16 frames; exclude origin, include destination, forward/reverse/multiple loops; owner supplies loop extent |
| OpportunityCursor | Per source/domain/generation monotonic index; consume once when due; reset with owner |

The remainder numerator 4 is cheap s=2/3 adversarial math only. Floating ballistic,
exponential and affine continuation is deliberately not added to native production:
the bounded motion model still needs clamp/collision decisions. Existing Python
oracles remain candidate math, not gameplay conversion.

## Opportunity and reset contracts

Each authored attack increments an epoch at `func_80837948`, independently of
render frames and pointer addresses. The off-to-positive active window within
that attack increments `hitOpportunity`. Sustained active state is one opportunity;
an explicit inactive/new-active window may create another for a future multi-hit
profile. Negative priming does not create a hit window. Action changes end the
active attack; counters retain their historical identity. Canonical sword history
still resets only where legacy code reset it.

Animation changes advance a generation; observed Player updates advance interval
identity. Isolated `AnimationEvents` consumes each named marker at most once per
interval, with distinct marker slots and full crossing ranges. It is bookkeeping,
not a replacement for canonical Link animation or existing float marker tests.
Future sound/action events use (generation, interval, marker, loop) identity.

Input acquisition and availability are separate. Live acquisition uses rational
performance-counter timestamps at the original PadMgr polling site; replay uses
its authored rational timestamps through the same `InputSample` interface. Ordered
press/release/held samples enter a bounded queue that rejects overflow rather than
overwriting unseen edges. Original consuming PadMgr requests mark eligibility;
the next actual Player update labels delivery with scene/Player/scope and step ID.
`ConsumeForPlayer` rejects stale owners, backward step IDs and invalid intervals;
removing an edge prevents redelivery to later substeps. Several distinct edges can
share a step. This is acquisition metadata, not a replacement for menu-filtered
PadMgr input; the existing Player snapshot still records what gameplay received.

| Reset/event | New sidecar behavior; legacy gameplay history unchanged |
|---|---|
| Process / fresh test fixture | Static initialization; replay always launches a new process |
| Save/scene initialization | Scene epoch increments, Player unbound, scope invalidated, input queue/last sample cleared, world-source cursor rebound |
| Player create/destroy/reused address | Monotonic Player generation on create; destroy unbinds and invalidates; cancel only the Player interval, preserve any open world interval; pointers are lookup only |
| Action transition | Action generation advances and active attack ends; no reset of input or retained sword endpoints |
| Attack start/window/end | New epoch; explicit positive-window edge; existing melee reset ends the window; action transition ends attack |
| Animation change/restart | Main Player animation generation advances; marker cursors/interval reset |
| Equipment change | Observed shield/boots/held action/tunic change advances equipment/scope generations and clears pending input metadata |
| Pose admission loss | True-to-false transition invalidates scope; only the actual bound Player may report it |
| Pause/debug pause/freeze/transition/death/draw disabled | Play boundary invalidates on entry even when draw is skipped; no stale deferred input; original game pause behavior unchanged |
| QA hold | Complete transaction withheld before input; generations and all live state stay frozen, with no debt accumulated |

Scope invalidation ends sidecar attack/event activity and clears input ownership;
it never clears Player joints, weapon history, colliders or effects. Clocks and
generation counters are process-monotonic. Unsupported profiles still execute
the complete original 20-Hz behavior; metadata is not high-rate admission.

Future sword priming, combo extension, swing/voice, blur and mutating hooks each
need their declared source opportunity, never just an active-condition test.
World/UI requests and target response are boundary-owned. `ContactEvent` is a
pointer-free schema: owner, attack/window, Player step/time, sequence, proxy/target
generation, collider/element, damage/animation, hit geometry/material/effect/bounce
and response state. `SameHitOpportunity` deliberately ignores which sword quad
produced a repeated observation. Target generation/element group and a new authored
window distinguish legitimate future hits. The canonical four-raw-contact/one-cut
sign behavior remains a target-response rule; no dedup or bridge dispatch runs.

## QA and inspection

`--observe-temporal` writes `temporal.jsonl` and a completion receipt separately
from preserved semantic snapshots and full traces. It adds rate/time/step IDs,
generations, action/animation, attack/window, queued/consumed input, pose generation,
sword history, empty contact queue, static collision summary and camera/yaw using
existing semantic observers. Temporal bytes also join direct Player purity checks.

`--native-sim-step-control` requires replay and implies observation. The complete
canonical transaction is gated before Graph_StartFrame/input and committed only
after graphics/audio/save-state processing. It never uses frameAdvCtx. A fresh
output directory accepts atomic `qa-command.json` commands with strictly increasing
sequence, current tick and operation step/run/pause. Paused `qa-state.json` exposes
inspection and the last image remains visible; window events continue pumping.
Gameplay, input, animation, logical audio and render replay do not advance while
held. The native verifier compares live semantic and temporal state across every
hold without restoring either. This is a replay/dev control, not an interactive
gameplay debugger or a public pause-menu replacement.

`run_corpus.py --single-step` drives one grant per measured transaction and holds
the first three boundaries for additional checks. Continuous/stepped and selected
presentation variants compare both original snapshots/traces and the entire
separate temporal stream. No new metadata field is normalized away.

## Reviewed implementation checkpoint and retained history

The first engine probe (`build/pass4a-01/probe`) passed idle/combo three times
each, exact against Pass 3D. Review then added Player-step input-delivery identity,
shared interval versus separate elapsed clocks, actual-Player pose observation
and draw-skipping pause/transition invalidation. The revised native core passes
104 checks (`temporal-unit-reviewed`), and Python passes 137 tests (`tooling-02.log`).
`build/pass4a-02/runtime-identity.json` binds the reviewed executable
`6fd4fe65511eaf9f86628f68478a0bff455780ad61e861a76849a086b44a689f` to native
source hashes. Build receipt is `build/pass4a-02/build`; the first validated slice
was combo passing three exact repetitions including temporal bytes.
The first integration compile lacked the window bridge declaration;
`build/pass4a-01/build-01` retains that compiler failure. Adding its existing header
fixed it. No native crash occurred. Acceptance results follow.

The complete reviewed canonical gate subsequently passed 30/30 runs: 2,580
transactions and 2,610 snapshots. Strict analysis preserved 2,451 live joint
mutations, 24 draw-owned combo mutations, 5,466 registrations, 24 raw contacts
and six target responses. Direct Player purity covered 3,756 prepared packets,
7,512 extra emissions and 142,728 ordered commands, including unchanged temporal
metadata. All 20 existing admission negatives still reject in every process.

Focused original regression passed nine runs (input-short-pulse, draw-rng-keese,
ocarina-memory-rng). HUD/message regression passed six (hud-warning and
message-pages-natural), with both extracted helpers passing direct purity.

The first QA campaign stopped on a Windows file-sharing race at combo tick 75:
the native reader briefly denied atomic replacement of `qa-command.json`. The
Python exception handler terminated its owned process; this was test infrastructure,
not a native exception. Two completed runs and the third partial run remain under
`build/pass4a-02/single-step`, with classification in `single-step-failure.json`.
The driver now retries the identical command/sequence within the existing timeout
and records controller termination on error. It does not change game code or state.
After two focused regression tests, the Python suite passes 139 tests.

Fresh `single-step-retry` acceptance then passed combo/sign three times each:
570 separately granted/held canonical transactions, 576 snapshots and 18 extra
timed hold checks. Every original state/trace and entire temporal stream matches
the same-executable continuous reference. The full canonical suite was not rerun
for a Python-only publication fix; all native source and executable hashes remain
unchanged. Presentation/CLI/control/startup results follow below.

## Final lifecycle ownership review

Source review found that the new sidecar's Player lifetime reset also cleared
`worldStep`. Destruction during an open world transaction could then erase the
interval before its end-frame commit. This was a source finding, not an observed
runtime failure. `ResetScope` now cancels only `playerStep`/`playerOpen`;
`PlayerTemporal_SceneInit` owns world-context clearing. Original gameplay remains
unchanged. No native fault or actor destruction was deliberately induced.

The final rebuild is bound by `build/pass4a-03/runtime-identity.json`, executable
SHA256 `68c47ba702ed6a164ca620aa3e59e0ebb08c38701c55364986f334481de331f6`.
Six fresh combo/sign runs and direct purity pass against the reviewed canonical
reference. Three fresh stepped-sign runs and ordinary startup also pass.
`source-reuse.json` verifies that only this adapter changed, and that all 30 earlier
canonical runs retain one scene/Player lifetime throughout measurement. The broad
suite is retained with explicit source reuse; it is not represented as a new
30-run execution on the final binary.

## Final acceptance and source identity

Native implementation checkpoint: `a0522e86d` (Player lifetime ownership fix).
Earlier reviewed engine checkpoint: `716faa9af`; QA driver repair and classification
review: `53a53f6cb`. All were pushed normally to `ChocoChopin/Shipwright`,
`mod/native-simulation-rates`. The final documentation commit identifies this handoff.
`build/pass4a-03/final-acceptance.json` binds receipt hashes, final source hashes,
executable identity, source reuse and retained failures. Local evidence stays ignored.

| Accepted gate | Exact result and retained location |
|---|---|
| All ten canonical Player fixtures, reviewed build | 30/30; 2,580 transactions / 2,610 snapshots; `build/pass4a-02/player-canonical` |
| Strict Player phases/contacts | 30/30; 2,451 joint mutations, 24 combo mutations, 5,466 registrations, 24 raw contacts / six target responses; `player-canonical/player_state_result.json` |
| Direct Player purity | 3,756 packets, 7,512 extra emissions, 142,728 ordered commands; temporal metadata unchanged; 20 admission negatives reject per process; `player-canonical/purity-analysis.json` |
| Focused original regression | 9/9; short input pulse, draw RNG and ocarina memory RNG, each three times; 540 transactions / 549 snapshots; `original-canonical` |
| Focused HUD/message regression | 6/6; warning and natural message pages, each three times; 660 transactions / 666 snapshots; both helpers pass direct purity; `draw-canonical` |
| Canonical QA stepping, reviewed build | 6/6 combo/sign; 570 held/granted transactions / 576 snapshots, plus 18 timed hold checks; exact original state/trace and entire temporal stream; `single-step-retry` |
| Rendering independence, reviewed build | 12/12 combo/sign at rendering 60/120 FPS, each three times; plus 3/3 trace-disabled slash; exact temporal stream and authoritative state; `presentation` |
| Final-binary focused canonical | 6/6 combo/sign, 570 transactions / 576 snapshots; exact prior canonical state/trace/temporal bytes; `build/pass4a-03/canonical` |
| Final-binary direct purity | 804 packets, 1,608 extra emissions, 30,552 commands; same existing admission negatives; `build/pass4a-03/canonical/purity-analysis.json` |
| Final-binary QA and phase/contact review | 3/3 stepped sign, 240 held/granted transactions / 243 snapshots and nine extra timed holds; all nine final runs pass strict phase/contact and exact temporal comparisons; `build/pass4a-03/single-step`, `build/pass4a-03/final-focused-analysis.json` |
| Native temporal core | 104 checks; production header and test inputs rehashed unchanged; `build/pass4a-01/temporal-unit-reviewed` |
| Python tooling | 139/139 after command-lock tests; `build/pass4a-02/tooling-final-reviewed.log` |
| Native CLI | 45/45, source-verified reuse on final build; `native-cli/native-validation.json` |
| Graceful controls | Copied snapshot mismatch detected at tick 17 and restored copy passes; deliberate purity mutation exits normally with code 2 and expected diagnostic; `mismatch-control`, `purity-control` |
| Ordinary startup | Reviewed and final builds pass visible-window/scene initialization and graceful close, exit 0, no forced termination; final `build/pass4a-03/startup/smoke.json` |
| Renderer helpers | 75 prior checks reused after input rehash: texture 24, TLUT 31, coverage 20; no dependency change; `helper-reuse.json` |
| Inventory | 2,437 source files / 429 actor entries, zero missing sources, one existing unmapped overlay; final regeneration and source hashes in `build/pass4a-03/inventory-freshness.json` |

Unless stated otherwise, short evidence paths in this table are beneath
`build/pass4a-02`. All fixture groups use three fresh processes per case. Historical
six-run probes and the two completed runs before the command-lock failure are
retained but are not inflated into these acceptance totals.

Source reuse is bounded: the final native delta only corrects ownership when
Player lifetime changes. All 30 earlier measured Player runs have stable scene
epoch 1 / Player generation 1; the reset is not entered during measurement. Other
native source hashes, canonical arithmetic, CLI, rendering and controls are
unchanged. Fresh final-binary runs cover startup, combo, sign contacts, purity and
stepping. No new transition/destruction campaign or second complete matrix is claimed.

Dependency pins remain libultraship `9280b17ddc504da6630892a46440e86be41ac571`
and torch `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. Existing Windows toolchain:
MSVC 19.44.35228, CMake 4.4.3, Python 3.12.5. Runtime receipts bind local asset
hashes; no asset is committed. The incremental build's embedded version string is
older than its source: use the recorded executable/source hashes, not that label.

There was no new native crash. The initial check-count receipt error, missing
bridge-header compile, pre-staging inventory omission, and Windows QA command-lock
failure remain preserved and classified above. The lifecycle correction was a
source-review finding, not a hidden failed runtime gate. No human gameplay,
audible acceptance or high-rate gameplay was performed. Authority remains 20 Hz.

Successful JSONL evidence uses lossless NTFS compression with before/after hashes;
`build/pass4a-03/compression.json` records logical and allocated sizes. No historical
failure, reference or partial output is deleted. Fixture assets use hardlinks.
The 231 completed JSONL files total 5,296,107,597 logical bytes and 2,150,621,184
allocated bytes; drive free space at the final compression receipt is
16,481,013,760 bytes. Those figures describe this pass's compressed streams,
not all historical build storage.

## Review and deliberate omissions

The second source review verifies that every original line in the seven touched
gameplay/outer-loop owner files is retained; additions only label opportunities
or gate QA. It checks impulse versus rate, root-motion displacement, mixed combo
and timer state, event duplication, scene/Player reuse, world20 ownership and the
unchanged Hz20 expression path. `build/pass4a-02/source-review.json` records the
review and exact source additions. No generic scale reaches a gameplay caller.

The existing 75 texture/TLUT/coverage checks are reused only after rehashing every
bound source/header; `helper-reuse.json` records that verification. No dependency
pin or renderer implementation changes. Inventory is regenerated after staging
the new files, with 2,437 source files and 429 actor entries; zero missing actor
sources and one pre-existing unmapped source remain. The first pre-staging scan
missed the newly untracked adapter files; the pushed checkpoint and corrected
freshness receipt preserve that bookkeeping correction.

Deliberate limits of this pass:

- No authoritative gameplay above 20 Hz, runtime 30, physical input cadence change,
  animation/movement/camera retiming or contact-bridge dispatch.
- No native affine/exponential movement helper until a concrete admitted motion
  model requires it; integer remainder/duration/marker contracts suffice here.
- No large historical cross-product or startup-stress rerun. Focused original/HUD
  selection and representative rendering cases are recorded with final totals.
  The previously omitted 48 Player variants and 39 startup-stress runs remain
  omitted. No full original 12-fixture or eight-fixture HUD corpus was rerun.
  No second full ten-Player/focused-rendering matrix after the isolated reset fix;
  final-binary coverage and source reuse are explicitly separated above.
- No new complete scene-transition, death, equipment-mod or game-menu matrix.
  Their sidecar invalidation sites are reviewed, generation contracts tested in
  native code, and fresh-process fixture lifetime is exercised repeatedly.
- No new hostile block/parry, spin/multi-hit or unadmitted dynamic collision claim.
  The contact schema permits later authored windows but implements no hit policy.
- No human gameplay, audible or pixel/GPU acceptance. Automated startup is a
  separate gate and uses muted master volume.

## Proposed next pass and milestone estimate

Pass 4B: implement a gated fixed Player scheduler and its complete ordinary
animation/input/camera/static-background/late-pose dependency closure over the
unchanged world20 transaction. Preserve the original Hz20 branch and test genuine
intermediate authoritative steps before enabling any capability. Resolve the
inventory's branch-specific smoothers/timers at their sole owning layers.

Pass 4C: integrate generation-bound authored contact opportunities with world20
target responses, qualify the ordinary sword pilot, and perform human interactive
acceptance. Estimate two substantive passes from 4A, with a possible third if
motion/camera ambiguities or interactive qualification require separate work.
This estimate grants no approval to start that work in Pass 4A.

## Changed file/function map

| Files | Implementation |
|---|---|
| `soh/soh/PlayerTemporalCore.hpp` | Native types, checked Add/StepQuanta/CommonBoundary, countdown/remainder/wrap, Crossings/AnimationEvents, OpportunityCursor/ConsumeWorld, Lifecycle/AttackState, InputTimeline/ConsumeForPlayer, ContactEvent/SameHitOpportunity, CanonicalControl |
| `soh/soh/PlayerTemporal.h`, `.cpp` | C bridge: SceneInit, ActorCreated/Destroyed, Begin/EndFrame, PlayBoundary, Sample, ActionChanged, AttackStarted, MeleeWindow, AnimationChanged, PoseAdmission, InputSample/LiveInput/InputConsumed; C++ Inspect |
| `soh/src/overlays/actors/ovl_player_actor/z_player.c` | Observer calls at Player_SetupAction, func_80837948, func_80833A20 and func_80832318; original state assignments untouched |
| `soh/src/code/z_skelanime.c`, `z_player_lib.c`, `z_play.c` | LinkAnimation_Change generation, actual Player_DrawImpl admission observation, Play_Main boundary observation |
| `soh/src/code/padmgr.c` | PadMgr_HandleRetraceMsg acquisition label and PadMgr_RequestPadData consumption eligibility; old processing unchanged |
| `soh/src/code/graph.c`, `soh/soh/OTRGlobals.cpp` | Pre-transaction NativeSimTest_WaitFrame and test-only window-event pumping while held |
| `soh/soh/NativeSimulationTest.cpp`, `.h`, `NativeSimulationTestInput.cpp` | Test CLI/inspection/QA receipts, production lifecycle seam dispatch, extra temporal purity closure, replay acquisition labels; existing semantic stream unchanged |
| `scripts/native-simulation/native/player_temporal.cpp`, `validate_player_temporal.py` | Standalone MSVC test of production header, identity-bound receipt |
| `run_corpus.py`, `presentation_matrix.py`, `temporal_qa.py` | Observation/step options, file-command driver, strict temporal invariants and exact metadata comparisons |
| `validate_native_cli.py`, `test_native_cli_control.py`, `test_temporal_qa.py`, `test_temporal_inventory.py` | CLI option guards, temporal negative controls and source-anchored inventory checks |
| `AGENTS.md`, architecture/plan/semantics/testing/ledger/divergences, `player-island.json`, inventory and this handoff | Authorized scope, precise ownership, audit coverage and evidence; no gameplay admission claim |
