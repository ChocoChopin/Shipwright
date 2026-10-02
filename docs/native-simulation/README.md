# Native simulation rates: architecture handoff

The objective is genuine fixed-step gameplay at **20, 30, 60 and 120 Hz**, with
rendering independently configurable. This is the first architecture pass. Normal
gameplay source and submodule revisions remain unchanged; high-rate simulation
support is **not implemented**. Current rendering interpolation does not meet it.

The branch is `mod/native-simulation-rates`, based on upstream develop
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`. The fork is
[ChocoChopin/Shipwright](https://github.com/ChocoChopin/Shipwright).

Checkpoint: native Windows Release build and ROM extraction succeeded; a bounded
startup reached scene initialization and exited cleanly. Interactive gameplay has
not been validated. The 22 mathematical tests pass. See BASELINE.md for the
disk-capacity recovery, exact artifact identities and evidence. All work is saved
in local commits; the experimental branch has not been pushed.

## Reading and working order

| Document | Purpose |
|---|---|
| [BASELINE.md](BASELINE.md) | Exact checkout, Windows build, ROM and launch evidence; reproduction commands |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source call graph, R_UPDATE_RATE audit, clock/render/input/audio design |
| [TIMING_SEMANTICS.md](TIMING_SEMANTICS.md) | Units, A-I taxonomy, equations, canonical behavior and divergence policy |
| [TESTING.md](TESTING.md) | Existing seams/limitations; proposed deterministic replay, fixtures and comparisons |
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
contracts. None is a deterministic gameplay runner. TESTING.md specifies that
future runner and separates exact canonical regression from cross-rate invariants.

The next implementation pass is Phase 1 only: an opt-in fresh-process 20-Hz replay
fixture with timestamped input, controlled RNG/clocks, selected state snapshots and
first-divergence traces. Use High reasoning with Ultra review of reset/RNG/audio.
Resolve any baseline blocker first. Do not begin mass actor conversion.
