# Native simulation rates: architecture handoff

The objective is genuine fixed-step gameplay at **20, 30, 60 and 120 Hz**, with
rendering independently configurable. Pass 2 adds opt-in canonical replay and
instrumentation while retaining normal gameplay arithmetic and submodule revisions.
High-rate simulation is **not implemented**. Rendering interpolation does not meet it.

The branch is `mod/native-simulation-rates`, based on upstream develop
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`. The fork is
[ChocoChopin/Shipwright](https://github.com/ChocoChopin/Shipwright).

Pass 2's bounded implementation and validation are complete. The final corpus
passes 12 fixtures in three fresh processes each, and the presentation/trace
matrix passes 21 more runs with exact semantic snapshots. The Python suite passes
60 tests and the executable passes 36 native CLI checks. Negative-control,
coupling-analysis, diagnostic-compatibility and ordinary-startup gates also pass.
The historical corpus 02 remains failed: it completed 35 of 36 engine runs and
retained an unresolved premeasurement startup access violation. The later clean
corpus does not establish that its cause is fixed.
Interactive gameplay has not been validated. See [BASELINE.md](BASELINE.md) for
the original build/extraction receipt, [TESTING.md](TESTING.md) for the current
contract and evidence status, and [PASS2.md](PASS2.md) for final source/artifact
identities and acceptance accounting.

## Reading and working order

| Document | Purpose |
|---|---|
| [BASELINE.md](BASELINE.md) | Exact checkout, Windows build, ROM and launch evidence; reproduction commands |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source call graph, R_UPDATE_RATE audit, clock/render/input/audio design |
| [TIMING_SEMANTICS.md](TIMING_SEMANTICS.md) | Units, A-I taxonomy, equations, canonical behavior and divergence policy |
| [TESTING.md](TESTING.md) | Implemented replay contract, exact commands, fixtures, comparisons and coverage limits |
| [PASS2.md](PASS2.md) | Pass 2 artifact identities, completed gates and unresolved reliability evidence |
| [RNG_AUDIO_AUDIT.md](RNG_AUDIO_AUDIT.md) | Controlled RNG streams, audio sink, draw/audio authority and observation limits |
| [EXECPLAN.md](EXECPLAN.md) | Ordered phases with prerequisites, tests, gates, failure modes and reasoning effort |
| [CONVERSION_LEDGER.md](CONVERSION_LEDGER.md) | Curated subsystem findings, claim workflow and machine-readable coverage |
| [KNOWN_DIVERGENCES.md](KNOWN_DIVERGENCES.md) | Expected resolution consequences and unresolved design choices |

Repository [AGENTS.md](../../AGENTS.md) preserves safety, scope and Windows workspace
rules for future agents without this conversation. Inventory files contain only
non-proprietary source metadata, never extracted ROM content.

## Architectural decision

Use one rate-parametric temporal context and integer 120-unit/second clock, with
fixed step advances 6/4/2/1. Preserve the exact legacy 20-Hz path. Inventory each
field's units and distinguish continuous rates, durations, impulses, decision
opportunities and authored frame identity. The four rates share authoritative
comparison endpoints every 100 ms. Higher precision must perform real world/input
updates, not replay interpolated pictures or hold all logic at 20 Hz.

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

The current pass implements the bounded Phase 1 replay envelope and coupling
instrumentation; it does not satisfy every scenario in the broader execution
plan. The next authority change is a bounded HUD/message extraction, conditional
on a stronger reliability baseline and the relevant fixture dependency closure.
Keep 20-Hz results exact and leave actor pose, collision, enemy draw RNG and audio
at their current seams in that first extraction. Use Ultra reasoning for boundary
design and independent review, High for scoped implementation. No higher-rate or
bulk actor conversion is admitted by this checkpoint.
