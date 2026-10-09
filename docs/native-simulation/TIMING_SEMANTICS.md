# Temporal semantics and mathematical contracts

## Pass 4C sign adapter

Canonical20 retains the live Player melee-animation alias. Bridged high-rate sign events instead capture producing attack animation and damage identity. Detection follows committed Player pose; complete feedback is delivered after legacy AT at the next world boundary, before Player and sign updates. No target timers or updates run at Player cadence.

Status: bounded internal Pass 4B Player adapters execute at 60/120 Hz; this is
not general gameplay support. Source inventory baseline:
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`; current evidence is in PASS4B.md.
Read ARCHITECTURE.md for the actual call graph and CONVERSION_LEDGER.md for examples.

Pass 4B internal gated adapters (not yet fully qualified) choose the constant-force
affine continuation below for Player vertical motion. `PlayerMotionCore.hpp`
retains legacy velocity units and gain 1.5 per 50 ms; a terminal hold is constant
velocity, while a clamp-crossing segment is rejected before commit. The
whole-profile preflight excludes it before any Player interval mutation.
World OC corrections remain displacements applied once at shared boundaries;
root deltas bypass this adapter entirely. This does not admit freefall or resolve
all coupled action-acceleration/contact cases under ND-001.

Bounded linear angular caps retain signed sixths residue for positive/negative
steps and wrap. Snaps, external angle assignments, action and Player scope changes
reset the residue. Nonlinear cap/snap and moving-target equivalence remain Class 3
choices. Combo-window
and target timers use event-anchored six-quanta opportunities, not render counts;
combo ordinal changes remain immediate, while late-pose combo extension and blur
each have a separate six-quanta owner. Canonical branches retain their old calls.

Pass 4A adds native vocabulary and isolated integer/marker/opportunity primitives,
not gameplay scaling. Its clock/reset tables and `player-temporal-units.json`
name owners explicitly. Step intervals share simulation availability time while
Player elapsed time advances only when Player updates. QA transaction time starts
after fixture setup; ordinary world elapsed time includes canonical setup work.
Authored Q16 intervals exclude their origin and include their destination in both
directions; the owner must provide unwrapped loop extent. Existing Link float
arithmetic, event tests and legacy sword resets remain unchanged. A positive
melee active-window edge is distinct from attack epoch and negative priming.
Player lifetime resets cannot erase an open world interval. Scene initialization
owns world-context reset; neither sidecar reset changes legacy sword history.
`unk_844` combines signed duration and B-release latch/opportunity semantics;
`meleeWeaponState` is encoded ordinal/window state, not elapsed animation time.

Pass 3C accepts the Player ownership/reference design under its documented reduced
test scope; it implements no timing primitive or higher-rate gameplay. Runtime
targets remain 20/60/120, with 30-Hz gameplay deferred and cheap s=2/3 math retained.

Pass 3D preserves these clocks and all prior-pose/contact latencies. Its admission
review confirmed a useful state distinction: `Player_UpdateHoverBoots` refreshes
`hoverBootsTimer` to 19 on ordinary grounded non-hover-boot updates, after its
earlier reset. A nonzero value therefore does not by itself mean hover boots are
active. Admission checks equipment and grounded/static-surface state instead.
The existing decrement/reset/refresh opportunities remain unchanged; this pass
does not reinterpret or scale that counter.

Pass 2 implements only the observation clock: one measured canonical transaction
advances `time_q` by six, and its snapshot is labeled at the interval endpoint.
Tick zero is captured after declared setup transactions and optional semantic
fixture initialization. Inputs retain rational timestamps in seconds; all due
transitions are fed through the original PadMgr accumulator at the next beginning
boundary. These controls do not modify movement, animation, timers or physics.
Measured non-3 legacy divisors and scene transitions are rejected rather than
mislabeling their elapsed time as canonical 20-Hz world steps. The deterministic
528-sample audio sink is a fixture envelope, not the proposed production scheduler.

## 1. Reference and units

Normal gameplay's canonical step is `h0 = 1/20 second`. For a selected rate
`f in {20,60,120}`, `h = 1/f`, and `s = h/h0 = 20/f`. Simulation uses fixed
steps; wall time decides how many steps to execute, never their size.

| f | h in seconds | s | integer clock advance at 120 units/second |
|---|---|---|---|
| 20 | 1/20 | 1 | 6 |
| 60 | 1/60 | 1/3 | 2 |
| 120 | 1/120 | 1/6 | 1 |

Use an integer/rational time domain for tick identity, deadlines and scheduling.
These are clock units, not a hidden 120-Hz gameplay loop. The selected rate advances the admitted Player island; unrelated world work
retains 20 Hz. All supported rates share authoritative endpoints every six units
(50 ms). Runtime 30 Hz is deferred. The standalone oracle retains s=2/3 as an
adversarial mathematical case against hardcoded /3; this adds no runtime, QA,
settings or release obligation. Interpolated observations are not authoritative snapshots.

Input timestamps may be finer than this lattice (integer host-counter ratios or
nanoseconds); do not round capture time prematurely. Rational comparisons assign
events causally to simulation boundaries. Use integer remainder accounting for
host clock conversion and audio sample budgets.

The engine mixes units already. `Actor_UpdatePos` (`soh/src/code/z_actor.c:1287`)
moves by `1.5 * velocity` at `R_UPDATE_RATE=3`, while gravity adds once per update
at line 1307. Link animation uses `R_UPDATE_RATE*0.5` and generic animation `/3`
(`soh/src/code/z_skelanime.c:1215,1648`). A stored velocity is not automatically
distance per canonical tick or SI velocity. Record each field's existing unit
before transforming it. Existing R_UPDATE_RATE 1/2 states and slowdowns need
their own reference cadence; do not redefine their behavior as 20-Hz gameplay.

20-Hz mode keeps the legacy arithmetic, casts, clamps, branch order, update order,
and random draw order. Evaluating a mathematically equal new expression can change
floating-point rounding. Helpers must have an explicit legacy branch until exact
equivalence is demonstrated. No compiler fast-math/FMA relaxation; pin compiler and
floating-point mode in receipts. Determinism across identical builds is the first
contract; cross-compiler/architecture bit equality requires separate evidence.

## 2. Conversion taxonomy

### A. Linear rates and periodic phase

If a legacy update adds `d`, a constant-rate continuation adds `s*d`. Prefer
named units (`LegacyDistancePerTick`, `RadiansPerSecond`, `DurationTicks`) at
boundaries instead of silently changing every stored field. Scale at one owning
layer only. A displacement already produced by animation, collision resolution,
or a platform transform is a displacement and must not be multiplied again.

For integer angles, alpha, texture offsets and similar accumulators, retain the
fractional remainder or a wider fixed-point phase, then quantize at the declared
consumer. Repeated `(int)(s*d)` can freeze motion at 120 Hz. Specify signed
rounding, wrapping, overflow and shortest-angle ties. Legacy quantization is
retained in canonical mode; new phase state needs spawn/reset/save ownership.

Texture/environment phase is generally `phase(t)=phase0+omega*t`, followed by
the original wrap. A frame-indexed lookup may instead be frame identity (G).
Animation advances an unwrapped phase, then wraps/clamps for display; marker
events test interval crossings, including reverse playback and loop wraps,
not float equality. Root motion comes from pose differences, not duplicate scaling.

### B. Acceleration, integration, and order

`velocity += acceleration*s; position += velocity*s` is a semi-implicit integrator
with a changed trajectory. Correct dimensional scaling alone does not preserve
the canonical discrete trajectory at common times.

For the unclamped legacy map

```
v_next = v + g
x_next = x + c*v_next
```

an affine fractional continuation exists:

```
v_next = v + s*g
x_next = x + c*(s*v + g*s*(s+1)/2)
```

Here `v` on the second line is the pre-step value; `c=1.5` for the cited Actor
helper in normal play. Derivation: after n legacy steps, velocity is `v+n*g`
and displacement is `c*(n*v+g*n*(n+1)/2)`. Substituting real s produces a semigroup
for constant g: composition for a then b equals a+b. It agrees with canonical
endpoints and supplies actual intermediate positions, including the adversarial nonintegral s=2/3 helper case.

For the reverse explicit order `x_next=x+c*v; v_next=v+g`, the quadratic term
is `g*s*(s-1)/2`. These are different models. The semi-implicit embedding corresponds
to continuous initial physical velocity `c*(v+g/2)/h0`, rather than `c*v/h0`.

Preferred candidate: use the affine continuation for an isolated constant-force
segment when preserving legacy endpoints matters. This is **not** a universal
physics replacement. Terminal-velocity clamps, input-dependent acceleration,
contacts, bounces and force discontinuities break the simple semigroup. Their
split points and operation order require fixtures and a documented model. A
physical SI integrator is another valid high-rate continuation, but differences
at legacy sample times must be classified as a deliberate Class 3 choice, not
silently balanced away. This never permits changing the 20-Hz compatibility path.

Impulses (jump velocity assignment, knockback, explosion impulse) are instantaneous
state changes, not rates. Never multiply an impulse by s. A per-step collision
correction is not a force. Coupled damping/acceleration/movement must be analyzed
as a combined map, not a bag of independently scaled lines.

### C. Exponential and multiplicative processes

For `x_next=q*x`, positive q has continuation `q_s=q^s`, or
`exp(s*log(q))`. For convergence `x_next=x+alpha*(target-x)` with fixed target,
`alpha_s=1-(1-alpha)^s`. Linear `alpha*s` is not equivalent.

Use numerically stable `-expm1(s*log1p(-alpha))` offline to derive constants;
runtime libm results may differ by platform. Three fixed-rate, versioned coefficient
tables or a controlled implementation are preferable for reproducible engine math.
The canonical mode uses original coefficients/operations. `alpha=0/1`, q=0,
negative q, overshoot and unstable coefficients require explicit domain handling;
negative q generally has no real fractional continuation.

For affine `v_next=q*v+b`, q != 1 gives
`v_s=q^s*v+b*(1-q^s)/(1-q)`, with limit `v+s*b` at q=1.
For coupled `v_next=q*v; x_next=x+c*v_next`, displacement is
`c*q*(1-q^s)/(1-q)*v`, not simply `s*c*v_next`. Its q=1 limit is `c*s*v`.
This illustrates why individually correct decay scaling can still give wrong travel.

`Math_ApproachF`, `Math_SmoothStepToF`, and integer angle equivalents
(`soh/src/code/z_lib.c:386,425,514,554`) mix exponential response, maximum step,
minimum step, integer division and snapping. Scaling exponential alpha and linear
caps is a useful candidate, but not the exact fractional iterate of this piecewise
map. Region crossings, target changes and snap thresholds require explicit tests.
Never replace all smoothing calls with one unexamined coefficient formula.

### D. Durations, countdowns, and cooldowns

Pass 4B's bounded camera adapter provisionally selects the fractional exponential
for fixed-target gains in [0,1]. Camera position/speed observations retain legacy
distance-per-50-ms units; actual Player displacement is normalized once by 6/q.
Camera countdowns retain sixths of a legacy opportunity, and angle accumulators
retain fractional binary-angle output. Min-difference snap thresholds remain
spatial/angular thresholds. World parity scratch is replaced by a complete static
slope query only inside the high-rate camera scope. Overshooting gains use a
declared local-increment continuation, a Class 3 choice without a semigroup claim.
These branches remain unadmitted pending engine fixtures; Hz20 expressions remain
separate. See PASS4B.md for exact ownership and exclusions.

A legacy duration N normal-play ticks represents `6*N` clock units. Record an
absolute deadline or signed remaining time plus remainder; don't truncate N*s or
round N*f/20 independently on each reset. One canonical frame lasts 50 ms, exactly three Player steps at 60 Hz or six
at 120 Hz. No non-nested gameplay deadline-jitter machinery is required. The
standalone 2/3-scale oracle may still test ceiling dispatch as generalized math.

Distinguish the exact semantic deadline from when a fixed step can act on it. Do
not introduce variable substeps simply to hit every timer. A scheduled event may
carry its original timestamp while committing on the first eligible step. Retain
overshoot/remainder for repeats so accumulated drift does not grow each period.

Transcribe the original state machine: decrement-before-test versus test-before-
decrement; whether assignment and decrement occur in one update; zero/signed
sentinels; freeze behavior; reset/scene boundaries; and transitions after expiry.
For example, `if (timer) --timer; else Fire();` acts one invocation later than
`if (timer && --timer == 0) Fire();`. Both can use N but have different contracts.
Legacy branches remain exact; initial deadline mapping comes from these traces.

Pause/gameplay/UI/cutscene/audio clocks are distinct. An invulnerability timer may
freeze when actors freeze; a menu animation may continue; a render throttle must
not alter either. Names ending in timer can also be state identities or counters.

### E. Discrete opportunities and state decisions

Classify each condition as continuous responsiveness (collision, held movement,
aiming) or a scheduled decision opportunity (AI choice, periodic spawn attempt).
Responsive conditions run each selected authoritative step. Legacy authored
decision cadence may retain rational 20-Hz opportunities while movement genuinely
runs at the selected rate. The selected Player/control island must actually run at 60/120; unrelated
world logic deliberately remains 20 Hz. Report both effective cadences explicitly.

At shared 50-ms boundaries, dispatch each due world opportunity once,
with deterministic due-time order, then preserved legacy phase/category/list
traversal and callsite sequence. Stable actor IDs identify traces, not a new sort
order. Multiple deadlines
must not disappear; define whether all or only one state transition is admissible.
World decisions sample the declared held/current Player state at their legacy
phase. Log sampling ambiguities; do not move all Player decisions to world boundaries.

### F. Random processes and random stream identity

For independent per-tick event probability p, an exponential-hazard interpretation
gives `lambda=-log(1-p)/h0` and `p_s=1-(1-p)^s`. This preserves no-event probability
over equal elapsed time. It does not preserve random-call order, coupled outcomes,
or a Bernoulli process's event-count distribution. A Poisson continuation permits
multiple arrivals and is a design choice, especially for p=1 or p outside [0,1].

Preferred for authored discrete opportunities: preserve their cadence and random
draw schedule. For a explicitly selected continuous random process: stable seeded
streams/event IDs and cumulative hazard thresholds can couple runs across rates,
but change stream semantics and require review. Never multiply p by s blindly.
Separate seeds alone cannot compensate for changed call ordering or render-side
draws. Existing 20-Hz global stream order must remain untouched.

Test same-rate replay exactly; compare event schedules for cadence-preserving
systems and distributions/invariants for probabilistic redesigns. Do not require
bit-identical RNG state across high rates unless that is an explicit model contract.

### G. Frame identity, parity, and scripts

Preserve distinct identities: authoritative step index, canonical elapsed-time
phase, gameplay-active clock, UI clock, script authored frame, and rendered frame.
Do not globally redefine `gameplayFrames` or multiply every `% 2` by a factor.
Parity may alternate collision work, flicker an invulnerable actor, index history,
or trigger an event: four different semantics.

For an authored event at legacy frame k, test crossing of its authored-time
boundary, preserving first-frame zero and inclusivity conventions. Use monotonic
unwrapped phase and event identity to prevent duplicate/omitted events, including
reverse animation and loops. A literal ordinal state marker remains an ordinal.

### H. Resolution consequences

Smaller steps can reveal wall/floor intersections, ledges, targets and button
changes at different times, often sooner. For supported nested cadences, each legacy boundary is also a Player boundary.
Higher Player rate reduces the maximum sampling
gap, not every individual event's latency. Finer steps can change contact normal,
actor order interactions, projectile hit
target and state transition count. Preserve physical/time constants and ordering,
then evaluate the recorded first causal divergence. Do not enlarge collision steps
or delay all input to canonical boundaries to erase legitimate improvements.

Continuous collision detection is a separate algorithmic decision, not something
automatically proved by 120 Hz. Platform movement and rider displacement need
the same authoritative transform interval; applying a full canonical displacement
every native tick is a conversion bug.

### I. Genuine semantic ambiguities

Discrete clamped smoothing, ballistic embedding, minimum invulnerability dispatch,
AI cadence, random-event models, same-step transition chains, legacy oscillation,
and script events between tick boundaries often have no unique continuation.
Record alternatives, selected provisional model, affected fixtures, and decision
owner in KNOWN_DIVERGENCES.md. Make progress on unrelated systems. Do not silently
change challenge, hit windows, probabilities or artistic timing.

## 3. Correctness and diagnostic decision tree

Priority: canonical 20 unchanged; durations and movement per second preserved;
animation/gameplay synchronization preserved; ordering kept as close as practical;
finer intermediate states allowed; balance/art direction changes explicit.

1. Does 20-Hz replay differ from the baseline with identical fixture/build envelope?
   This fails the canonical gate; trace it before wider conversion.
2. Do dimensional/analytic invariants fail (distance, decay, gravity, repeat drift,
   duplicate marker, impulse magnitude)? **Class 1: conversion bug.** Fix it.
3. Is the first difference a newly resolved input/contact/threshold consistent with
   the chosen model and its bounded dispatch latency? **Class 2: expected resolution
   consequence.** Preserve evidence; do not restore coarse simulation to hide it.
4. Do valid models imply different behavior or authored intent is unknown?
   **Class 3: semantic/design ambiguity.** Record an explicit decision, not a patch
   disguised as mathematical correctness.

Allow field-specific numerical tolerances only with physical justification. Do not
globally widen epsilon until a replay passes. Exact IDs/enums/event ordering are
not floating-point quantities. NaN/Inf, missed collisions, accumulating timer drift,
or changed canonical state hashes cannot be dismissed as resolution effects.

The bounded Player head/focus adapter uses the same fractional exponential for
uncapped proportional angle motion. `SmoothPlayerAngle` carries fractional
binary-angle output per field; min/max increments are per 50 ms and scale by
q/6. Approach-to-zero's legacy 0.1 step followed by the 1.5 update multiplier
becomes gain 0.15 with caps 600/6000. This selects a continuous high-rate model;
legacy integer division and piecewise cap transitions do not define a unique
fractional iterate (Class 3). The 20-Hz helper is unchanged. External writes,
action/scope changes and snaps reset the respective remainder. Two explicit
scratch owners cover the derived focus-yaw difference and upper-body yaw;
neither stores a stack address. Native tests cover uncapped endpoints, caps,
wrap, reset and rejection; engine high-rate qualification is still pending.

## 4. Required primitive properties

The standalone semantic oracle in `scripts/native-simulation/` checks representative
math contracts without the engine. Future C/C++ implementations must add engine
tests for: s=1 legacy equivalence; constant-force composition; decay semigroup;
long-run integer remainder; signed angle wrap; marker crossings/reverse/loops;
duration off-by-one/reset/pause; coupled damping/integration; clamp boundaries;
optional adversarial s=2/3 deadline dispatch; and invalid-runtime-rate rejection. Oracle success is not evidence
that any actor has been converted.
