# Deterministic simulation testing contract and design

## Replay storage lifetime

Before each replay, the runner requires 1 GiB free for evidence; copy fallback
also budgets the complete asset size before copying. Successful runs release
only their staged asset links/copies after validation against the preserved
originals. All logs, snapshots, traces and receipts remain. Engine/fixture
validation failures retain staged inputs; later cross-run mismatches retain
outputs and the canonical original assets. No original asset, save, source or
prior output is removed.
Invocation receipts list `released_staged_assets`. The controller-only storage
tests cover space refusal before launch, copied/linked input release, original
preservation, changed-input retention and failed-process retention.

The first reviewed-build startup campaign stopped before process 96 because
disk space was exhausted: 95 successful starts, one prelaunch infrastructure
failure, 84 unattempted entries. Its original receipt remains incomplete and is
never overwritten. Complete only the outstanding per-cohort counts in fresh
campaigns, retain the failed attempt separately, and bind each continuation to
the same executable, assets and fixture as its prior successful reference.

## Pass 3B direct helper gate (accepted)

The first helper coverage record must be initialized as a JSON object before
value() reads its counters. Run the native regression against the production
routine with `python -B scripts/native-simulation/validate_purity_coverage.py
--output <fresh-ignored-path>`. It uses the baseline Windows compiler, the installed
nlohmann header and repository-local temp/output directories, and returns normal
failure codes. Twenty checks and six focused repaired engine runs pass; see
PASS3B.md for the retained crash-only receipt and the complete final acceptance.

`run_corpus.py run --verify-presentation-purity` forwards the native option before
asset extraction. It requires native test mode. Each admitted packet is emitted
normally and twice more into independent command/paint buffers; extra lists are
never submitted to the renderer or interpolation recorder. Every call is checked
against live PlayState, SaveContext, registers, segments, controller, HUD/static
and message/static storage, DoAction aliases, complete semantic state, draw
observations and logical event sequence. Same-process bytes/pointers never enter
portable hashes. Ordered Gfx words and private paint emissions must match.

The adapter also compares every initialized packet byte after each call, including
fields that do not affect current visible paint. These same-process bytes never
enter portable hashes. `purity.json` attests `packet_bytes_checked`; the launcher
hashes the completed receipt in `invocation.json`, and the analyzer verifies it.

`purity.json` records two extra calls, setup/measured and visible/invisible
coverage, admission negative counts, fixture and first failure. The runner binds
source/executable identities. `analyze_purity.py --corpus <path> --require-both`
requires both helpers across the HUD/message corpus and rejects admitted message
calls in 0x305F. Canonical comparison remains a separate acceptance gate.

`validate_purity_control.py --output <fresh-ignored-path>` enables the explicit
test-only mutation control. It changes one message timer after a scratch call
and requires detection plus normal exit with failure code 2. It does not cause
or reproduce a native crash. Unexpected native exceptions still invoke the
crash-stop policy. The control requires native test plus purity mode. The final
control passes with normal exit 2. All 120 final canonical and 93 presentation-matrix
runs pass, with 192 successful startup/gravity runs and one separately retained
prelaunch disk-full failure. The 121 Python, 40 native CLI, 31 TLUT and 20 coverage
counter checks also pass. PASS3B.md lists exact receipts and source identities.

Strict draw analysis binds the exact invocation to the corpus purity declaration;
missing/extra flags remain errors. Both modes retain every existing phase, input,
state and paint check. Invisible messages retain legacy setup commands. All
matrices enable purity; trace-off cases still compare live state and packet bytes.

Historical Pass 2 status: Pass 2's bounded Phase 1 implementation and validation are complete,
2026-10-03. The native replay harness passes the 36-run canonical corpus and the
21-run presentation/trace matrix. The Python suite passes 60 tests (29 runner,
22 math and 9 acceptance checks), and the executable passes all 36 native CLI
checks. Negative-control, coupling-analysis, diagnostic-compatibility and ordinary
startup gates also pass, with exact evidence and limits below.
Corpus 02 retained a startup access violation before measurement, unresolved at
the Pass 2 checkpoint. Pass 3A later reproduced a resource-backed TLUT over-read;
the bounded staging correction passed its native helper and rebuilt-engine gates,
including 192 fresh startup trials on the fixed executable. PASS3A.md separates
these results from the earlier completed fixtures and retained failures.
Native 30/60/120-Hz simulation remains unavailable.
Pass 3A's fixed executable also passes 37 native CLI checks, the retained original
36-run corpus and 21-run matrix, and the new 24-run draw corpus plus 72-run matrix.
At the Pass 3A checkpoint the tooling suite passed 105 tests. PASS3A.md retains
that checkpoint's acceptance receipt; PASS3B.md owns current acceptance, and
the Pass 2 counts below remain historical.
`BASELINE.md` records the untouched-engine receipt. Sections 1-7 retain the
original reconnaissance and broader testing design; the current implemented
boundary below is narrower than that future acceptance corpus. Historical line
anchors in those sections refer to the baseline revision.

## 0. Implemented canonical replay interface

`soh/soh/NativeSimulationTest.cpp`, `NativeSimulationTestInput.cpp` and
`NativeSimulationTestBoot.c` implement developer-only fixture execution. Without
`--native-sim-test` the test hooks return immediately and ordinary boot, input,
audio scheduling and gameplay arithmetic remain on their original paths.

The executable accepts:

```text
soh.exe --native-sim-test <fixture.json> --output <fresh-directory> [--trace]
```

Use the Python runner below for process isolation, asset identities, timeouts,
hash generation and comparisons. A direct invocation must also use an isolated
working directory containing the locally generated archives. Fixtures and source
contain only recipes and input metadata; no extracted Nintendo data is committed.

### Boot, steps and determinism envelope

The graph loop selects its existing Play overlay for test mode. The boot recipe
calls `SaveContext_Init`, SRAM global-option initialization and the existing
debug-save initializer, with explicit child/adult age, entrance, English language,
normal game mode and noon. Age is set before debug-save initialization because
it selects equipment. The recipe follows scene-select semantics and invokes the
ordinary load-game hook. It does not restore a heap image or load a player save.

Each process starts with fresh static/global state. `setup_ticks` complete legacy
transactions run with neutral input. Optional `initial_player.pos`/`yaw`, HUD
timer, message, loaded-bank Ice Keese and ocarina-memory recipes are applied once
at the measurement boundary; these construct the fixture without changing gameplay
timing.
Snapshot 0 records the resulting initial state. Then exactly `ticks` transactions
produce snapshots 1 through N. `time_q = tick * 6` uses the future 120-unit/second
clock representation; it does not execute a hidden 120-Hz world loop.

`RunFrame` consumes input, then `Graph_Update` runs update, complete CPU draw/pose
generation and `Audio_Update` control. `Graph_ProcessGfxCommands` runs the test
mixer blocks before GPU presentation, followed by savestate-request work. The
completion snapshot is after that complete transaction. In admitted 20-Hz steps,
the mixer runs three 528-sample blocks before `RunCommands`; CPU draw/pose generation
has already completed. This fixed test sink does not copy the device-buffer
feedback policy of ordinary audio. Test mode requires `R_UPDATE_RATE == 3` and
exactly one `Play_Update` and `Play_Draw`
per measured transaction. A fixture entering an unsupported pause/transition
cadence fails; this initial runner does not reinterpret authentic 30/60-Hz menu
states as 20-Hz world updates. Rendering may still pace execution, but wall time
and sleeps do not decide the number of authoritative steps.

The runner creates a fresh repository-local working directory for every process,
hardlinks or copies the local archives and controller database, and sets that
process's TEMP/TMP inside its run directory. No existing config is copied.
`Configure` pins interpolation FPS, disables refresh matching, mouse gameplay,
alternate-asset hotkeys, savestates, easy frame advance, host-time synchronization,
seasonal snow and remote Crowd Control/Sail/Anchor integrations. Raw F-key/TAB
shortcuts are suppressed in `Graph_StartFrame`; window events still pump.
This is a controlled default configuration, not coverage of arbitrary mods,
network interactions, user configuration or physical-controller mapping.

### Input schema and consuming seam

Schema 1 input entries provide integer `time_num`, positive integer `time_den`,
strictly increasing `sequence`, `buttons`, `stick_x` and `stick_y`. Optional
`port` defaults to 0; the native provider accepts ports 0-3 and unsigned 32-bit
`buttons`. Right-stick axes default to zero and `connected` defaults to true.
Each entry is a full normalized pad state for its
port. Numerators and denominators are bounded to one billion; both sticks use
signed 8-bit coordinates. Disconnected entries must be neutral. Gyro fields are
explicitly rejected in this version. Core validation also rejects invalid schema,
non-20 rate, malformed integer fields, invalid age/entrance/tick limits, non-finite
positions and out-of-range yaw before boot.

At the beginning of time `t`, replay consumes every ordered event with rational
timestamp `<= t`. Rational comparisons retain sub-tick input times; no early
rounding to a 20-Hz frame occurs. `PadMgr_HandleRetraceMsg` selects the replay
provider before hardware polling. Each due transition passes through the original
`PadMgr_ProcessInputs`; a step with no transitions samples its held state once.
`GameState_ReqPadData` uses the existing consuming request to copy and clear edges.
Consequently a short press/release between boundaries sets both edge masks while
the final held mask is clear, and holding a button does not repeat its press edge.
Verbose traces record original time/sequence, application time and full event
state. The rumble retrace callback still runs once per transaction; physical
rumble, device queries and mouse sampling are absent from replay.

This retains existing PadMgr conventions, including its 16-bit cast for button
edge accumulation despite the project's 32-bit held-button field. It does not
repair or redefine extended-button edges, right-stick accumulated deltas or
disconnection handling. Initial corpus assertions focus on port 0 and ordinary
N64 button bits. The Python runner currently restricts input to port 0 and 16-bit
buttons, a narrower contract than the native provider. Multiport and extended-button
acceptance are not proved by the port-0 corpus. Device mapping/deadzones and
simulated device lag are upstream of this seam and outside its coverage.

### Observation, hashes and first divergence

Snapshots serialize named world/save, Player, camera, collision, actor-base,
input, RNG and audio fields. Floats carry exact IEEE-754 binary32 bits plus a
readable round-tripping decimal. Actor identities use scene/spawn ordinals;
execution/list order is recorded without changing it. No pointer addresses,
padding or raw savestate bytes enter hashes. Player action symbols are mapped;
ordinary actors currently expose base fields, not complete family-specific
action/timer serializers. Snapshot coverage is therefore explicitly partial.

CPU drawing remains authoritative. Optional traces bracket `Play_Update`,
`Play_Draw`, `Actor_DrawAll`, `Interface_Draw` and `Message_Draw`, as well as
input, presentation and audio. Before/after semantic differences identify writes
in those covered fields. Player pose/collider state, actor culling/lifecycle,
HUD timers and message progression stay at their original call sites. A trace
cannot prove absence of mutations in fields the serializer does not yet cover.

Gameplay/explicit-state RNG arithmetic and call order are retained and observed
with phase, actor, generator-site, state and count. Audio uses synchronous legacy
block grouping, a fixed 528-sample test sink and deterministic counter inputs;
the ordinary hardware-buffer feedback path remains the default outside replay.
Ocarina memory setup exercises its real audio-RNG dependency. This does not prove
all audio/ocarina scenarios or all enhancement RNG streams deterministic.

The engine writes `snapshots.jsonl`, optional `trace.jsonl`, and a completion
`result.json`. Missing completion, malformed/non-finite fields, wrong counts or
noncontiguous tick/time labels fail validation. `run_corpus.py` adds SHA-256 for
each whole snapshot and domain, plus a sequence hash in `hashes.json`. Strict
comparisons use exact values/bits and report the first differing tick and field;
there are no broad float tolerances. `--trace` also compares complete trace order.
Presentation variants use `--allow-presentation-difference`, which permits only
fixture `presentation_fps` and configuration `interpolation_fps` to differ while
comparing every serialized semantic field exactly. It cannot be combined with
`--trace`: presentation-count events intentionally differ across those runs.
The corpus receipt records executable/source/submodules, local archive hashes,
fixture identities and per-process results; the engine completion also records
the resolved fixture and pinned configuration values. Coverage assertions establish
that movement, turning, contact or other named behavior actually occurred;
repeat equality alone is insufficient.

### Reproducible commands

From the repository root, with the selected Python 3.12 interpreter:

```powershell
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' -B -m unittest discover -s scripts/native-simulation -p 'test_*.py' -v
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' scripts/native-simulation/baseline.py build --config Release --jobs 4
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' -B scripts/native-simulation/run_corpus.py run --output build/native-simulation-runs/canonical20 --repeats 3 --trace
```

The default corpus is `scripts/native-simulation/fixtures/*.json`. `--fixture`
can select a bounded subset; `--exe` and `--assets` select explicit local inputs.
The output root must be fresh and ignored. Completed output directories can be
compared or used for the negative control:

```powershell
python -B scripts/native-simulation/run_corpus.py compare <reference-output> <candidate-output> --trace
python -B scripts/native-simulation/run_corpus.py compare <20-fps-output> <60-fps-output> --allow-presentation-difference
python -B scripts/native-simulation/run_corpus.py negative-test <completed-output> --output build/native-simulation-runs/negative-control --tick 3
python -B scripts/native-simulation/validate_native_cli.py --output build/native-simulation-runs/native-cli-validation
```

The negative control changes one float bit and its matching decimal in a copy of
real diagnostic output, requires changed hashes, an exact tick/field report and
comparison exit 1, restores the original rows, then requires exit 0. It never
leaves a deliberate perturbation in game source or game state. Runner exit codes
are 0 pass, 1 mismatch/coverage failure and 2 invalid fixture/infrastructure
failure; the engine uses 2 for a rejected/incomplete test and 0 only on completion.
Measured build, repeatability and negative-control outcomes must be added to the
receipt after execution; the commands above are not those outcomes.

`validate_native_cli.py` launches the actual executable on malformed fixtures in
empty working directories, without archives. It requires exit 2 and an
initialization-phase failure before snapshots, config or saves are created.
Cases cover typed integer limits, unsupported rate/schema, input ordering and
axes/connection/gyro constraints, player setup, and preservation of an existing
result or trace file. The trace-only case supplies a valid fixture and `--trace`
so a missing freshness guard cannot hide behind fixture rejection. This
complements the Python parser tests by exercising the native entry point directly.

For a separate Windows ordinary-startup smoke, run:

```powershell
python -B scripts/native-simulation/smoke_default.py --output build/native-simulation-runs/default-smoke
```

This launches the same executable with no replay arguments, fresh local assets,
and an isolated config setting integer `gSettings.Volume.Master` to zero. It
observes the owned game window for 12 seconds, requests a graceful close, and
requires exit 0, a scene-initialization log entry, and unchanged executable/assets.
Only its child process may be killed if the close times out; that outcome fails.
`smoke.json` records window titles, log/config hashes, and process evidence.
This is startup coverage, not interactive gameplay acceptance or visual inspection.

### Pass 3A draw-state observation and startup stress

The optional fixture boolean `observe_draw_state` adds a named `draw_state`
snapshot domain. Its absence retains the Pass 2 snapshot and event schema exactly.
The eight recipes in `fixtures/draw-state/` are separate from the original twelve
top-level fixtures. They observe legacy authority before extraction:

| Fixture | Bounded behavior |
|---|---|
| `hud-zero` | One-second main countdown: preview, slide, zero, STOP and OFF |
| `hud-zero-input` | B edges around STOP/OFF and Player item gating |
| `hud-warning` | Twelve-second countdown and ten-second/urgent warnings |
| `hud-old-digit` | Seventy-two-second countdown and previous drawn ones-digit warning dependency |
| `hud-message-gate` | Main countdown held by a two-page message, then resumed |
| `message-pages-natural` | English 0x1043 opening, natural character progression, A page advance and close |
| `message-pages-skip` | The same message with B acceleration and A close |
| `message-fade-observed` | Existing 0x305F quicktext/fade path, retained as unsupported extraction fallback |

`hud-zero-input` uses Kokiri Forest: the earlier Link's House recipe disabled B
item use and could not exercise the intended Player behavior. Three revised
probe runs and strict observer checks pass; the retained evidence is
`build/native-simulation-hud-input-probe-01` and
`build/pass3a-design/hud-input-observe-review.json`. This focused pre-fix candidate
result does not accept the entire draw corpus or the corrected renderer.

The completed corrected-renderer acceptance is now
`build/native-simulation-draw-state-02` (24 runs plus strict draw/coupling analysis)
and `build/native-simulation-draw-presentation-01` (72 runs). Use the former as
the immutable Pass 3A draw-state reference for the next extraction. Its exact
source/executable identity and all retained failed candidates are in PASS3A.md.

The serializer records hidden HUD divisors, state counters, retained digits,
eligibility inputs, preamble state and main/sub-timer coordinates. Message
observations include cursor/delay/mode state, decoded-page lifetime, text settings,
DoAction dependencies and bounded buffer fingerprints. Buffer contents and resource
payloads are not serialized. Actual timer clock/digit emissions and ordered text
glyph/icon metadata are observed adjacent to their existing display-list calls.
The paint fields therefore catch omission, stale geometry/color, premature glyphs
and incorrect icon selection without changing event fingerprints. No authority is
moved by these diagnostics. Unsupported text paths explicitly mark the ASCII
paint observation incomplete.

`analyze_draw_state.py` reconstructs every measured phase from the initial
snapshot and the full trace, checks each transaction against its completion
snapshot, and checks source-derived timer, input, warning, text and paint behavior.
It requires exactly the eight declared fixtures and three completed matching
repeats each. Equality alone cannot bless a missing glyph or shifted transition.
The analyzer's corruption tests must remain green when its acceptance logic changes.

Run these commands with the selected Python interpreter and fresh output paths:

```powershell
$drawFixtures = Get-ChildItem scripts/native-simulation/fixtures/draw-state/*.json | Sort-Object Name
$drawArgs = @()
foreach ($fixture in $drawFixtures) { $drawArgs += @('--fixture', $fixture.FullName) }
python -B scripts/native-simulation/run_corpus.py run --output build/pass3a-draw-corpus --repeats 3 --trace @drawArgs
python -B scripts/native-simulation/analyze_draw_state.py --corpus build/pass3a-draw-corpus
$drawMatrixArgs = @()
foreach ($fixture in $drawFixtures) {
    $drawMatrixArgs += @('--fixture', $fixture.FullName, '--trace-disabled-fixture', $fixture.FullName)
}
python -B scripts/native-simulation/presentation_matrix.py --reference build/pass3a-draw-corpus --output build/pass3a-draw-matrix --fail-fast @drawMatrixArgs
```

The draw matrix is 24 cases / 72 fresh processes: all eight fixtures at 60 and
120 presentation FPS, plus all eight with tracing disabled at 20 FPS, three
repeats each. The preceding corpus covers 20 FPS with tracing enabled. The
original presentation matrix remains separately required. It proves exact CPU
state and requested display-list replay counts; it does **not** repeat C drawing
helpers or attest GPU completion. The next High extraction pass must add a direct
repeated-presentation-helper test with unchanged live state/events/RNG and equal
paint output, as specified in ARCHITECTURE.md.

`startup_stress.py` writes the complete scheduled attempt list before launching
anything and retains each process receipt, partial output, crash/resource log
evidence, executable/asset/fixture identities, exit status and diagnostic host
duration. Scene and measurement progress require positive durable evidence;
absence of buffered output is recorded as unknown. Failed starts are never
replaced or omitted. The default startup matrix is two scenes x three presentation
rates x trace on/off, round-robin. Each process has one setup and one measured
transaction. The separate `full-gravity` profile uses the original full fixture.

```powershell
python -B scripts/native-simulation/startup_stress.py --exe x64/Release/soh.exe --source-commit <compiled-runtime-commit> --expected-exe-sha256 <verified-sha256> --output build/pass3a-startup --starts-per-cohort 15 --timeout 45 --fail-fast
python -B scripts/native-simulation/startup_stress.py --exe x64/Release/soh.exe --source-commit <compiled-runtime-commit> --expected-exe-sha256 <verified-sha256> --output build/pass3a-gravity --profile full-gravity --starts-per-cohort 12 --timeout 45 --fail-fast
```

Stop on any new native crash, retain its logs/partial outputs, and report it.
Wait for user direction before debugging or reproduction; do not deliberately
induce a crash. The matrix/stress `--fail-fast` option retains the first failure
and stops before another launch; corpus runs must omit `--keep-going`. Mocked
controller tests verify this behavior without starting the game. The commands
above are for later source changes, not a reason to repeat completed campaigns.

The source argument declares compiled provenance; it must be bound to the actual
build receipt and executable hash, not inferred from today's checkout HEAD or the
embedded configure-time version stamp. Always specify the executable path: Pass
3A found pre-scene DirectX stalls when identical binaries were launched from
copied `build/` locations, while the original `x64/Release/soh.exe` path completed
the stress campaign. The association is measured; its cause is unresolved. Keep
the immutable reference copies, but run a reference only at a verified working
location with its hash checked before/after, sequentially with candidate work.
Do not overwrite the candidate while it is executing. These stalls are separate
from the reproduced TLUT source over-read. [PASS3A.md](PASS3A.md) owns campaign
denominators and final receipts, including failed and interrupted attempts.

The preserved-reference campaigns completed 180 short starts and 12 full gravity
replays. Keep these 192 successes separate from the five copied-path timeouts and
from the later candidate failure. `native-simulation-corpus-05/corpus_result.json`
remains `infrastructure-error`: 34 runs completed of 35 attempted, gravity run 002
exited with `0xc0000005`, and gravity run 003 was not attempted. Its crash log
captures `gLinkChildSwordTLUT` at `[0x0000025956AFDE50, 0x0000025956AFDF28)`,
216 image bytes inside a retained 296-byte file buffer. The 512-byte TLUT request
ends at `0x0000025956AFE050`, 296 bytes beyond owned storage.

The reviewed fix in `ChocoChopin/libultraship`, branch `codex/tlut-source-bounds`,
is committed as `c6bbb8c328938c115f4a1cbeaca3d00a4502269d`. It bounds
resource-backed staging copies by image and owned-buffer extents, preserves
defined bytes and partial CI4 writes, and zeroes only the requested unavailable
tail. Raw-source fallback is unchanged. This is a defined missing-storage policy,
not byte-identical emulation of the original N64 transfer. The complete archive
scan found eight palette references, all with an audited CI8 jewel consumer whose
maximum index is 106; none observes the missing entries. Original ROM comparison
and exact ranges are recorded in PASS3A.md. Archives remain unchanged.

`build/native-simulation-tlut-bounds-02/tlut-bounds-result.json` records 31 passing
ordinary native helper checks, including bounded copies and cache-address tokens.
No guard-page or intentionally crashing test was performed. This is synthetic
memory-copy coverage. Separate rebuilt-game gates passed on executable SHA-256
`e3da61b9397dee5d2e1ef3e168927d9525742bb6626142291853e879ff6864f4`:
36 original canonical runs, 21 original matrix runs, 37 native CLI checks,
ordinary startup and 192 fresh startup trials. See PASS3A.md for the source/asset
binding and the new draw-state acceptance; retain failed candidate evidence.

For the next High pass, compare the full original corpus to
`build/native-simulation-corpus-04`, and compare the entire draw-state corpus to
the accepted Pass 3A draw corpus in PASS3A.md using `run --reference ... --trace`.
Run Python/native CLI, both matrices, the deliberate mismatch control, ordinary
startup, coupling/draw analysis and fresh-process stress after the change. A
completed semantic or event mismatch stops extraction acceptance; an incomplete
startup remains a retained reliability failure and cannot be blessed by rerunning.

The original-corpus gates use these existing commands (choose fresh paths):

```powershell
python -B scripts/native-simulation/run_corpus.py run --output build/high-canonical --reference build/native-simulation-corpus-04 --repeats 3 --trace
python -B scripts/native-simulation/analyze_couplings.py --corpus build/high-canonical
python -B scripts/native-simulation/presentation_matrix.py --reference build/high-canonical --output build/high-original-matrix --fail-fast
python -B scripts/native-simulation/run_corpus.py negative-test build/high-canonical/gravity-fall/run-001/output --output build/high-negative --tick 17
python -B scripts/native-simulation/validate_native_cli.py --output build/high-cli
python -B scripts/native-simulation/smoke_default.py --output build/high-default-smoke
```

Both presentation matrices must use a newly produced passing corpus from the
**same candidate executable**, since the tool requires matching executable bytes.
The preserved corpora are `run --reference` comparison inputs, not direct matrix
references for a newly built candidate. For the new draw-state run, use the
`$drawArgs` above and add `--reference <accepted-Pass3A-draw-corpus>`; then run its
analyzer and matrix with that newly produced corpus as their reference.

### Repaired integration checkpoint

`build/native-simulation-coupling-probe-01/corpus_result.json` records three exact
fresh-process repeats of `message-draw` and `draw-rng-keese`, including complete
verbose trace equality and behavioral assertions. The executable SHA-256 is
`c82c484661417a924e6dcedde498b41e7ba3bf3c4b8bf4c84f1e5887b7a384dc`.
`build/native-simulation-cli-validation-02/native-validation.json` records 36/36
native rejection/preservation checks on that executable. The runner also rejects
unmapped Player actions and missing/duplicate actor identities; base actor state
is deliberately partial, and an unmapped animation-resource key is not by itself
a failed run.

The startup-output repair preserves the runner's Windows stdout/stderr handles
instead of reopening a console in test mode, and delays JSON stream opening until
after Context logging initializes. Any earlier trace events are buffered in order.
The malformed initial outputs are preserved, never trimmed or accepted. The
precise operating-system handle reuse was inferred from the six startup log lines
and their initialization sites, not independently captured at the handle level.

### Pass 2 final validation references and historical reliability

[PASS2.md](PASS2.md) owns final gate accounting. The runtime source is
`68cd6a6520dcc9e3c4147fbc9dec62cd2f702417`; the final executable SHA-256 is
`e64ec79a23627f7d288ee0a6e96fbbc701ddefee1a8d403b7f1aa856459dadbf`.
Later tooling/documentation commits have separate identities. Results below apply
only to their recorded inputs and do not establish every scenario in the broader
Phase 1 design.

| Gate | Evidence under `build/` | Status |
|---|---|---|
| Native Release build | `native-simulation-evidence/build.log`, `build-invocation.json` | PASS, exit 0 |
| Full canonical corpus, three fresh repeats | `native-simulation-corpus-04/corpus_result.json` | PASS, 12 fixtures x 3 runs; 3,420 measured steps and 3,456 snapshots |
| Native CLI rejection/preservation | `native-simulation-cli-validation-04/native-validation.json` | PASS, 36/36 on final executable |
| Presentation/trace matrix | `native-simulation-presentation-01/matrix_result.json` | PASS, 7 cases x 3 runs; all semantic snapshots exact |
| Deliberate snapshot perturbation and restoration | `native-simulation-negative-04/negative-test.json` | PASS, one-bit mismatch at tick 17 in `actors[0].displacement.x`; restoration matches all 81 snapshots |
| Ordinary-mode startup and graceful close | `native-simulation-default-smoke-02/smoke.json` | PASS, one game window and scene initialization; graceful exit 0 without forced termination |
| Coupling/event analysis | `native-simulation-corpus-04/measured_couplings.json` | PASS, all 36 complete runs and expected repeats |
| Diagnostic-change compatibility | `native-simulation-diagnostic-comparison-01/comparison_result.json` | PASS, 12 completed run-001 pairs; 1,152 snapshots and 346,149 trace records exact |
| Revised Python tooling/math suite | `unittest discover` invocation above; PASS2.md | PASS, 60/60: 29 runner + 22 math + 9 acceptance |
| Inventory regeneration | `inventory-final-check/` | PASS, all four generated files byte-identical |

The matrix includes 18 runs at 60/120 presentation FPS and three runs with tracing
disabled. The presentation cases request three/six display-list submissions per
measured transaction respectively; this counter measures requests, not GPU
completion. The simulation remains 20 Hz, complete CPU draw still runs, and
semantic snapshots remain exact. The ordinary-mode smoke proves startup and
graceful shutdown, not interactive gameplay, visual quality or audible acceptance.

`native-simulation-corpus-02/corpus_result.json` is an `infrastructure-error`
receipt, with 35 of 36 engine runs completed. In `gravity-fall/run-003`, the native
crash log records access violation `0xc0000005` at
`Fast::gfx_load_tlut_handler_rdp` in the unchanged libultraship interpreter during
startup, before measurement. Its wrapper eventually timed out and its trace is
incomplete. The stack identifies the failing operation, not the root cause;
at that checkpoint there was no established fix or basis for calling this a
timing-conversion defect. The later Pass 3A reproduction and reviewed correction
are recorded above without replacing this historical receipt.
The failed run remains preserved and cannot be omitted from reliability claims.

Corpus 03 was intentionally stopped for a rebuild correcting diagnostic capacity;
its `interruption.json` preserves that decision. It is neither a complete
acceptance corpus nor evidence of a new gameplay failure. Debugger probes 01/02
stalled during driver initialization and captured no AV; those are separate
debugger-infrastructure failures. Further clean runs or non-reproducing startup
probes can strengthen a baseline without proving the original failure resolved.

Before extracting HUD/message authority, establish a stronger reliability baseline
and preserve the exact admitted executable plus these failed/interrupted receipts.
No result here admits higher-rate gameplay or replaces interactive acceptance.

### Initial measured draw/input observations

The first completed `animation-sword` replay under
`build/native-simulation-corpus-01/animation-sword/run-001/output` contains 101
snapshots for 100 measured steps. Three injected B presses enter the sword action
`Player_Action_808502D0`. Melee state is active at ticks 15-18, 42-45 and 72-75;
all three weapon geometry slots activate, and AT registration count reaches 3.
The `draw.actors.begin` to `draw.actors.end` differences show Player body/focus/
left-hand and weapon-vertex writes, with six AT-count changes inside actor draw.
The fixture now explicitly asserts active weapon geometry and AT registration,
in addition to changing animation frames and melee state. This proves covered
draw authority in that run; the final repeatability receipt must separately
establish the full corpus and final fixture identities.

All three completed `input-short-pulse` runs in the same initial corpus satisfy
the explicit edge assertions. Snapshot 2 has held=0 and pressed=released=32768
for the A press at 10 ms and release at 20 ms. Both original timestamps remain in
the trace and are applied at q=6 (the interval beginning at 50 ms). Snapshots
3/4/5 show Z press, held-without-repeated-press, and release respectively, from
events exactly at 100/200 ms. Snapshot labels are interval ends, while event
application labels are interval beginnings; this accounts for the tick offset.
The initial corpus nevertheless failed its strict output-integrity gate: the
third pulse run's trace and a later idle run's snapshots began with process-log
text instead of JSON. Those runs are infrastructure failures, despite passing
engine completion and the pulse snapshots' behavior assertions. Preserve the
initial evidence and rerun after correcting Windows console/output handling;
these partial observations are not a complete repeatability acceptance receipt.

## 1. Original baseline findings and available seams

| Existing facility | Source evidence | What it establishes; remaining work |
|---|---|---|
| Native controller read | `libultraship/src/libultraship/libultra/os.cpp:29`, `libultraship/src/libultraship/controller/controldeck/ControlDeck.cpp:56` | `osContGetReadData` zeroes pads, pumps SDL and reads mapped controllers. A future replay provider can supply normalized pads here; no timeline record/replay implementation was found in the controller path. |
| Mapping, lag, extra axes | `libultraship/src/libultraship/controller/controldevice/controller/Controller.cpp:26` | Buttons, both sticks, gyro, and a six-entry simulated-input-lag buffer are upstream of `OSContPad`. Record this complete normalized representation; pin/disable simulated lag. |
| Input edge accumulation | `soh/src/code/padmgr.c:290`, `:370`; `soh/src/code/game.c:252` | PadMgr accumulates press/release edges; `RequestPadData(mode=1)` copies and clears them. A held button must not become a new press on each substep. Right-stick/gyro handling and accumulated deltas also require coverage. |
| Native frame envelope | `soh/src/code/graph.c:476` | `Graph_StartFrame`, `PadMgr_ThreadEntry`, `Graph_Update`, graphics processing and savestate requests establish actual ordering. Do not infer an authoritative update boundary from a render callback name. |
| Hooks | `soh/soh/Enhancements/game-interactor/GameInteractor_HookTable.h:13`, `:28`, `:40`; `soh/src/code/game.c:357`; `soh/src/code/z_actor.c:2698`; `soh/src/overlays/actors/ovl_player_actor/z_player.c:12322` | Game-frame, player, actor, scene and draw hooks can instrument lifecycle and traces. `OnGameFrameUpdate` precedes `gameState->frames++` and is not a universal post-simulation fence. Hook coverage/order must be pinned before using it as a snapshot contract. |
| Frame advance | `soh/src/code/z_frame_advance.c:16`, `soh/src/code/z_play.c:698` | A CVar/controller gate exists for Play update. It does not prove a paused, deterministic whole-process single-step API; rendering, audio, menus and globals have separate work. |
| Scene/debug start | `soh/soh/Enhancements/Warping.cpp:52`, `soh/src/overlays/gamestates/ovl_select/z_select.c`; `soh/soh/Enhancements/debugconsole.cpp:402`, `:442`, `:1516` | Boot-to-warp, debug save, scene select, `entrance`, `reload`, `save_state`/`load_state` are useful entry points. These are game facilities, not an existing unattended CLI fixture runner. |
| Save files | `soh/soh/SaveManager.cpp:1371`, `:2093`, `:2268`; `soh/src/code/z_sram.c:83` | Versioned save fields reconstruct persistent progress; load can remap entrances. Save files do not serialize the running actor world. |
| Runtime savestates | `soh/soh/Enhancements/savestates.cpp:86`, `:247`, `:425`, `:455` | Whole heaps, selected globals and manually enumerated overlay statics are copied under the audio mutex. Some audio pointers are explicitly relocated. State slots live in an in-process map. This is a useful diagnostic tool, not an established portable or complete deterministic snapshot format. |
| Tests | `libultraship/CMakeLists.txt:84`; `libultraship/tests/CMakeLists.txt:3`, `:11`, `:49` | `LUS_BUILD_TESTS=ON` enables a `lus_tests` GoogleTest target with CTest discovery. Existing tests cover library utilities/resources, not gameplay replay. No authoritative gameplay regression target was found in top-level/SoH CMake. |
| CLI/window | `soh/soh/OTRGlobals.cpp:309`, `:410`, `:1521` | The original baseline initializes a window and passes arguments into extraction; it has no unattended replay contract. Pass 2 adds the `--native-sim-test` interface in section 0. `--simulation-rate`, `--replay` and `--headless` remain unimplemented proposals. |

### A savestate is not yet a reset proof

`SaveStateInfo::rngSeed` is declared at `savestates.cpp:94` but is never saved or
loaded in that file. The core RNG state `sRandInt` is a translation-unit static
in `soh/src/code/code_800FD970.c:4`; copying the system heap does not capture it.
The manually enumerated static-state functions cover selected modules, not all
global/static/C++ state. `Audio_NextRandom` also owns a function-local `audRand`
(`soh/src/code/code_800E4FE0.c:850`). Controller history, C RNG, randomizer state,
resource caches, hooks and interpolation history need independent audits.

Do not export raw savestate memory as a cross-process fixture: pointers,
allocator layout, function addresses and loaded resource ownership make such a
file build/process specific. Do not claim an in-process reset is equivalent until
restore/replay produces the same trace repeatedly, including scene transitions.

## 2. Reset and execution model

Prefer **sequential fresh processes**, one fixture/rate/repetition per process.
The current `gGameState`, `gPlayState`, `gSaveContext`, static actor/camera/audio
state, singleton `Ship::Context`/`OTRGlobals`, hooks and resource ownership make
two independent worlds inside one process an extensive refactor. Separate
processes can later run concurrently for throughput or visual comparison, but
sequential runs make initial determinism failures easier to isolate.

Proposed fixture boot sequence:

1. Verify executable SHA, source/submodule commits, compiler/options, fixture
   schema, archive hashes, CVars and enabled mods. Reject an unknown schema/rate
   rather than silently choosing 20 Hz. Keep a baseline executable separately.
2. Create an ignored repository-local run directory with a fresh configuration,
   explicit save fixture or semantic debug-save recipe, logs and output paths.
   Use read-only local references/copies for required asset packages. Verify
   resolved paths before launching and never use a person's normal save directory.
3. Disable network game interactions, crowdsourced changes, uncontrolled hooks,
   randomizer generation, wall-clock cosmetics and real controller input. Pin
   language, camera/graphics settings, enhancements, frame advance and lag.
   Deliberate mod/enhancement coverage becomes a separate fixture matrix later.
4. Initialize all relevant seeds/clocks **before the first affected draw** and
   before actor initialization. Loading a scene currently reseeds gameplay RNG;
   replacing only a post-load seed misses randomized initialization. Select a
   stable seed for every scene-init ordinal, not elapsed host time.
5. Enter the declared entrance/room/age/time-of-day through a deterministic
   fixture boot entry; await a bounded resource-ready condition independent of
   wall-clock duration. Run a declared setup sequence. Compare the resulting
   semantic initial-state digest against the fixture contract.
6. Set test elapsed time to zero at a documented quiescent step boundary. Do not
   reset only `gameplayFrames`: actor timers, audio, input, camera and global
   phases still need their declared initial values. Record the setup trace.
7. Inject input, run exact fixed steps, emit snapshots and event records, and
   terminate after an exact elapsed-time/condition limit. Write a result manifest
   atomically after completing output; absence of that manifest is a failed run.

Portable Windows builds resolve `Context::GetAppDirectoryPath` to `.` unless
`NON_PORTABLE` is defined (`libultraship/src/ship/Context.cpp:539-584`). `SHIP_HOME`
is only consulted in the Apple/Linux branches there. Thus choose an isolated
working directory on Windows and verify effective paths; do not assume setting
`SHIP_HOME` isolates a Windows run. A future explicit test-root argument is safer
than dependence on build-specific default paths. Keep `TEMP`/`TMP` process-local
inside the workspace if any harness subprocess uses temporary files.

## 3. Input timeline and comparison clock

The simulation clock uses signed/unsigned checked 64-bit quanta of 1/120 second.
Its fixed step sizes are 6, 4, 2, 1 quanta at 20, 30, 60, 120 Hz respectively.
Use a rational or integer-nanosecond **input** timestamp so tests can deliberately
place events between simulation boundaries. Never round timestamps to 20-Hz
frames before playback. Identical timeline data is used in every run.

Proposed event fields: `{time_num, time_den, sequence, port, buttons, stick_x,
stick_y, right_stick_x, right_stick_y, gyro_x_bits, gyro_y_bits, connected}`.
Times are seconds since the declared test origin; a stable sequence resolves
equal timestamps. Fixtures declare full pad-state changes, legal axis ranges and
gyro representation. Record hardware-mapped pads after mapping/deadzone/lag for
the normalized-pad replay layer; test device mappings independently.

At step start `t`, consume all input events with timestamp `<= t` that have not
already been consumed; never expose future input. The state drives `[t,t+dt)`;
the resulting snapshot is labeled `t+dt`. An event exactly at a boundary affects
the next interval. For an event between boundaries, the first possible response
is the next simulation boundary. Record both input time and application time.

Process all transitions in order, preserve final held state, and accumulate
press/release edges since the previous sample just as PadMgr does. A press and
release between samples can set both edge masks while leaving the button up.
Multiple taps in one interval cannot be fully represented by one legacy bitmask:
record their ordered event trace, use one edge bit per button for the legacy
consumer, and log this as an explicit input-resolution policy. Never synthesize
extra gameplay steps to replay those taps. Test short pulses separately from
held-input equivalence. Reusing one immutable sampled input for multiple steps
must not reissue edge presses. GUI focus and SDL sampling must not alter a replay.

Pass 2 implements the normalized-pad provider in `PadMgr_HandleRetraceMsg`, before
the hardware call. Every due transition passes through `PadMgr_ProcessInputs`,
preserving ordered edge accumulation rather than overwriting `OSContPad` once.
No libultraship submodule change was required. Section 0 defines the current
input limits; gyro, multiport corpus coverage and higher-rate playback are future work.

All four rates have real authoritative states together every **12 quanta =
100 ms**. At 50 ms, 30 Hz has no state. Do not interpolate a 30-Hz snapshot and
call it authoritative equivalence. Use 100-ms checkpoints for cross-rate field
comparisons, every step for same-rate tests, and exact event timestamps plus
bracketing intervals for events between checkpoints. Include cases whose input
changes occur at 1/20 second and at a non-lattice timestamp; otherwise the corpus
will conceal precisely the 30-Hz issue the architecture must solve.

Keep separate simulation tick, legacy cadence/epoch, world-active time,
menu/cutscene clocks, host elapsed time and presentation frame IDs. Pausing must
not advance frozen world deadlines, and the runner must not hang waiting for a
world-time limit during a menu pause. Bound total engine steps and host duration
independently. Test both controller time and world-active time in pause fixtures.

## 4. RNG, audio and drawing are prerequisites

The initial harness must report its determinism envelope rather than promise
that one seed makes all of Shipwright deterministic.

| Domain | Evidence | Required test policy |
|---|---|---|
| Gameplay LCG | `code_800FD970.c:12-40`, `z_play.c:521` | Fixed seed at the actual scene reseed seam; retain generator arithmetic and draw ordering at 20 Hz. Record state/count and caller/site identity without consuming a draw. |
| Explicit per-object LCG | `code_800FD970.c:47-81` | Capture every state used by the fixture; global seed alone does not set caller-owned RNGs. |
| Audio clocks/RNG | `code_800E4FE0.c:71`, `:850`; `audio_seqplayer.c:1363` | Audio RNG mixes `osGetCount`, task count and function-local state. Supply a deterministic test clock/initial state and deterministic audio task schedule; preserve call ordering. |
| Audio affects gameplay | `code_800EC960.c:2432` | Ocarina memory-game note generation calls `Audio_NextRandom`. Muting sound is not proof that audio can be skipped or omitted from all authoritative tests. |
| Host-buffer feedback | `OTRGlobals.cpp:1054-1073` | Audio task sample sizes vary with host buffer occupancy; define a deterministic sink/sample schedule for test mode before requiring audio-state equality. Trace queued sound/sequence/ocarina events independently of PCM/device output. |
| C/C++ enhancement RNG | `OTRGlobals.cpp:1579-1587`; `ShipUtils.cpp:125-148` | Pin time and `srand` for covered code. `ShipUtils::RandInit` sets state, but default `next32` can lazily reseed from `random_device` if `default_init` remains false: a seed call alone is not a validated default-stream reset. |
| Draw-side RNG and mutation | `ovl_En_Firefly/z_en_firefly.c:740-770`; `ovl_Boss_Goma/z_boss_goma.c:1987`, `:2104`, `:2134` | Limb callbacks consume gameplay RNG, update focus/collider state and spawn effects. Preserve CPU pose and side effects until intentionally separated and regression-tested. |
| UI draw-side time | `z_message_PAL.c:1599`; `z_parameter.c:6052` | Message and HUD draw paths update timers/state. A no-draw run is not currently equivalent to an ordinary update. |

Paths abbreviated in this table are under `soh/src/code`,
`soh/src/overlays/actors`, or `soh/soh` as identified by their filenames.

First prove repeated **20-Hz** runs with identical render/audio settings; then
prove identical 20-Hz authoritative traces across interpolation/presentation
settings. Initially use a small ordinary graphical window with full CPU game
draw semantics. A null GPU sink or hidden/minimal window is a later experiment:
verify the graphics backend actually supports it and compare traces against the
ordinary renderer. There is no evidenced existing headless switch.

Neither rendering every new simulation tick nor retaining old draw cadence is
automatically correct when draw performs gameplay work. Instrument dependencies,
move authoritative pose/collider work to a declared update phase, preserve
compatibility ordering, and classify residual cosmetic cadence/RNG questions.
Changing all random streams to per-actor generators would alter 20-Hz behavior;
do not use that as a quick determinism repair.

For random opportunities, preserving `p(dt)=1-(1-p20)^(20*dt)` preserves no-event
survival probability, **not** seeded trajectories or random draw order. Keep
legacy-cadence streams for literal compatibility tests; separately validate any
approved hazard model using fixed seed ensembles and predeclared distribution
tests. A single divergent random trajectory is neither automatic failure nor
automatic acceptance. Report the first draw-count/site divergence and the
affected gameplay outcome. RNG-consuming render variation remains a blocker to
claiming presentation-independent simulation.

## 5. Snapshot and trace contracts

Snapshots are **diagnostic serialization**, not automatically restorable saves.
Use an explicitly versioned schema with field names, fixed-width values,
canonical byte order, fixed actor order and no raw addresses/padding. Every
manifest includes source and executable hashes, submodule hashes, fixture/input
hashes, archive identity (hash/size only), compile flags, rate, render cadence,
audio policy, CVar/config digest, seeds and snapshot phase.

Give actors stable IDs assigned at spawn: scene epoch + monotonically increasing
spawn ordinal, plus actor type/params for diagnostics. Record parent/child IDs,
spawn reason and deterministic update/list order. Never sort by pointers or
silently match actors by nearest position after divergence. Actor list ordering
is itself part of the evidence. Map function/action pointers to explicit symbolic
IDs; if a state lacks a serializer, mark coverage incomplete rather than hashing
its address. Scene/static/dynamic collision identities use resource keys,
polygon indices and actor IDs, not pointer values.

Minimum snapshot domains:

| Domain | Fields |
|---|---|
| Clock/world | Exact elapsed quanta, tick, relevant legacy counters/phases, scene/room/entrance, transition, pause/menu/message/cutscene state, world time, switch/chest/event flags and state of deferred queues. |
| Player | World/previous position, velocity/speed/gravity, yaw/shape rotation, action/state flags, health/magic/ammo, targeting IDs, invulnerability/cooldown deadlines, grounded/wall/ceiling/water flags, floor polygon and moving-platform ID. |
| Actor | Stable ID/type/params/category/list order, action ID, position/velocity/rotation, animation, health, timers and callsite-specific state; explicit coverage list per actor family. |
| Animation | Resource ID, previous/current frame, speed, looping direction, morph weight/time, root motion, and emitted marker events. |
| Camera | Camera ID/mode/state, at/eye/up/FOV, smoothing state and target IDs. Camera differences can change culling and actor scheduling; do not automatically dismiss them as visual. |
| RNG/audio | Audited stream states/counters, site sequence, deterministic audio task phase, queued sound/sequence/ocarina events. Hardware buffer depth/PCM are separate diagnostics. |
| Input | Original event IDs/timestamps, applied held/pressed/released values, axes, consumed sequence range, connection state and input sampling phase. |

Initially capture one snapshot after the **complete legacy frame transaction**,
including gameplay-affecting draw work, at a fixed pre/post counter-increment
position. Optional checkpoints before/after actor update and pose/draw identify
the source of divergence. Phase 1 must name the actual C/C++ call sites and prove
that render interpolation does not execute the snapshot hook a second time.
After pose separation, migrate the schema boundary explicitly and regenerate
baselines only through reviewed schema/fixture updates.

Use a versioned canonical binary encoding for hashes, or a canonical JSON format
with ordered keys/arrays and floating values encoded by IEEE bit strings. Emit
readable decimal values alongside the bits. Use SHA-256 per domain and whole
snapshot; hashes detect a change, field-level diffs explain it. Reject non-finite
authoritative values. Do not mask NaNs, coerce negative zero or quantize floats in
the strict hash unless that normalization is an explicit field contract.
Cross-rate tolerance comparison is separate from strict hashing.

Proposed JSONL event fields:

```json
{"schema":1,"time_q":12,"tick":3,"phase":"collision","sequence":71,"actor":"scene0:spawn4","kind":"floor-contact","other":"scene0:spawn2","resource":"scene-floor","polygon":17,"scheduled_due_q":null,"input_sequence":6,"site":"Player.ResolveFloor"}
```

Add transition old/new action, collision candidates/results/normal/TOI or
bracket, damage attacker/victim/amount, timer arm/due/dispatch, animation marker,
RNG stream/site/draw/result, moving-platform delta and scene-load begin/end.
Preserve sequence ordering within equal timestamps. Trace all steps near the
first mismatch with a bounded ring buffer; avoid unbounded per-frame asset dumps.
Never place copyrighted asset payloads in committed fixtures or traces.

## 6. Comparator and classification

Run a same-rate repeatability gate before comparing rates. A flaky 20-vs-20
trace is test nondeterminism until investigated; it is not evidence of a temporal
conversion bug. Compare original 20-Hz code plus minimal deterministic test seams
with the candidate's compatibility path using the same seams and toolchain.
Test seams must be disabled by default; show the untouched default baseline still
builds/launches. Original upstream cannot be retroactively called deterministic
merely because a seed/clock-controlled reference variant is deterministic.

| Comparison | Acceptance |
|---|---|
| Reference 20 vs candidate 20 | Exact integer/bit/float-bit state and event sequence equality within audited coverage; identical RNG state/draw ordering. No float tolerance excuses a changed legacy operation order. |
| Repeat candidate at one rate | Exact audited domain hashes and event order. Repeating at a different wall-clock speed must not change the trace. |
| Same rate, different render cadence | Exact authoritative state and event sequence; visual output may differ. Preserve deterministic CPU pose work. |
| Pure mathematical primitives | Exact rational schedule/deadline/linear/affine results; explicit float error bounds for decay/smoothing, then production C/C++ float tests with the actual compiler. |
| Cross-rate continuous fields | Compare at common 100-ms boundaries using per-field `abs_error <= abs_tol + rel_tol*max(abs(a),abs(b))`; report maximum/RMS drift and first violation. Angles use wrap-aware distance. |
| Discrete outcomes | Inventory, damage amount, quest flags, spawn counts, transitions and animation event ordering remain exact unless a specific documented resolution/ambiguity exception applies. |
| Timed events | Compare exact intended deadlines and actual dispatch separately. Same absolute deadline must survive conversion; dispatch delay is bounded by the owning step/cadence and declared phase. |

Do **not** invent one permissive world-position epsilon. Each fixture sets
tolerances from primitive float error bounds and geometric/semantic contracts
before inspecting the candidate result. For initial isolated linear/ballistic
fixtures, measure compiler-specific rounding against analytic/reference answers,
document the bound and duration, and reject drift that exceeds it. Never widen
all tolerances until an actor passes. Keep tolerances versioned with a rationale.

For a monotone continuous threshold with the same starting state, finer sampling
can shift detection within the old bracketing interval; event phase/order can add
a documented dispatch step. A universal `<=50ms` allowance is invalid for
nonmonotone thresholds, different trajectories, repeated contacts or nonlinear
state transitions. Compare crossing brackets and causes before assigning class.

Every mismatch must enter one of these records:

1. **Class 1, mathematical conversion bug:** wrong deadline, linearized damping,
   duplicate animation event, lost input edge, dropped debt, wrong order/units,
   changed 20-Hz behavior. Fix and add a focused fixture.
2. **Class 2, expected resolution consequence:** finer sampling legitimately
   detects a ledge/collision/condition earlier, supported by a trace and approved
   mathematical semantics. Preserve evidence and a narrow expected range; do
   not reintroduce 20-Hz stepping to eliminate it.
3. **Class 3, semantic/design ambiguity:** competing valid extensions such as
   collision opportunity balance, RNG coupling, multi-tap coalescing or pose
   cadence. Document options/default/impact in `KNOWN_DIVERGENCES.md`; continue
   unrelated conversion. A gameplay-affecting undecided case cannot silently
   become a golden result.

Classify missing assets, driver/window failure, compiler failure, timeout,
permission error and nondeterministic test setup separately as infrastructure
failures. Never label them Class 1 gameplay regressions without supporting data.

## 7. Corpus and autonomous workflow

Start small and earn coverage in this order:

1. Clock/deadline/edge unit tests: all rates, exact 30-Hz alternation, long runs,
   pause/freeze/debt, zero/negative/sentinel timers and strict legacy dispatch.
2. Deterministic 20-Hz idle/flat movement fixture; then repeat and render-cadence
   matrices with an enemy/particle to expose draw-side RNG.
3. Isolated motion/turning/damping/animation/ballistics; input pulses, held attack,
   targeting/camera, morphs and animation marker crossings.
4. Floor/wall/ceiling collision, slopes, ledges, moving platforms, swimming,
   climbing, horse, hookshot, bombs/projectiles, doors and scene transitions.
5. Damage/invulnerability, AI opportunity rates, enemy/boss state loops, NPC and
   dialogue/cutscene time, pause/menu transitions, minigames including ocarina.
6. Save/reload/restart sequences, long sessions, randomized fixed-seed input
   fuzzing constrained to reachable scenarios, and structured input minimization.

Each conversion-ledger entry names its fixtures and trace fields. Claiming an
actor converted requires the shared primitive tests, strict 20-Hz compatibility,
repeatability at each supported rate, cross-rate contract and presentation
independence within that entry's coverage. A passing library unit test cannot
stand in for a gameplay fixture. Initial unconverted-rate runs may be expected
to diverge; mark coverage incomplete, not supported.

Proposed full multi-rate workflow (only its canonical subset is implemented):

```text
verify manifests -> build candidate -> unit tests
 -> reference20 three times -> candidate20 three times -> strict comparison
 -> candidate30 three times -> candidate60 three times -> candidate120 three times
 -> compare common times, events, temporal contracts
 -> render-cadence variants -> classify first divergence
 -> write result.json + readable diff + bounded event context
```

The Phase 1 subset of `scripts/native-simulation/run_corpus.py` now provides the
canonical-only entry point described in section 0. The multi-rate workflow above
remains future work. Its process lifecycle contract is: give each process a host timeout,
exact simulation limit and captured exit code; preserve failed run artifacts;
cancel only processes it created. Hash current inputs/config so resuming never
mistakes stale output for a fresh result. Proposed exit statuses distinguish
pass, regression, infrastructure failure and unresolved semantic exception.

The agent loop is: claim bounded subsystem -> inspect its ledger/semantics ->
patch -> compile -> focused fixtures plus required compatibility gates -> inspect
first divergence -> classify/fix -> rerun affected gates -> update ledger and
divergence records -> atomic commit. Never automatically bless new goldens,
delete failing evidence, broaden tolerances, skip 30 Hz or keep rerunning a known
permission error. Bisect/minimize the input sequence after deterministic replay
is established. Keep human experiential acceptance a separate late phase.

## 8. Commands that exist now

The dependency-free oracle uses `fractions.Fraction`, `math` and `unittest`.
From the repository root on Windows:

```powershell
python -B -m unittest discover -s scripts/native-simulation -p test_semantic_oracle.py -v
```

The original math checkpoint used Python 3.14.2 and passed **22 tests**. The final
Python 3.12 suite passes **60 tests: 29 runner, 22 math and 9 acceptance checks**
using the full discovery command in section 0. `-B` avoids bytecode cache artifacts. The
math tests create no temporary files and need no ROM, extraction,
graphics context or external Python packages. They verify rational scheduling,
causal timestamp dispatch, 30-Hz deadline jitter without drift, common time
boundaries, linear movement, the constant-acceleration fractional legacy map,
exponential decay, stationary-target smoothing and hazard survival. They also
demonstrate that naively scaled semi-implicit Euler misses canonical endpoints.

The affine oracle uses `s=20/r`, `v'=v+s*a`,
`x'=x+c*(s*v+a*s*(s+1)/2)` for **constant** acceleration/coefficient; `c=1` and
the legacy actor coefficient `c=3/2` are covered. It does not approve using that
formula across clamps, collision, nonlinear forces or state transitions. The
20-Hz reference executes its legacy algebraic branch, but Python exact-rational
results and coefficient bit identity do not prove the actual game's C/C++
float operation order or behavior.

Existing optional libultraship unit-test route, **source-supported but not run by
this document's author** (use the baseline's generator/options/environment):

```powershell
cmake -S . -B build/native-simulation-tests -DLUS_BUILD_TESTS=ON
cmake --build build/native-simulation-tests --config Release --target lus_tests
ctest --test-dir build/native-simulation-tests/libultraship/tests -C Release --output-on-failure
```

GoogleTest is fetched at version `v1.16.0`. Verify CTest discovery and actual build
directory layout; if necessary run the built `lus_tests.exe` directly. These
commands may require the same dependency/toolchain setup as the baseline and are
not a claim that gameplay, headless execution, or deterministic audio passed.
Any Python harness using pytest must set `--basetemp` inside the repository;
independent tempfile users additionally receive process-local `TEMP`/`TMP`.
