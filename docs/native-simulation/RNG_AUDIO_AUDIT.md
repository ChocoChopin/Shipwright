# Canonical replay RNG and audio audit

This is the Phase 1 source and instrumentation contract. It adds deterministic
test inputs and observations; it does not convert audio or gameplay to a higher
simulation rate. Executed corpus results belong in TESTING.md and the local run
manifests. The default game keeps its host clock, device feedback and audio worker.

## Ordered frame boundary

The relevant native sequence is:

1. `Play_Update`, then full `Play_Draw` and common game-state drawing.
2. `Graph_Update` calls `Audio_Update` in `soh/src/code/graph.c`. This is control
   work: ocarina state, sound requests, sequence commands, fanfares and command
   scheduling. `GameState_Init` also calls it once during initialization.
3. `Graph_ProcessGfxCommands` starts mixer/sequencer buffer production, runs
   graphics commands, and waits for audio completion before returning.

In ordinary play step 3 uses a worker overlapping GPU work. The worker produces
`max(1, R_UPDATE_RATE)` blocks, hence three blocks during ordinary 20-Hz gameplay.
Its sample count is 560 or 528, selected by audio-device buffer occupancy. Each
block executes `AudioMgr_CreateNextAudioBuffer`, including sequencer/synthesis
work and audio RNG advancement. Skipping sound playback alone would leave this
hardware feedback in the logical engine.

Test mode precaches the same audio resources but does not create the worker. At
the existing worker-wake boundary it executes the same buffer function serially,
using 528 samples per block, before the unchanged GPU command path. It preserves
the authentic divisor grouping, including one-block boot states. Generated PCM
is discarded. It neither skips audio logic nor replaces it with event counters.

528 is one existing device-feedback branch, selected as a fixed diagnostic sink.
It is not a claim about hardware audio fidelity or a new production sample-clock
policy. 528 samples at 60 blocks per second is 31,680 samples per nominal second;
this envelope therefore does not establish exact 32-kHz real-time playback.
Choosing a future rational sample budget is a separate reviewed change. Completion
before GPU submission makes worker ownership deterministic; equivalence for GUI
or interpreter callbacks that themselves mutate gameplay still requires evidence.

The test buffer holds 528 stereo samples (2,112 bytes). For the normal 32-kHz,
60-Hz audio parameters, `AudioSynth_Update` partitions this into three 176-sample
chunks. Its final output writes call `aSaveBufferImpl` synchronously, each copying
704 bytes; that function rounds byte counts down to a multiple of 16. The output
pointer is not retained for later work. Mixer rounding for intermediate operations
uses its own DMEM buffer. The production worker's larger allocation holds up to
three blocks together for device submission. This source audit found no output
buffer-size or lifetime defect at the test seam; it is not general memory-safety
proof for the audio engine.

## Controlled and observed RNG domains

| Domain | Existing semantics | Test seam and observation |
| --- | --- | --- |
| Gameplay LCG | `code_800FD970.c`: unsigned `state * 1664525 + 1013904223`; float draws reinterpret the original masked bits | Only the `Play_Init` host-time seed is selected from the fixture. Explicit `Rand_Seed` calls retain their supplied values. Each seed is an event; each draw reports resulting state, generator API, phase, actor scope and ordered ordinal. Arithmetic and draw order remain intact. |
| Caller-owned LCG | The `Rand_*_Variable` family mutates a caller's `u32` state | Seed/draw events use a separate domain. No pointer is emitted. Domain counts are aggregate across caller-owned streams; per-object stream identity is not yet universal. |
| Audio context RNG | `AudioMgr_CreateNextAudioBuffer` mixes `totalTaskCnt` and `osGetCount` into `gAudioContext.audioRandom` after synthesis | The host count operand alone uses the deterministic replay audio clock. Every produced block and resulting RNG value is observed. |
| Audio next RNG | `Audio_NextRandom` owns `audRand`, initially `0x12345678`, and combines count, task count and audio context RNG | The count operand uses the replay audio clock; output state and call order are observed. Fresh-process fixtures reset the static; there is no claimed in-process reset. |
| ShipUtils PCG-style stream | A 64-bit multiply/add feeds rotated 32-bit output; the default stream lazily seeds from `random_device` | Only the lazy default seed is overridden in replay. Seed low/high words, state low/high words and output are observed. Supplied explicit-state streams retain their initialization semantics. |
| C library RNG | `InitOTR` calls `srand(time(NULL))`; found consumers are dynamic-cosmetic color generation | Replay seeds `srand` from the fixture and fixes the initialization calendar time. C-library generator internals and per-draw calls are not serialized; covered fixtures keep those cosmetics disabled. |

The audio context starts zeroed in `AudioLoad_Init`. Seed control occurs before
scene actor initialization, so randomized spawn/setup work is included rather
than overwritten after it. Fresh-process reset remains mandatory. The original
unreachable `func_800E5000` body retains its host-clock code after an unconditional
`return NULL`; it is not part of the desktop execution path.

Observer calls do not request RNG values, alter seeds, move calls between phases,
or change actor traversal. A site identifies the generator/ingress API, not every
calling source line. Phase and current stable actor identity narrow attribution;
same-API calls from two lines of one actor are distinguished by ordinal only.
No return addresses, process addresses or allocator identities enter the schema.
Nested actor initialization preserves its caller's scope. The scope brackets the
existing actor lifecycle callbacks, not every surrounding hook: delayed-init and
post-update `GameInteractor` callbacks outside those brackets may emit events with
no actor identity. Their phase, API, value and event order are still observed.
This limits attribution and does not imply that those hooks are absent or that
their gameplay effects have been excluded from the transaction.
The observer retains a cumulative event-order fingerprint even without verbose
JSONL output, including seed changes, RNG draws, audio blocks, requested/queued
sound IDs, sequence commands, ocarina memory notes and ShipUtils' high state word.
Presentation counts and phase boundary events are trace-only; changing how often
GPU commands are presented must not itself change an authoritative fingerprint.
A companion `ship-utils-high` snapshot entry stores
that word; it is not an independent generator. Seed events update the current
snapshot state without counting as random draws.

`Graph_StartFrame` clears and ignores the host scancode in replay mode, preventing
F-key savestate/TTS and Tab asset toggles from entering through a path outside
the normalized pad provider. Window event pumping remains active for graphics.

## Gameplay coupling and trace scope

| Coupling | Classification and instrumentation |
| --- | --- |
| Gohma limb color and Firefly limb effects draw from the gameplay LCG during CPU drawing | Draw-owned RNG affects the later shared stream. Retain full CPU draw; RNG records carry draw phase and actor scope. These hooks do not establish actor-specific fixture coverage by themselves. |
| Player post-limb drawing computes melee/shield geometry and collision registration | Authoritative pose/contact work, retained in place. Update-only or GPU-only tests would omit it. |
| Actor drawing changes update-culling flags and `isDrawn` and dispatches flagged sounds | Scheduling, lifetime and audio effects, retained. `Audio_PlaySfxGeneral` records requested ID and accepted queued IDs, including additive swaps, without dereferencing position/frequency pointers. |
| Interface and message drawing advance timers and dialogue/ocarina state | Authoritative state-machine work, retained. Selected state snapshots before/after CPU draw identify changed fields; no wholesale movement into update occurs. |
| `Audio_Update` advances ocarina/control state and consumes queued sound/sequence commands | Authoritative control stage. Sequence ingress records exact `Audio_QueueSeqCmd` command words; sound ingress records IDs in order. |
| `Audio_NextRandom` selects Lost Woods memory-game notes | Explicit gameplay consumer of audio RNG in `AudioOcarina_MemoryGameNextNote`; a note-pitch event follows the existing nonrepeat rule. This source hook alone is not a passed memory-game fixture. |
| Sequence scripts and sound frequency choices read `gAudioContext.audioRandom` | Audio behavior depends on the deterministic buffer schedule even in a quiet fixture. Snapshot task count/state and RNG order, not just sound requests. |

Sound-request observations do not yet serialize every pointed-to frequency,
volume, position, reverb or bank-internal field. Ocarina note events do not
serialize the entire song/controller state. The initial oracle must list its
selected audio state fields and exercised scenarios honestly; passing quiet
movement fixtures does not qualify the ocarina minigame or all audio resets.

`Audio_Update` also reads `osGetTime` for debug-duration measurements. Those host
profiling fields are excluded from authoritative hashes. The source search found
other timing calls in profiler/scheduler, allocator diagnostics, DMA logs, JPEG
timing and fault handling; the replay does not globally replace the platform
clock or hash those measurements.

## Required evidence and later work

Two fixture recipes call existing engine entry points once at the measurement
setup boundary. `hud-countdown` calls `Interface_SetTimer(10)` and then leaves all
preview/movement/countdown work to `Interface_Draw`; its assertion requires an
actual seconds decrement. `ocarina-memory-rng` calls
`AudioOcarina_MemoryGameInit(0)`, which selects three real memory-game notes through
`Audio_NextRandom` and the existing nonrepeat rule; its assertion requires at least
three audio-next draws. It covers note initialization, not playback, dialogue,
controller performance or scoring. These are semantic setup recipes, not synthetic
loops that merely consume RNG or manually decrement a timer. Consult TESTING.md
for executed outcomes rather than treating fixture presence as a passing result.

The runner must compare at least three fresh-process repeats and preserve exact
RNG event ordering plus task-count progression. A normal measured 20-Hz step has
three block events of 528 samples each. Input, actor, draw and audio observations
must share phase/step identity so the first changed RNG ordinal is actionable.
Negative comparator tests must reject altered state/event values; they do not
require changing any generator or engine arithmetic.

Later gates remain independent: per-actor variable-stream identity, complete
audio/control serializers, ocarina performance/scoring and broader draw-RNG actor
coverage, presentation-cadence independence, audio reset/scene transitions,
hardware output acceptance, and the generalized rational audio scheduler.
None is implied by deterministic same-build
canonical repeats under this fixed sink.

## Measured canonical replay evidence

These observations use the native Windows Release executable with SHA-256
`b7c80d0ce177f9f2d45addb0284bbeb37109dde67b412f30c1649cf50835f539`
(26,961,920 bytes). The source/configuration/asset envelope is recorded in each
local `corpus_result.json`. They establish same-binary canonical repeatability
under the declared deterministic sink, not a comparison against an uninstrumented
upstream executable or interactive audio acceptance. All runtime artifacts stay
in ignored `build/` directories.

The three `startup-idle` runs in `build/native-simulation-probe-02` each contain
81 semantic snapshots and 5,260 trace records. Both comparisons to run 1 pass
exactly: final snapshot SHA-256
`649488ae429e2055bc0c9d2e4a78a53b99f65c6224cbe66538d2bec20c428f6f`,
trace SHA-256
`8dbb24bb4438d460c31832d52ffc246c339e43c30549b7d415dd1d6d2739d4df`.

`animation-sword` in `build/native-simulation-corpus-01` passes three consecutive
runs, each with 101 snapshots and 57,599 trace records. The final snapshot hash is
`971f6a17c8458bdf12c860bdfc3201fd4f3c8a71a0dfbe3488f91eb1aa76ba2e`
and the trace hash is
`e59f72bc2201d5986ad1ac5ad9c61a2afe0f64c1ce1495edc3a302b01160ca5a`.
Its 100 measured steps produce exactly 300 blocks of 528 samples: audio task/block
count advances from 180 after setup to 480, and sample count from 95,040 to
253,440. There are 300 audio-context RNG advances, three `Audio_NextRandom` calls
in `audio_control_begin`, and 16,005 gameplay-LCG draws in `update_begin`.

The sword trace shows `/collision/at_count` changing **1 -> 3 inside actor CPU
draw** at beginning ticks 16, 17, 43, 44, 73 and 74. The corresponding boundary is
`draw.actors.begin` to `draw.actors.end`; Player weapon active flags and computed
tip/base coordinates also change there. This is measured collision-registration
authority in draw, not merely a visually changed pose. Snapshots identify the
Player actions as `Player_Action_Idle` and `Player_Action_808502D0`, with no
unmapped action in this fixture.

The three measured audio-next draws occur at beginning ticks 14, 41 and 71 in the
separate audio-control phase. No gameplay LCG draw was observed inside CPU draw
for this particular fixture. Its draw-side `rng.events` changes represent queued
logical audio events; a changing aggregate RNG-domain fingerprint must not be
misreported as a random generator call. This initial fixture did not cover the
source-identified Gohma/Firefly draw-RNG sites; the later focused Keese receipt
below covers the Firefly limb path.

`hud-countdown` in the same corpus passes all three runs, with 121 snapshots and
6,717 trace records per run. Its final snapshot SHA-256 is
`36d7527f29085db777b1fd3feb1cb4e407c68953d3d5abdc3072947d9d796769`
and trace SHA-256 is
`43bce8b41f00a34974164d84f8ecc516253ac6206e9c4383367b32a0c5d9d682`.
The timer begins with ten seconds in state 5. The trace attributes every measured
timer transition below to `draw.interface.begin` through `draw.interface.end`:

| Beginning tick | Observed HUD-owned mutation |
| --- | --- |
| 0 | Timer state 5 -> 6 |
| 20 | Timer state 6 -> 7 |
| 40 | Timer state 7 -> 8 and seconds 10 -> 9 |
| 60 | Seconds 9 -> 8 |
| 80 | Seconds 8 -> 7 |
| 100 | Seconds 7 -> 6 |

The 120 measured steps emit 360 blocks of 528 samples and 360 audio-context RNG
advances. Four sound requests and four accepted sound queue events also originate
inside the HUD draw phase. All four measured gameplay-LCG calls occur in
`update_begin`; the observed Player action is `Player_Action_Idle`. The timer
recipe therefore exercises actual draw-owned countdown and logical audio work,
without attributing its event fingerprint changes to draw-owned RNG calls.

`ocarina-memory-rng` also passes three runs, with 61 snapshots and 4,549 trace
records each. Its final snapshot SHA-256 is
`8d46ead0900a377e4e28e6d67d2c609d7e44347d23e71e742243b0b035bb4864`
and trace SHA-256 is
`c8282d99d84b01c1dde78cd90b83532968410b247adb7de370d80e4359aeda9e`.
The existing memory initialization selects pitch values **5, 14, 5**, with note
length 45 and volume 80. Exactly three `Audio_NextRandom` calls perform that work
at the one-time setup boundary, before measured snapshot 0; the audio-next stream
already reports three calls and state 860358716 in that snapshot and does not
advance again during the following 60 measured steps. Those steps emit another
180 blocks of 528 samples and 180 audio-context RNG advances. This proves
deterministic initialization of the real selected notes, not playback, player
responses, note timing during play or memory-game scoring.

These are individual fixture receipts. The full `corpus-01` run is not a passing
corpus: two other outputs contained logging text where JSONL was expected. Their
failure is retained as output-infrastructure evidence; it does not invalidate the
exact comparisons above or establish the cause of the stream corruption. A
later repaired corpus must carry its own executable identity and comparison
receipts.

Trace ticks label the interval's beginning; a mutation in trace tick `n` is
captured by the committed snapshot at tick `n + 1`. This distinction matters when
reading a draw-owned countdown or collision registration against end snapshots.

### Focused draw-RNG and message evidence

`build/native-simulation-coupling-probe-01` uses the later native Windows Release
executable SHA-256
`c82c484661417a924e6dcedde498b41e7ba3bf3c4b8bf4c84f1e5887b7a384dc`
(26,963,968 bytes). Both fixtures pass three fresh-process repeats, including exact
snapshot and trace comparisons. These receipts are distinct from the earlier
binary and the failed initial corpus above.

`draw-rng-keese` uses Ice Cavern entrance `0x88`, requires the Firefly object bank
to be present and loaded, and spawns one ice Keese through the existing actor
entry point. Snapshot actor type 19 has stable identity `scene1:spawn10`. Its
unchanged limb callback performs **exactly six `Rand_ZeroOne` calls per measured
tick**, all attributed to that identity in `draw.actors.begin`. Across 60 ticks
this is 360 actual gameplay-LCG draws, matching the snapshot `draw_calls` delta.
This is stronger evidence than a change in the aggregate event fingerprint.
There are also 967 gameplay-LCG calls in `update_begin` and 180 audio-context
advances from 180 blocks of 528 samples. Player actions are
`Player_Action_Idle` and `Player_Action_8084FB10`; Keese coverage remains base
actor state plus its observed RNG calls, not a complete enemy action serializer.

Each Keese run contains 61 snapshots and 7,728 trace records. Its final snapshot
SHA-256 is
`950a1b71edb73c16762887d058bc21d7d8c0b5a6b2a4601fa1289c7903d55d2f`
and trace SHA-256 is
`db8dd63dff9cd01eaec1e20d6e8fa03947e0569957ffda8d80c33d5041ea86e6`.

`message-draw` starts the existing textbox `0x305F` once. The trace shows text
draw position **1 -> 30 at beginning tick 10** and **30 -> 31 at tick 11**,
followed by message mode **6 -> 53 at tick 12**, all between
`draw.message.begin` and `draw.message.end`. Other observed message transitions
occur in update, including mode 53 -> 54 at tick 72 and 54 -> 0 at tick 74;
the fixture does not assign the entire message state machine to draw. Its 120
measured steps produce 360 audio-context advances and 360 blocks of 528 samples,
with `Player_Action_Idle` throughout.

Each message run contains 121 snapshots and 9,984 trace records. Its final
snapshot SHA-256 is
`56bc3883dd7b548958de109ff87a5c2fc0fdbc60627c7f396a7738f496a9be8b`
and trace SHA-256 is
`d71892e38073823f14d1812318230cc0d68e9410e6873a0d2c5713a812c14b73`.

Reproduce these measurements with
`python -B scripts/native-simulation/analyze_couplings.py --corpus build/native-simulation-coupling-probe-01`.
The analyzer writes `measured_couplings.json`, validates every JSON/JSONL record
without filtering or repair, checks actual audio block/sample/task/clock/RNG
deltas, and records selected phase mutations with old/new values. Its aggregate
pass requires a passing corpus completion receipt and all expected fixture
identities and repeats, with exact repeat snapshot/trace hashes. Failed or
incomplete corpora retain per-run diagnostic results but cannot pass as a subset.

### Startup failure and diagnostic scope

The later twelve-fixture `build/native-simulation-corpus-02` remains failed.
Eleven fixtures completed; `gravity-fall/run-003` timed out after a Windows access
violation in the first graphics transaction, before measurement. Its retained
game log places the failure in `Fast::gfx_load_tlut_handler_rdp` in the unchanged
libultraship renderer. The log does not establish whether the TLUT source address,
resource length or another earlier write caused the fault. No palette clamping,
skipping, fixture retry relabeling or physics change is a justified fix from that
evidence.

The diagnostic executable SHA-256
`5522dfe9cd1c804df4f0ee13d4a4bd6a5a849d51ac97e91837adb57416f5a863`
(26,968,576 bytes) adds replay-only crash metadata for the current graphics
command, TLUT parameters, texture resource and address ranges. Those addresses
belong only to local crash diagnostics and never enter semantic hashes. The
diagnostic does not read bytes at the texture-source pointer or modify renderer
behavior. Ordinary runs return immediately from this additional reporting path.

`build/native-simulation-palette-stress-01` records forty successful fresh
processes with that diagnostic executable. Each has one setup transaction and
one measured transaction at the Kokiri Forest entrance. This did not reproduce
the startup crash; it does not demonstrate that its cause was fixed or replace
the full corpus gate.

A subsequent source review bounded the additional crash-report lines to the
pinned callback's 32,768-byte capacity, including a terminating NUL. That change
addresses diagnostic reporting capacity only, not the startup access violation;
it is not contained in the `5522...` executable receipt above. The capacity fix
is committed as `68cd6a6520dcc9e3c4147fbc9dec62cd2f702417`; its rebuilt executable
has SHA-256 `e64ec79a23627f7d288ee0a6e96fbbc701ddefee1a8d403b7f1aa856459dadbf`.
The final corpus and presentation receipts below use that rebuilt executable.
CLI, negative-oracle and ordinary-smoke receipts are separate gates. The
historical startup failure remains unexplained, and successful later runs do
not establish a renderer fix.

### Final canonical corpus receipt

`build/native-simulation-corpus-04/corpus_result.json` and its
`measured_couplings.json` both pass on the `e64ec79...` executable above
(26,969,088 bytes). All 12 fixtures complete three fresh-process runs: **36 of 36**
expected outputs, no missing or unexpected outputs, and 24 passing comparisons
with exact complete snapshot sequences and traces. Every run passes the audio,
RNG and authoritative-event counter checks. Across 3,420 measured transactions,
the traces contain 10,260 audio blocks and audio-context RNG advances, with
exactly three 528-sample blocks per transaction and 5,417,280 samples overall;
snapshot block, sample, task, clock, RNG and event-counter deltas agree with the
traces. Each Keese run again measures 360 draw-owned `Rand_ZeroOne` calls for
`scene1:spawn10`, six on each of 60 ticks, matching the snapshot draw-call delta.
The final runs also retain sword collision registration 1 -> 3 and weapon
geometry changes in actor draw, HUD seconds 10 -> 6 in interface draw, message
position 1 -> 30 -> 31 and mode 6 -> 53 in message draw, and ocarina pitches
5, 14, 5 selected by three real audio-next draws before snapshot zero. These are
canonical same-build repeatability and measured-coupling results within the
declared serializer and fixed-sink scope; they do not qualify higher simulation
rates, full enemy behavior, ocarina scoring or hardware audio output.

`build/native-simulation-presentation-01/matrix_result.json` also passes all
seven cases with three fresh-process runs each, using the same executable.
Sword, HUD and Keese fixtures at both 60 and 120 presentation FPS match every
complete canonical 20-FPS snapshot exactly while simulation remains 20 Hz.
Their traces record respectively three and six requested display-list replays
per transaction; this measures requests before `RunCommands`, not GPU
completion. Three additional `startup-idle` runs with tracing disabled also
match the canonical snapshots exactly. This establishes presentation-cadence
and trace-enable independence for these selected cases within the fixed sink
and serializer scope, without enabling higher-rate simulation.
