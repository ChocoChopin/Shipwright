# Pass 2: canonical replay acceptance receipt

This pass implements an automated, opt-in **20-Hz** oracle. It does not implement
30/60/120-Hz gameplay. Implementation and the bounded final validation are complete:
the final corpus and presentation matrix pass. The earlier intermittent graphics
startup failure remains unresolved and limits unattended-reliability claims.

## Source and artifact identity

- Branch: `mod/native-simulation-rates`, fork `ChocoChopin/Shipwright`.
- Replay engine checkpoint: `9c3cd0d23aeb2a195992c91da6c792f0375da171`.
- Final runtime source: `68cd6a6520dcc9e3c4147fbc9dec62cd2f702417`, including
  bounded replay-only crash reporting; pushed. Subsequent commits change tooling
  and documentation, not the compiled runtime.
- Upstream base: `9eafd15fe1382c5a41e881f1b6ea87345c797d18`.
- libultraship: `62e973aeb4a53ad4d22bb91e2d9373ecdfcd246c`; Torch:
  `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. Neither pointer changed.
- Native MSVC Release executable: `x64/Release/soh.exe`, 26,969,088 bytes,
  SHA-256 `e64ec79a23627f7d288ee0a6e96fbbc701ddefee1a8d403b7f1aa856459dadbf`.
- Toolchain: native Windows x64, Visual Studio 2022/v143, MSVC
  `19.44.35228.0`, Windows SDK `10.0.26100.0`, CMake `4.4.3`, Python `3.12.5`.
  The full baseline environment is recorded in [BASELINE.md](BASELINE.md).
- The build receipt records the preceding HEAD plus source changes/hashes;
  its runtime files are the exact files committed at `68cd6a652`. Corpus 04
  records that committed runtime source and executable bytes separately.
  The embedded version string is a configure-time stamp, not evidence of the
  complete compiled-source identity; use these out-of-band receipts instead.
- A copy and matching PDB are retained in ignored
  `build/native-simulation-reference/soh-pass2-final.exe` and
  `soh-pass2-final.pdb` for later differential checks and local crash diagnosis.
- Local NTSC US 1.0 ROM SHA-1: `ad69c91157f6705e8ab06c79fe08aad47bb57ba7`.
  The original ROM was not modified. No ROM, archive, save, binary or generated
  Nintendo content is committed or uploaded.
- `oot.o2r` SHA-256:
  `a058767a2f4f8f415b099a5c5189a9bf974a068f331b88131afea8df24e6a997`;
  `soh.o2r` SHA-256:
  `51c8b166b913fdc745901fc48a2ca6261e480e3e8c0715ae0f2d946adb982712`.

## Validation gates

| Gate | Result | Local evidence under `build/` |
|---|---|---|
| Native Release build | PASS, exit 0 | `native-simulation-evidence/build.log`, `build-invocation.json` |
| Python tooling/math tests | PASS, 60 tests: 29 runner + 22 math + 9 acceptance | Exact invocation below |
| Source inventory regeneration | PASS, all four generated files byte-identical | `inventory-final-check/` |
| Native malformed-input and preservation checks | PASS, 36/36 | `native-simulation-cli-validation-04/native-validation.json` |
| Historical message and Keese integration probe | PASS on preceding executable, 2 fixtures x 3 fresh processes | `native-simulation-coupling-probe-01/corpus_result.json` |
| Final canonical corpus | PASS, 12 fixtures x 3 fresh processes, exact snapshots and complete traces | `native-simulation-corpus-04/corpus_result.json` |
| Coupling and event-count analysis | PASS, all 36 complete outputs and expected repeats | `native-simulation-corpus-04/measured_couplings.json` |
| Diagnostic-change compatibility | PASS, 12 completed run pairs: all 1,152 snapshots and 346,149 trace records exact | `native-simulation-diagnostic-comparison-01/comparison_result.json` |
| Presentation/trace-independence matrix | PASS, all 7 cases x 3 fresh processes, exact canonical snapshots | `native-simulation-presentation-01/matrix_result.json` |
| Deliberate one-bit mismatch, then restoration | PASS, mismatch exit 1 at tick 17 and exact restoration exit 0 | `native-simulation-negative-04/negative-test.json` |
| Ordinary startup with diagnostics disabled | PASS, initialized scene, graceful close, exit 0 | `native-simulation-default-smoke-02/smoke.json` |
| Interactive gameplay/visual/audio acceptance | Not performed | Human acceptance remains separate |

The final corpus contains these twelve categories. Each uses 60 complete setup
transactions before its measured interval; every process starts fresh.

| Fixture | Measured steps | Behavior required by assertions |
|---|---:|---|
| animation-sword | 100 | Melee animation, active weapon geometry and attack registration |
| camera-targeting | 100 | Camera movement with targeting/input |
| collision-wall | 120 | Movement and wall-contact flag |
| draw-rng-keese | 60 | Gameplay RNG consumption during CPU drawing |
| gravity-fall | 80 | Downward velocity, displacement and landing |
| hud-countdown | 120 | Real timer decreases |
| input-short-pulse | 60 | Both short-pulse edges, held state and subsequent release |
| message-draw | 120 | Message-state changes and visible text progression |
| ocarina-memory-rng | 60 | Real audio RNG and memory-note construction |
| startup-idle | 80 | Stable initialized world and advancing frame counter |
| straight-movement | 120 | Player displacement and animation advancement |
| turning | 120 | Movement and multiple yaw values |

One complete repetition contains 1,140 measured steps and 1,152 snapshots,
including each fixture's initial snapshot. Three repetitions therefore contain
3,420 measured steps and 3,456 snapshots. Corpus 04 passed at those totals.

The negative control changed `$.actors[0].displacement.x` in a copy at tick 17:
bits `00000000` to `00000001`. Comparison identified that exact first tick and
field; restoration matched all 81 idle snapshots. The original output was
unchanged. The diagnostic-change comparison uses only completed run-001 outputs
from the failed corpus 02; it does not accept that corpus or erase its failed run.

## Exact commands

Run from the repository root, with fresh ignored output directories. The paths
below are the actual acceptance destinations; choose a new suffix when rerunning.

```powershell
$python = 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe'
& $python -B -m unittest discover -s scripts/native-simulation -p 'test_*.py'
& $python scripts/native-simulation/baseline.py build --config Release --jobs 4
& $python -B scripts/native-simulation/validate_native_cli.py --output build/native-simulation-cli-validation-04
& $python -B scripts/native-simulation/run_corpus.py run --output build/native-simulation-corpus-04 --repeats 3 --trace --keep-going
& $python -B scripts/native-simulation/presentation_matrix.py --reference build/native-simulation-corpus-04 --output build/native-simulation-presentation-01
& $python -B scripts/native-simulation/run_corpus.py negative-test build/native-simulation-corpus-04/startup-idle/run-001/output --output build/native-simulation-negative-04 --tick 17
& $python -B scripts/native-simulation/smoke_default.py --output build/native-simulation-default-smoke-02
& $python -B scripts/native-simulation/analyze_couplings.py --corpus build/native-simulation-corpus-04
& $python -B scripts/native-simulation/inventory.py
& $python -B scripts/native-simulation/inventory.py --output build/inventory-final-check
git diff --check
```

The read-only diagnostic comparison additionally called
`run_corpus.compare_runs(reference, candidate, include_trace=True)` for each
fixture's completed `corpus-02/<id>/run-001/output` and
`corpus-04/<id>/run-001/output`. Its aggregate and per-fixture receipts remain in
`build/native-simulation-diagnostic-comparison-01`; source output hashes were
checked before and after comparison.

The presentation matrix runs sword, HUD and Keese fixtures three times each at
60 and 120 presentation FPS, plus three trace-disabled idle runs: 21 additional
processes. It requires exact semantic equality against canonical 20-FPS output,
allowing only the declared presentation setting to differ. Traces must show
3 or 6 requested display-list replays per measured transaction; this is a CPU
submission count, not proof of completed GPU frames. All 21 processes passed,
including exact trace-disabled idle state and the declared presentation counts.
Simulation stays at 20 Hz. Combined with the canonical corpus, 57 final-executable
replay processes passed; the ordinary-mode startup check is a separate process.

## What changed

Paths below are repository-relative; the complete change list is available with
`git diff --name-only a4d6214bf9d9e2c02edf4e5394810cbcf48e12f9 HEAD`.

| Subsystem | Principal files |
|---|---|
| Replay, semantic boot, snapshots and normalized input | `soh/soh/NativeSimulationTest*`, `soh/src/code/padmgr.c` |
| Frame ordering, actor identity and Player action mapping | `graph.c`, `z_play.c`, `z_actor.c` under `soh/src/code`; `soh/src/overlays/actors/ovl_player_actor/z_player.c` |
| CLI integration and test audio boundary | `soh/src/code/main.c`, `soh/soh/OTRGlobals.cpp` |
| RNG, audio clocks and ingress observations | `soh/soh/ShipUtils.cpp`; `code_800E4FE0.c`, `code_800EC960.c`, `code_800F7260.c`, `code_800F9280.c`, `code_800FD970.c` under `soh/src/code` |
| Replay-only crash evidence | `soh/soh/CrashHandlerExt.cpp` |
| Runner, comparators, negative controls, fixtures and tests | `scripts/native-simulation/` |
| Repeatable local dependency configuration | `CMake/automate-vcpkg.cmake`, `scripts/native-simulation/baseline.py` |
| Architecture, coverage and provenance | `docs/native-simulation/` and regenerated inventory |

`NativeSimulationTest*` adds fixture parsing, fresh debug-save boot, rational-time
normalized controller injection, exact step limits, explicit semantic snapshots,
phase traces and completion receipts. The PadMgr seam retains the existing
press/release accumulator, including multiple transitions within one step.
`graph.c`, `z_play.c`, `z_actor.c` and Player action-name mapping provide ordering,
stable actor identities and observations. Ordinary actor behavior and arithmetic
are unchanged. `main.c` and `OTRGlobals.cpp` consume diagnostic arguments before
ROM extraction and preserve redirected output handles in test mode.

Gameplay and explicit-state RNG algorithms retain their arithmetic. Replay pins
initial seeds and clocks, records generator state/count/order and actor/phase,
and runs real audio control and mixing serially using the existing three-block
grouping with a fixed 528-sample sink. The normal audio worker/device path remains
available when diagnostics are disabled.

The Python tools create fresh local work/config/temp directories, capture source,
executable, fixture and asset identities, validate exact request/completion
binding, compute SHA-256 per snapshot/domain/sequence, and identify the first
different tick and named field. Float values contain binary32 bits and readable
decimals; no pointer addresses, struct padding or broad float tolerance enters
comparison. Unknown Player actions and missing/duplicate actor identities fail.

The build wrapper also prevents automatic vcpkg source advancement during a
repeatable local run. Final build receipts confirm vcpkg stayed at
`3aea538b2bb21a586502c67b00eb474fdd2e3098`. All temporary artifacts remain inside
the repository. The nine added acceptance tests reject missing RNG streams,
missing authoritative events, extra audio ticks, incomplete presentation counts,
and changed executable/asset identities between presentation cases.

## Measured authority and architecture decisions

The probe directly observes Ice Keese actor `scene1:spawn10`, type 19, making six
`Rand_ZeroOne` calls inside actor drawing on every measured tick: 360 total. The
message notice advances text position and its state machine inside message draw.
Earlier sword/HUD evidence identifies weapon geometry and attack registration
inside actor draw, and countdown/state transitions inside HUD draw. The final
coupling receipt confirms those observations in every corresponding final repeat,
and confirms three 528-sample blocks per measured transaction with matching
audio task, sample, clock, RNG and authoritative-event counters in all 36 runs.

Audio control (`Audio_Update`) and audio buffer production are separate stages.
The former follows CPU drawing in `Graph_Update`; the latter follows at the
existing worker wake boundary. Real ocarina memory initialization consumes three
audio RNG values and constructs three notes. These findings strengthen Pass 1's
ordering design; they do not justify removing CPU draw or audio logic from replay.

No authority was moved in this pass. Test execution deliberately retains complete
CPU drawing once per canonical transaction. The audio sink is a diagnostic
envelope: 528 x 60 = 31,680 samples per nominal second, not a claim of exact
32-kHz playback fidelity. A production sample budget belongs to a later pass.

## Failures retained and limits

The initial ROM-error modal came from passing `--native-sim-test` to the extractor
as a filename; it was an integration defect and is fixed. The initial corpus also
rejected two JSONL files containing startup logging. Their failed evidence remains
under `native-simulation-corpus-01`; the parser never strips or tolerates those
lines. The startup fix preserves process handles and delays file opening until
logging is initialized. This was test infrastructure failure, not an accepted
gameplay divergence or a reason to modify the ROM.

`native-simulation-corpus-02` remains a failed receipt: 35 runs completed and the
third gravity run suffered a graphics startup access violation before measurement
in `gfx_load_tlut_handler_rdp`. Its complete emitted trace prefix matches a
successful run. The cause is unresolved; neither the successful one-step startup
stress runs nor a subsequent passing corpus establishes that this fault is fixed.
Replay-only graphics crash metadata was added without changing rendering. Review
then bounded the additional crash-log writes to the pinned callback buffer's
capacity. This reporting fix is separate from the original startup failure.

`native-simulation-corpus-03` was intentionally stopped to rebuild that reporting
fix; its partial output and `interruption.json` are preserved. It is not an
acceptance run. Two debugger-assisted startup probes produced infrastructure
timeouts and no access-violation capture. No system settings or renderer behavior
were changed to make those probes pass.

The oracle requires a graphics context and local archives; it is not GPU-free.
Coverage is finite and explicit: selected Player/camera/world fields and base
actors, not all actor-family actions/timers or all hidden engine state. Variable
RNG instances are aggregated, caller sites are generator-level, and some actor
hooks have phase-only attribution. Pause/scene transitions, complete ocarina
scoring, NPC conversations, save/reload, long sessions and cross-platform float
identity remain uncovered. Gyro is rejected; physical mapping, device latency and
extended button edges are outside this corpus.

Repeated controlled replay does not prove byte-for-byte equivalence against the
untouched upstream executable, which lacks these seams and uses host clocks and
audio feedback. The preserved Pass 1 executable, normal-path review and separate
startup smoke provide distinct evidence. This Pass 2 checkpoint becomes the
instrumented canonical reference for future timing changes. No actors or gameplay
subsystems have been converted to higher rates.

## Exact next pass

Keep this executable/corpus as the canonical reference. Begin a bounded Phase 2
extraction of HUD countdown and simple-message draw-owned state only after the
unresolved startup-reliability issue is characterized or fixed and its reproduction
evidence is retained. First extend
fixtures for any dependency found during review. Preserve exact 20-Hz snapshots,
event/RNG order and presentation independence before and after moving those writes.
Leave actor pose/collision, enemy draw RNG and audio scheduling at their existing
seams during that first extraction. Do not enable higher-rate gameplay or begin
bulk actor conversion. Use **Ultra** for ownership/order design and independent
review, **High** for the scoped implementation. Pass 3 has not begun.

## Later crash-repair checkpoint (2026-10-03)

The startup defect was subsequently reproduced in candidate corpus 05,
`gravity-fall/run-002`. Its bounded diagnostics captured a 512-byte TLUT request
from `gLinkChildSwordTLUT`, whose image contains 216 bytes inside a retained
296-byte file buffer. The renderer's two 256-byte staging reads exceeded that
storage. The original corpus 02 has the same exception and handler but lacks
this resource metadata; its precise palette identity cannot be proved retrospectively.

The independently reviewed repair is libultraship
`c6bbb8c328938c115f4a1cbeaca3d00a4502269d`, on
`ChocoChopin/libultraship`, branch `codex/tlut-source-bounds`. The parent pins that
fork/revision. Resource-backed staging copies now preserve available bytes and
zero only the unavailable requested suffix. Raw-source and nonstandard
`tmem < 256` fallbacks are unchanged. All eight audited original palette consumers
use indices no greater than 106, within the 108 stored entries. Zero tail is a
defined missing-storage fallback, not byte identity with the original N64 bulk
transfer. Archives and the preserved Pass 2 executable/corpus remain unchanged.

The fixed Release executable is 26,991,616 bytes, SHA-256
`e3da61b9397dee5d2e1ef3e168927d9525742bb6626142291853e879ff6864f4`,
preserved as `build/native-simulation-reference/soh-pass3a-tlut-fixed.exe` with
its PDB. It was built from root base `4e5ec595717dd1008c7d09b6aaab0144bc6bb4c7`
plus the already saved, uncommitted Pass 3A runtime observations and the reviewed
library repair. All eight compiled runtime/library files are separately bound
by size and SHA-256; this is not a clean crash-only root build claim.
`build/pass3a-evidence/fixed-validation-01/` retains the successful complete build
and original test logs. `build/pass3a-evidence/crash-validation-01/crash-validation.json`
records the narrowed crash-only validation and unchanged runtime/asset identities.

| Gate under `build/` | Outcome |
|---|---|
| `native-simulation-tlut-bounds-02/tlut-bounds-result.json` | PASS, 31 native production-helper checks |
| `pass3a-evidence/fixed-validation-01/python-tests.log` | PASS, 103 tooling/math/acceptance tests |
| `native-simulation-cli-05/native-validation.json` | PASS, 37 native CLI checks |
| `native-simulation-corpus-06/corpus_result.json` | PASS, all 12 original fixtures / 36 fresh runs; complete snapshots and traces exactly match corpus 04 |
| `native-simulation-presentation-02/matrix_result.json` | PASS, original seven cases / 21 fresh runs, including trace disabled |
| `native-simulation-corpus-06/measured_couplings.json` | PASS, all 36 runs with expected RNG/audio/event accounting |
| `native-simulation-startup-gravity-fixed-01/stress_result.json` | PASS, 12/12 complete gravity replays, no exception or timeout |
| `native-simulation-startup-stress-fixed-01/stress_result.json` | PASS, 180/180 starts across two scenes, 20/60/120 presentation FPS and trace on/off; every initialization/measurement confirmed, no exception or timeout |
| `native-simulation-default-smoke-03/smoke.json` | PASS, ordinary startup and graceful exit 0 |
| `native-simulation-negative-05/negative-test.json` | PASS, one-bit diagnostic mismatch detected at tick 17; exact restoration passes |

This verifies the bounded resource-copy repair and preserves the existing
canonical acceptance suite. The failed corpus 02/05 evidence and separate
copied-executable-location stalls remain retained. Finite successful campaigns
do not establish zero future crash probability, pixel equivalence or human
gameplay acceptance. Independent crash-source and runtime audit receipts are in
`build/pass3a-evidence/`.

This checkpoint intentionally stops at the crash repair. The existing Pass 3A
observation/design work is preserved locally; its new draw-state corpus/matrix
acceptance and final handoff remain for the resumed pass. No authority extraction
or higher-rate gameplay was implemented here.
