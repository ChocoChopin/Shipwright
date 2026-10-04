# Pass 4A: Player temporal core and pre-pilot controls

Status: in progress; no acceptance claimed. Starting source
`f8fe6a7fd005a508b2626de2685e8df41efafa59`; dependency pins unchanged from PASS3D.

The authorized scope is native temporal vocabulary, opportunity and lifecycle
contracts, bounded Player unit inventory, canonical pause/single-step and diagnostic
inspection. PASS3C/PASS3D own the phase order and admitted profile. No gameplay
arithmetic, cadence, animation, camera or collision is retimed. Effective Player
and world rates remain 20 Hz. Contact records are schema only, with no active bridge.

Implementation starts with standalone native tests of the same header used by the
engine. Canonical metadata stays in a separate diagnostic stream so the preserved
semantic snapshots/full phase traces remain exact oracles. QA gates the complete
transaction before input and retains the last presented image while paused; it
does not use the legacy collision-suppressing frame advance.

Validation will cover all ten Player fixtures with three repetitions, strict
phase/contact analysis and CPU purity, focused original/HUD regressions, native
core and Python tests, CLI/controls/startup, canonical single-step equivalence and
representative presentation independence. No historical large matrix is restored.

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
No native fault occurred. Engine integration and full acceptance remain pending.

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

`unk_845`, melee-window state, action variables, targeting gates, invincibility,
hoverBootsTimer, smoothers and camera quantities remain mixed or branch-sensitive.
The inventory does not authorize bulk conversion of those groups. In particular,
grounded ordinary boots can carry hoverBootsTimer=19; nonzero is not admission.
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
| Player create/destroy/reused address | Monotonic Player generation on create; destroy unbinds and invalidates; pointers are lookup only |
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

## Reviewed implementation checkpoint (acceptance pending)

The first engine probe (`build/pass4a-01/probe`) passed idle/combo three times
each, exact against Pass 3D. Review then added Player-step input-delivery identity,
shared interval versus separate elapsed clocks, actual-Player pose observation
and draw-skipping pause/transition invalidation. The revised native core passes
104 checks (`temporal-unit-reviewed`), and Python passes 137 tests (`tooling-02.log`).
`build/pass4a-02/runtime-identity.json` binds the reviewed executable
`6fd4fe65511eaf9f86628f68478a0bff455780ad61e861a76849a086b44a689f` to native
source hashes. Build receipt is `build/pass4a-02/build`; all ten canonical cases
are running, with combo already passing three exact repetitions including temporal
bytes. The first integration compile lacked the window bridge declaration;
`build/pass4a-01/build-01` retains that compiler failure. Adding its existing header
fixed it. No native crash occurred. All final acceptance gates remain pending.
