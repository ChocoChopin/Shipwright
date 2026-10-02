# Deterministic simulation testing design

Status: architecture/reconnaissance, 2026-10-02. The game does **not** yet have the
proposed deterministic runner or native 30/60/120-Hz simulation. The executable
mathematical reference tests below are the only new testing implementation in
this pass. Build/asset/launch evidence belongs in `BASELINE.md`; source references
here refer to the pinned upstream/submodule revisions recorded there.

## 1. Findings and available seams

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
| CLI/window | `soh/soh/OTRGlobals.cpp:309`, `:410`, `:1521` | Startup initializes a window and passes arguments into extraction. No existing `--simulation-rate`, `--replay`, `--headless` or unattended gameplay-test CLI contract was found. All such flags below are proposals. |

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

The exact injection seam is a Phase 1 deliverable. A test-only normalized-pad
provider adjacent to `osContGetReadData` exercises the ordinary PadMgr path;
multiple events between steps additionally need ordered edge accumulation rather
than merely overwriting `OSContPad` once. Avoid requiring a libultraship submodule
patch if a Shipwright boundary can provide the same well-defined contract.

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

Proposed runner flow (not an available command today):

```text
verify manifests -> build candidate -> unit tests
 -> reference20 three times -> candidate20 three times -> strict comparison
 -> candidate30 three times -> candidate60 three times -> candidate120 three times
 -> compare common times, events, temporal contracts
 -> render-cadence variants -> classify first divergence
 -> write result.json + readable diff + bounded event context
```

Suggested future entry point is `scripts/native-simulation/run_corpus.py` with
an explicit executable path, fixture list, rates and output root. It will launch
the game with a separately implemented test-only fixture/trace interface. Neither
that runner nor those game flags exist yet. Give each process a host timeout,
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

Executed in this pass with Python 3.14.2: **22 tests passed**. `-B` avoids bytecode
cache artifacts. Tests create no temporary files and need no ROM, extraction,
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
