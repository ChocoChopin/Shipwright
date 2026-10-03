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
audio/control serializers, ocarina and draw-RNG fixtures, presentation-cadence
independence, audio reset/scene transitions, hardware output acceptance, and the
generalized rational audio scheduler. None is implied by deterministic same-build
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
misreported as a random generator call. Source-identified Gohma/Firefly draw-RNG
sites still require their own runtime fixtures.

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
