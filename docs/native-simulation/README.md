# Native simulation rates: architecture handoff

The user's subsequent live-mode policy removes profile eligibility vetoes and
permanent fallback from the experimental60/120 selector. See the first section of
[the pilot fix](PASS4C_PILOT_FIX.md). Historical fixture admission/results below
remain evidence of that bounded closure, not proof of all unrestricted gameplay.

**Human pilot blocker corrected:** [ordinary Navi/fidget admission and persistent
rate status](PASS4C_PILOT_FIX.md). This contains the current executable identity,
narrow validation and launch instructions; user gameplay confirmation is pending.

**Pass 4C is complete: bounded Player/sign contact and a human-test build.**
Player60/120 ordinary and Z-targeted slashes now cut the controlled world20 sign.
Canonical20 retains the legacy contact path. Settings / Graphics exposes the
experimental Player-rate selector and effective-rate/fallback status. Human
playtesting remains pending. See [PASS4C.md](PASS4C.md) for exact launch instructions,
qualification, limitations and source/build identities. Stop for user pilot feedback.

**Pass 4B completed the bounded Player scheduler and lean online validator.**
Input, movement, static collision, animation, control camera and immutable pose
execute behind whole-profile admission. At that historical checkpoint, selection
was fixture-only and contact remained closed; [PASS4B.md](PASS4B.md) preserves its
receipts and cleanup manifests. Successful runs emit compact hashes/counters,
with detailed state retained only in bounded failure rings. Earlier sections below
record their own historical capabilities.

**Pass 4A is complete: native temporal foundations and canonical QA controls.**
The 120-quanta vocabulary, bounded unit inventory, opportunity/generation contracts
and exact whole-transaction pause/step control are implemented. All ten canonical
Player fixtures, focused regressions and direct purity pass; final-build checks
and explicit source reuse are distinguished in [PASS4A.md](PASS4A.md).
Gameplay remained 20 Hz at that checkpoint. Its scheduler successor is Pass 4B.

**Pass 3D is complete: bounded canonical Player pose/contact extraction.**
All ten Player fixtures pass three repetitions on the final native source;
direct CPU purity, 20 negative admission cases, focused presentation and broad
regressions pass. See [PASS3D.md](PASS3D.md) for identities, counts, reuse and
omissions. Player authority ran at 20 Hz at that checkpoint; no human gameplay was performed.

**Pass 3C is complete under the user's explicitly reduced acceptance scope.**
Player ownership design, observation and ten canonical fixtures are accepted on
the validated renderer repair. Coverage is 90 canonical replays, 135 presentation
variants, 43 CLI checks, focused controls and ordinary startup; prior helper gates
remain source-verified. The user omitted 48 remaining Player variants and 39
startup-stress runs. No new native crash occurred; one earlier runtime-budget
overrun and all historical failures remain preserved.

See [PASS3C.md](PASS3C.md) for exact identities, counts and omissions. Pass 3B's
accepted references remain untouched. Authoritative 60/120-Hz gameplay was not
implemented at that checkpoint.

The objective is a genuine fixed-step Player island at **20, 60 and 120 Hz** over a 20-Hz world, with
rendering independently configurable. Pass 2 adds opt-in canonical replay, and
Pass 3A adds bounded draw-state observation and an extraction design, while
retaining normal gameplay arithmetic. Pass 3B now completes the bounded late
countdown/plain-message extraction and direct CPU-helper purity acceptance;
see [PASS3B.md](PASS3B.md) for that accepted checkpoint and [PASS3C.md](PASS3C.md)
for the Player-specific design and acceptance status. A reproduced renderer palette over-read
also has a reviewed, scoped dependency fix that passed rebuilt-engine validation.
Those pre-scheduler passes did not implement high-rate simulation. Rendering
interpolation alone does not meet it.

The branch is `mod/native-simulation-rates`, based on upstream develop
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`. The fork is
[ChocoChopin/Shipwright](https://github.com/ChocoChopin/Shipwright).

Pass 2's bounded implementation and validation are complete. The final corpus
passes 12 fixtures in three fresh processes each, and the presentation/trace
matrix passes 21 more runs with exact semantic snapshots. The Python suite passes
60 tests at that checkpoint and the executable passed 36 native CLI checks. Negative-control,
coupling-analysis, diagnostic-compatibility and ordinary-startup gates also pass.
The historical corpus 02 remains failed: it completed 35 of 36 engine runs and
retained a premeasurement startup access violation whose cause was unresolved at
that checkpoint. The later clean corpus did not establish a fix.
Pass 3A preserves those references and records the old HUD countdown and simple
message behavior before extraction. The preserved build completed 180 short
startup trials and 12 full gravity replays without a TLUT access violation.
A later candidate reproduced the fault in corpus 05: 34 runs completed of 35
attempted, gravity run 002 failed and run 003 was unattempted. Its captured owned
sword palette provides 216 bytes for a 512-byte renderer copy. All eight audited
display-list consumers use jewel texture indices no higher than 106, within the
stored palette. The reviewed `ChocoChopin/libultraship` fix on
`codex/tlut-source-bounds` bounds staging to resource storage and defines a zero
fallback for missing tail bytes; this is not byte-identical N64 bulk-transfer
emulation. Archives are unchanged. The dependency fix is committed as
`c6bbb8c328938c115f4a1cbeaca3d00a4502269d`; its ordinary native helper test passes 31
checks. The fixed executable passes the original 36 canonical runs, 21 matrix
runs, 37 native CLI checks, ordinary startup and 192 fresh startup trials.
No intentionally crashing test was performed. Five separate copied-executable-location
timeouts remain recorded. Eight additional fixtures observe countdown boundaries,
old-digit warnings, message/input interaction and
actual CPU timer/glyph/icon emissions. The Pass 3A handoff limited Pass 3B to
main countdown and English 0x1043 plain-message authority at their existing late
overlay slots. No authority was extracted during Pass 3A.

Pass 3A validation is complete: 105 Python tests, 24 new canonical draw-state
runs with strict phase/input/paint and audio checks, and 72 new presentation/trace
runs pass. The 20-Hz snapshots remain exact at 60/120 presentation FPS and with
tracing disabled. That handoff required a direct CPU-helper purity gate before accepting the
bounded extraction; Pass 3B now satisfies it within the documented envelope.

Interactive gameplay has not been validated. See [BASELINE.md](BASELINE.md) for
the original build/extraction receipt, [TESTING.md](TESTING.md) for the current
contract and evidence status, and [PASS2.md](PASS2.md) for final source/artifact
identities and acceptance accounting. [PASS3A.md](PASS3A.md) retains pre-extraction fixture/design evidence;
[PASS3B.md](PASS3B.md) owns current extraction acceptance, source identities,
startup denominators, review corrections and the next-pass recommendation.

## Reading and working order

| Document | Purpose |
|---|---|
| [BASELINE.md](BASELINE.md) | Exact checkout, Windows build, ROM and launch evidence; reproduction commands |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source call graph, R_UPDATE_RATE audit, clock/render/input/audio design |
| [TIMING_SEMANTICS.md](TIMING_SEMANTICS.md) | Units, A-I taxonomy, equations, canonical behavior and divergence policy |
| [TESTING.md](TESTING.md) | Implemented replay contract, exact commands, fixtures, comparisons and coverage limits |
| [PASS2.md](PASS2.md) | Pass 2 artifact identities, completed gates and unresolved reliability evidence |
| [PASS3A.md](PASS3A.md) | Startup characterization, pre-extraction observation acceptance and reviewed High-pass handoff |
| [PASS3B.md](PASS3B.md) | Accepted bounded extraction, helper purity and exact runtime identities |
| [PASS3C.md](PASS3C.md) | Player ownership, canonical reference fixtures, nested Player/world boundary and next High scope |
| [RNG_AUDIO_AUDIT.md](RNG_AUDIO_AUDIT.md) | Controlled RNG streams, audio sink, draw/audio authority and observation limits |
| [EXECPLAN.md](EXECPLAN.md) | Ordered phases with prerequisites, tests, gates, failure modes and reasoning effort |
| [CONVERSION_LEDGER.md](CONVERSION_LEDGER.md) | Curated subsystem findings, claim workflow and machine-readable coverage |
| [KNOWN_DIVERGENCES.md](KNOWN_DIVERGENCES.md) | Expected resolution consequences and unresolved design choices |

Repository [AGENTS.md](../../AGENTS.md) preserves safety, scope and Windows workspace
rules for future agents without this conversation. Inventory files contain only
non-proprietary source metadata, never extracted ROM content.

## Architectural decision

Use one rate-parametric temporal context and integer 120-unit/second clock, with
fixed Player step advances 6/2/1. Preserve the exact legacy 20-Hz path. Inventory each
field's units and distinguish continuous rates, durations, impulses, decision
opportunities and authored frame identity. The three supported rates share authoritative comparison endpoints every 50 ms.
The human control loop must actually advance on Player substeps; unrelated world
logic remains 20 Hz by design. Runtime 30-Hz gameplay is deferred; inexpensive
s=2/3 mathematical tests remain useful but do not imply gameplay support.

Before higher-rate conversion, establish reproducible canonical replay and move
authoritative draw side effects into explicit simulation phases. Draw currently
advances timers, updates combat geometry/culling and consumes gameplay RNG. Audio
cadence and RNG also affect gameplay. Merely changing R_UPDATE_RATE or calling
Play_Update more often would accelerate or desynchronize much of the game.

## What the tools prove

`scripts/native-simulation/baseline.py` provides local build/extraction phases,
hash verification and logs. `inventory.py` generates reproducible heuristic source
metadata. `semantic_oracle.py` and its tests demonstrate candidate mathematical
contracts. `run_corpus.py` drives the opt-in engine replay interface, validates
semantic snapshots, computes SHA-256 hashes and produces field-level differences.
It runs fresh processes and requires at least three consecutive repeats. Fixture
assertions check exercised behavior as well as equality. Cross-rate invariants
remain future work.

The implemented tools cover the bounded canonical replay envelope, coupling
instrumentation, startup stress and the selected HUD/message diagnostics; they
do not satisfy every scenario in the broader execution plan. Pass 3B satisfies the prior bounded extraction specification. Follow PASS3C.md
for the next Player-specific High pass; do not generalize its admission.
That acceptance keeps old and new 20-Hz state/paint/event traces exact and
requires a direct repeated CPU-helper purity gate;
display-list replay at higher FPS does not establish that property. Leave actor
pose, collision, enemy draw RNG, other HUD/message profiles and audio scheduling
at their current seams. No higher-rate or bulk actor conversion is admitted by
this checkpoint.
