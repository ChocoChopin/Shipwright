# Native simulation conversion ledger

## Pass 4C completed scope

PlayerContactCore.hpp and PlayerContactBridge.cpp own bounded sign contact reservations and world20 response delivery. PlayerTemporal.cpp binds lifecycle and phase hooks. z_player_lib.c produces sweep availability and admits only known sign cylinders without body overlap; z_en_kanban.c reads captured animation only for bridged hits. Ordinary/Z-sign60/120 contact, rejection, canonical regression and local selection are qualified; human play is pending and broad combat is unconverted. See PASS4C.md for identities and compact receipts.

Baseline under investigation: upstream `develop` at `9eafd15fe1382c5a41e881f1b6ea87345c797d18`.
Through Pass 4A, **zero gameplay subsystems and zero actors have been rate-converted**.
Pass 3B extracts only bounded countdown/message authority at canonical cadence. The
source review below is representative reconnaissance, not a claim that every
branch of 429 actors has been audited. See [ARCHITECTURE.md](ARCHITECTURE.md),
[TIMING_SEMANTICS.md](TIMING_SEMANTICS.md), [TESTING.md](TESTING.md) and
[KNOWN_DIVERGENCES.md](KNOWN_DIVERGENCES.md) before claiming work.

## Auditable inventory and limits

### Pass 4B completed: gated fixed Player scheduler and dependency closure

The internal selector executes Player input/action, movement, static collision,
owned animation, control camera and extracted late pose at 60/120 Hz. World
actors/collision/blink/scripts/environment/HUD/message/audio remain 20 Hz. The
gate admits ordinary child idle, walking, ready-sword slash and distant friendly
targeting with default equipment/settings on bounded dry static geometry. Links
House fixed-eye walking additionally admits the tested nonclimbable static wall.
Unsupported input/state withholds remaining intermediate starts, preserves queued
logical events and resumes canonical execution at the next world boundary.

Owner: Codex on `mod/native-simulation-rates`, starting
`6153eb451a37dfc6b3bfa6f6edebece85d9e08e8`. Scope is the bounded ordinary child
input/action/movement/static-background/animation/control-camera/late-pose island
at 20/60/120, with world work held at 20 Hz. Shared world helpers retain their
original behavior. Exact canonical fixtures remain the oracle. No target damage
bridge, moving geometry, hostile combat or broader actor conversion is claimed.
See PASS4B.md for acceptance receipts, source identities, explicit omissions and
historical failures. Only internal fixtures select the gated high-rate capability.
The user's lean policy governs subsequent runs: one by default, compact successful
receipts/hashes, in-process invariants/comparisons and bounded diagnostic rings.
Default successes serialize no full snapshots/substeps. Explicit diagnostics and
failure dumps preserve needed detail; `PASS4B.md` records parity and output sizes. Cleanup manifests
replace redundant raw success archives; they do not replace comparison results.
Player-step/next-world QA is implemented. Immutable evaluated packets live within
one synchronous world draw arena. High-rate sword history/geometry advances once
per Player interval, with no global Player collider registration or target damage.
Motion/animation per-interval checks, held-target/world ownership and rendering
independence are separate gates; passing one does not establish the others.
`player-temporal-units.json` records units, continuation choices and reset owners.
Pass 4C remains the contact bridge and interactive target-contact qualification.

### Pass 4A completed: temporal core and canonical QA

Owner: Codex on `mod/native-simulation-rates`, starting `f8fe6a7fd`.
Scope: native time/rate/context types, opportunity/attack/input/contact identity,
generation/reset ownership, unit inventory and full-transaction pause/step controls.
No gameplay scaling or cadence change. Legacy operations/reset sites remain exact;
new state is sidecar metadata with explicit scene/Player lifetime. The reviewed
native helper passes 104 checks and Python tooling 139 tests. Reviewed-build
acceptance is 30 Player runs, strict phase/contact and direct purity, 15 focused
original/HUD runs, six stepped runs, 15 presentation/trace-off runs and 45 CLI
checks. Controls and startup pass. The isolated final Player/world reset ownership
fix passes six fresh canonical/purity and three stepped-sign runs plus startup;
broader results have explicit source reuse, not a second complete rerun.
No new native crash occurred. Windows command-lock evidence and earlier findings
are retained. PASS4A.md and `build/pass4a-03/final-acceptance.json` record exact
identities, totals, omissions and the next boundary. No higher-rate admission.

### Pass 3D completed: canonical Player pose/contact extraction

Owner: Codex, `mod/native-simulation-rates`, starting at
`ce54b58bb94d5fc695494fb70a6c6aa1e98bd3d7`.
Claim: bounded ordinary child Player pose/contact authority at the existing late
skeleton slot, immutable same-transaction presentation, whole legacy fallback,
and direct CPU purity verification. No timing units, action cadence, collision
scheduling or high-rate support change. Authority uses the unchanged canonical
callback arithmetic and existing reset sites; packet storage expires with the
graphics transaction. Native source checkpoint `857722ae8507cb84aca9ed2d823fc4ffc187fec4`
passes all 30 canonical Player runs, direct purity and 20 admission negatives.
Focused presentation evidence totals 45 runs (39 source-verified reuse, six on
the final build); 24 broad regression runs, 131 Python tests, 43 CLI checks,
graceful controls and ordinary startup pass. See PASS3D.md for exact evidence,
omissions and unchanged dependency pins. No new native crash occurred.

### Pass 3C completed: design/reference work, explicitly reduced acceptance

Owner: Codex, `mod/native-simulation-rates`, starting at `39344b1c4`.
Scope: bounded Player pose/contact ownership design, optional semantic observation,
canonical fixtures, and the Player-rate/world-20 boundary. No authority extraction,
timing primitive, gameplay rate conversion or actor admission is claimed. Runtime
targets are now 20/60/120; 30-Hz gameplay is deferred, with cheap fractional math
coverage retained. Opt-in Player snapshots now observe joint/body/attachment
geometry, active windows, collider flags/registration, combo state and real sign
contacts at their original phases. Ten canonical fixtures cover idle, slash,
third-attack combo, shield posture, Z attack, turn/move attack, static wall and
ordinary/Z-targeted sign contacts. Observer counters have fresh-process lifetime;
gameplay field units/lifetimes and the extraction graph are recorded in PASS3C.md.
At the instrumentation checkpoint, the original 36 runs match Pass 3B exactly,
all 30 new Player runs and their phase/target-consumption analysis pass, and
43 native CLI checks and 129 Python tests pass. The final campaign also passed
the 36-run original purity-on corpus, the 24-run HUD/message purity-off corpus
and its phase analysis, plus 31 TLUT and 20 coverage checks. The HUD/message
purity-on campaign stopped after nine successes on a new startup exception
0xc0000005 in `hud-zero-input/run-001`; 14 runs remain unattempted. No reproduction
or debugging followed before the later explicit crash-fix authorization.
PASS3C.md and ignored `pass3c-evidence/crash-stop.json`
record that historical boundary, when the claim was not yet accepted. The later
repair and user-approved acceptance below preserve those stopped receipts and
the Pass 3B references.

### Pass 3C crash-only renderer repair (scoped dependency change)

Owner: Codex. User-authorized scope: fix and validate the retained crash, then stop.
The fault is a resource-backed I4 importer reading a 128-byte magic-meter request
from a 64-byte image at the end of its owned file. This is renderer application
source failure, not a gameplay timing conversion. Source/row strides are bytes;
row counts and requested texture dimensions keep their existing convention.
`CopyTextureResourceRows` stages only a short resource request, retains available
bytes and zeroes missing bytes. The scratch buffer has one-import lifetime; the
resource's existing shared ownership remains intact. Valid resources retain their
original path and dimensions. Raw/replacement pointers and other decoders are
outside this fix. TLUT keeps its API and behavior through the shared extent helper.

The separate dependency commit is `9280b17ddc504da6630892a46440e86be41ac571` on
`ChocoChopin/libultraship`, branch `codex/i4-resource-bounds`; the root pin and
branch hint advance together. The reviewed source payload is three files only.
The Release build, 24 texture checks, 31 TLUT checks, 129 Python tests and 18 focused
fresh-process replays pass. All 18 execute the bounded magic-meter path; exact
canonical state/events and direct helper purity are preserved. Twelve traced HUD
runs pass phase/input/paint checks. The old failed run and both preserved reference
executables remain unchanged. See PASS3C.md and ignored
`build/pass3c-crashfix-01/crash-resolution.json` for source/build/asset bindings.

The refreshed inventory contains 2,433 files / 697,288 lines, 1,525 candidate files /
63,860 candidate lines; all 430 actor claims remain unclaimed. Two scans are
byte-identical across all four generated outputs. No timing edit or actor admission
is claimed. The repair stopped at the user's model-switch boundary. The user's
subsequent continuation authorizes the remaining acceptance gates, now using fresh
`build/pass3c-resume-01` paths and the same bound repaired executable. Existing
reference/failure receipts remain unchanged; same-executable matrix prerequisites
and the reused completed HUD matrix are accounted for explicitly in PASS3C.md.
The repaired continuation has passed all 90 canonical runs and their strict
analyses. A later 120-second presentation timeout retains 75 exact snapshots
and active output through the cutoff, with no native exception marker. The
continuation under `build/pass3c-resume-02` used a 300-second presentation
allowance. The user then explicitly trimmed repetitive testing. Accepted coverage
is 90 canonical plus 135 matrix replays, including 42 Player variants; 43 CLI
checks, focused controls and ordinary startup also pass. The remaining 48 Player
variants and 39 startup-stress runs are omitted, not accepted as passing.
`build/pass3c-resume-03/final-acceptance.json` binds the completed reduced scope:
225 replays / 21,381 snapshots, two engine controls, no new native crash, retained
timeout and unchanged old evidence. All 430 actor claims remain unclaimed;
the next canonical extraction is unstarted.

### Pass 3B completed (bounded authority extraction, no rate conversion)

Owner: Codex. Runtime checkpoint: `ae9c2bfc2dfa778712da05b118bbceb3727316ef`.
Only ordinary main countdown and English null-talker 0x1043 authority moved,
at their existing late HUD/message slots. Whole-profile rejection retains the
legacy path; per-transaction packets are immutable and not a second authority.
Eligible-call units, reset/lifetimes, STOP duration, prior-digit warnings and
message entry-mode/DoAction/icon timing are unchanged. All original and new
canonical corpora, direct helper/admission/control checks, both presentation
matrices, CLI/startup/TLUT/tooling gates and focused review pass. PASS3B.md owns
the exact source/build/asset identities, counts, retained failures and limitations.
No actor claim changed and no higher-rate gameplay conversion is claimed.

The accepted Pass 3B inventory covers **2,432 files / 697,043 source lines**,
with **1,524 files / 63,848 candidate lines**. All 429 table actors resolve to
source; one separately registered actor remains outside that table. All 430
actor claims remain unclaimed. The four generated files are byte-identical
across two scans at this runtime checkpoint; these are heuristic denominators,
not proof of conversion or exhaustive per-field auditing.

Run from the checkout with the intended Python interpreter:

```powershell
python scripts/native-simulation/inventory.py
```

The standard-library-only scanner reads Git-tracked `.c/.cpp/.cc/.cxx/.h/.hpp/.inc`
files in `soh/src`, `soh/include`, `soh/soh`, `libultraship/src` and
`libultraship/include`. It does not read ROMs or extracted asset packages.
Generated source references under `soh/assets`, extraction tools, and nested
third-party dependency implementations are outside its declared scope. Current
extraction data is not required to run it. Initialize libultraship first.

Committed inventory files:

| File | Meaning |
| --- | --- |
| [inventory/summary.json](inventory/summary.json) | Source/submodule revision, every scanned file's SHA-256 and line counts, explicit rules, limitations and class counts |
| [inventory/timing-candidates.csv](inventory/timing-candidates.csv) | Deterministically ordered path, one-based source line, candidate classes and matching rule IDs |
| [inventory/actors.csv](inventory/actors.csv) | All 429 non-unset entries from `soh/include/tables/actor_table.h`, source mapping and actor description |
| [inventory/unmapped-actor-sources.csv](inventory/unmapped-actor-sources.csv) | Overlay source outside the legacy table: `ovl_En_Partner/z_en_partner.c` |
| [actor-claims.csv](actor-claims.csv) | Human-maintained work ownership/status; regeneration never overwrites it |

Initial snapshot: **2,422 files / 694,621 source lines**, of which **1,516 files /
63,411 lines** match one or more heuristic rules. All 429 table actors resolve to
source. `En_Partner` is separately registered in
`soh/soh/Enhancements/ExtraModes/IvanCoop.cpp:19` and has its own claim row.
Unset table slots are not actors. Overlay files, actor IDs and live instances are
different denominators; actors with variants need multiple fixtures.

The Pass 2 runtime-checkpoint inventory covers **2,427 files / 695,783 lines**, with
**1,521 files / 63,630 candidate lines**, at runtime checkpoint
`68cd6a6520dcc9e3c4147fbc9dec62cd2f702417`. The additional files are replay and
observation seams. The denominator remains 429 table actors plus En_Partner;
all 430 actor claims remain unclaimed, with no conversion implied by a fixture.

The accepted Pass 3A inventory at runtime checkpoint
`b14ea69cea954685803aa8d334a5424f8bdde190` covers **2,430 files / 696,478 lines**,
with **1,523 files / 63,699 candidate lines**. It includes the two observation
headers and the separately reviewed dependency helper; libultraship is pinned
to `c6bbb8c328938c115f4a1cbeaca3d00a4502269d`. There are still 429 table actors,
one separately registered actor and no missing table sources. All 430 claims
remain unclaimed. These counts are heuristic coverage denominators, not proof
of conversion or complete per-field auditing.

The Pass 2 checkpoint's candidate class counts overlap: A 28,129; B 7,896; C 3,635;
D 32,702; E 31,929; F 3,234; G 38,370; H 8,002. Class I cannot be diagnosed by regex.
These are **not** numbers of defects, necessary edits or independently verified
temporal dependencies. The scan includes comments, strings, prototypes, reads,
audio sample counters and non-temporal loop indices. It misses aliases, opaque
fields, interprocedural timing and many direct assignments. Never bulk-replace
matches, and never infer rate safety from zero matches.

Line anchors below refer to the baseline; find the symbol again after rebases.
The file hashes make a stale inventory detectable. Regenerate after a source
change, review new/deleted actors, and carry claim history forward manually.
The scanner accepts `--output .tmp/native-simulation-inventory-check` for a local
repeatability check; output outside the checkout is rejected.

## Claim and completion protocol

Claim a narrow subsystem row below, or one actor row in `actor-claims.csv`, before
editing. Record owner and branch/worktree in `owner`; never take another active
claim silently. Complex coupled actors (boss and projectiles, horse and rider,
minigame controller and targets) need a named joint owner or explicit interface
agreement. Shared helpers must have their contracts approved before bulk callers
are changed.

Allowed actor statuses are `unclaimed`, `claimed`, `audited`, `converted`,
`verified`, `blocked`, `excluded`. Representative reading in this document leaves
an actor `unclaimed`. `audited` means the full selected source scope and helper
contracts were inspected, with a field-by-field unit sheet; it is not a test
result. `converted` is an implementation milestone, not advertised rate support.
`verified` requires all gates below. `excluded` must state why and must not make a
scene containing that actor rate-supported. A released claim retains its audit
record and owner history in its linked notes.

Each row has `claim_id`, `owner`, `status`, `audit_commit`, `conversion_commit`,
`classes`, `tests`, `divergences`, `notes`. The actor catalogue resolves a claim
ID to sources. In `notes`, link a review sheet recording:

1. Actor variants/parameters, update/draw/limb callbacks, initialization/reset,
   spawn/death/culling/pause behavior, fields, original units and reader/writer
   sites. Include hidden `unk_*`, function statics and enhancement hooks.
2. Temporal classes A-I, chosen helper/clock for each field and return-value
   semantics. Distinguish a per-step rate from displacement, geometry, impulse,
   collision correction, lookup index or event count. Document scheduling order.
3. Tests/fixtures at 20/60/120 for the admitted Player island; world-20 dependencies, exact 20-Hz reference commit, same-rate repeat
   results, physical-time assertions, event multiplicity/order and scenario
   coverage. Shared mathematics tests alone do not verify an actor.
4. Relevant ND identifiers and evidence class. Record input phase, scene, seed,
   actor parameters, config digest and first divergent timestamp.

To mark `verified`: unchanged canonical 20-Hz trace; deterministic repeated runs
at each supported rate; invariant duration/rate checks with declared quantization
bounds; rendering-rate independence; pause/resume and scene lifecycle tests;
every deviation classified and linked. The canonical harness now exists, with a
strictly narrower envelope than these eventual multi-rate gates. Compiling or
passing a 20-Hz fixture cannot mark a gameplay conversion `verified`. Update
commit IDs and fixture IDs before closing a claim.

## Semantic inventory: shared infrastructure

The temporary Pass 2 observational claims for SYS-LOOP, SYS-RNG, SYS-AUDIO,
SYS-SAVE-SCENE and SYS-POSE/SYS-MESSAGE/SYS-HUD-TIMERS are closed as completed
instrumentation and bounded validation by `/root` and delegated replay agents on
`mod/native-simulation-rates`. This does not mark those systems timing-converted
or any actor verified. Other rows remain unassigned reconnaissance.
The symbols are starting points, not the full audit boundary.

The implemented instrumentation records full CPU draw, then synchronous test
audio mixing, then presentation before the completed-transaction snapshot.
Native normalized input accepts ports 0-3 and 32-bit held buttons; the corpus
runner deliberately admits only port 0 and 16-bit buttons, matching the audited
edge-mask scope. The Python suite passes 60 tests (29 runner, 22 math and 9
acceptance checks), and the final executable passes 36 native CLI checks. The
final 36-run corpus, 21-run presentation/trace matrix, negative control, ordinary
startup smoke, complete coupling analysis and diagnostic-change comparison pass;
TESTING.md/PASS2.md bind those results to their exact inputs and coverage.
Corpus 02's startup AV was unresolved at the Pass 2 checkpoint and remains a
failed historical fixture. Pass 3A reproduced a resource-backed TLUT over-read
and records its reviewed dependency correction and passing rebuilt-game
acceptance in PASS3A.md; the old log cannot prove the same resource identity.
Neither that renderer correction nor a successful startup is a timing conversion.
Pass 3A adds a bounded, reviewed main-countdown
and English 0x1043 message closure, new opt-in state/paint observation and eight
fixtures while preserving legacy authority. `/root` and the delegated HUD,
message and independent-review agents own that observation/design work on the
same branch; it is not a gameplay conversion claim. PASS3A.md owns acceptance
identities, stress accounting and final gates. ARCHITECTURE.md is the exact
next High implementation contract for SYS-HUD-TIMERS and SYS-MESSAGE, including
the future direct CPU-helper repetition gate and its separate purity receipt.
The existing presentation-FPS matrix does not repeat these helpers and cannot
close that gate. No purity test or authority extraction is implemented by this
design entry.
The fixed executable passes the original canonical gates and 192 startup trials.
Pass 3A adds 24 accepted draw-state runs, strict phase/input/paint/coupling checks
and 72 exact presentation/trace comparisons; the full Python suite passes 105
tests. This accepts the observation/design checkpoint, not the future extraction.
Other branches of those subsystems and all higher-rate conversion claims remain
unassigned. All 430 actor claims remain unclaimed.

| Claim ID / scope | Baseline anchors and observed semantics | Classes / conversion contract and verification focus |
| --- | --- | --- |
| SYS-LOOP: loop, retraces, rates, interpolation | `z_play.c:657` `Play_Update`, `:1652` `Play_Main`; `R_UPDATE_RATE` changes inside transitions; detailed platform flow in ARCHITECTURE | A,D,E,G,H,I. Distinct simulation, presentation, script and audio clocks; original Draw side effects are part of baseline transaction. No global `R_UPDATE_RATE` replacement. |
| SYS-ACTORS: actor scheduling | `soh/src/code/z_actor.c:2580` `Actor_UpdateAll`; freeze countdown `:2680`, color filter `:2695`, category/list traversal, talk/ocarina suppression and culling | D,E,G,H,I. Preserve traversal/spawn/death visibility and one-update latency. Admitted Player dependencies execute per Player step; unrelated actors deliberately remain 20 Hz, with explicit event bridges. Pause/freeze semantics are per timer domain. |
| SYS-PHYSICS: shared integration | `z_actor.c:1287` position adds velocity times `R_UPDATE_RATE*0.5` plus unscaled collision displacement; `:1295` applies gravity then clamps vertical velocity; `:1336` animation root motion | A,B,H,I. Differentiate authored velocity, once-only impulses, already-integrated root displacement and collision correction; avoid double scaling. Establish integration contract before actor callers. |
| SYS-MATH: step/approach/smoothing | `soh/src/code/z_lib.c:24` scaled integer step; `:49/:72` unscaled integer/float steps; `:386/:425/:514/:554` smooth/approach and integer angular variants | A,C,H,I. Multiplicative contraction is not a linear rate. Clamped/minimum-step and integer truncation require piecewise semantics plus fractional residuals. Helper return values trigger state changes. |
| SYS-ANIMATION: animation, morph and pose | `soh/src/code/z_skelanime.c:1178/:1215` Link rate `R_UPDATE_RATE/2`; `:1567/:1648` ordinary rate `R_UPDATE_RATE/3`; `:1413` `Animation_OnFrameImpl`; `Actor_UpdatePosByAnimation` | A,D,E,G,H,I. Preserve each authored timebase, reverse/wrap/terminal events and root-motion displacement. Morph blending is not uniformly exponential. Trace previous/current frame and event crossings. |
| SYS-COLLISION: AT/AC/OC, background, platforms | `soh/src/code/z_collision_check.c:2903` OC and `:3089` damage; `soh/src/code/z_bgcheck.c:2943/:2961/:3013` DynaPoly state; `z_actor.c:2710/:2731` BG setup and transform-history ordering | B,E,H,I. Collision displacements are geometric corrections, not velocities. Keep registration, resolution, hit consumption and platform previous/current transform ordering. Higher-resolution contacts need ND classification, not forced old tick delays. |
| SYS-POSE: draw authority / culling | `soh/src/code/z_player_lib.c:1789` `Player_PostLimbDrawGameplay`; weapon quad vertices `:1527/:1587`; Gohma limb callbacks below | E,F,G,H,I. Draw generates collision, body/focus/attachment positions and RNG side effects. Equivalent CPU pose work is prerequisite for skipping legacy command generation; retain old phase ordering at 20 Hz. |
| SYS-CAMERA: gameplay camera | `soh/src/code/z_camera.c:155` vector LERP; `:1247` clamp; `:1645` start-swing timer; `:1789` random wiggle; camera state families throughout file | A,C,D,E,F,H,I. Camera behavior affects input-relative motion, culling and targeting. A display-only camera interpolation does not convert camera simulation. Audit quakes and subcameras too. |
| SYS-EFFECTS: lifetimes and particle integration | `soh/src/code/z_effect_soft_sprite.c:254` acceleration then position; `:270` life decrements before callback; effect overlays and `z_effect.c` | A,B,C,D,F,G,I. Emission rate and lifetime are separate from particle motion. Allocation failure and RNG calls can influence later gameplay; cosmetic classification needs evidence. |
| SYS-TEXTURE: texture/environmental animation | `soh/src/code/z_scene_table.c:126` draw config reads `gameplayFrames` for texture offsets/modulo; arrow spin `ovl_En_Arrow/z_en_arrow.c:537`; `Gfx_TwoTexScrollEx` also carries interpolation information | A,G. Preserve physical phase/period using a phase representation; do not apply rate scaling to both authored offsets and render interpolation. Discrete texture indices remain discrete events. |
| SYS-ENV: day/night/weather | `soh/src/code/z_kankyo.c:934/:936` dayTime additions (night multiplier); `:1816` lightning has a 10% per-update jump plus random fractional drift; `z_scene.c:356` time reset | A,D,E,F,G,I. Preserve freeze/time-speed/Sun's Song and modular wrap. Lightning is a compound stochastic process, not merely one Bernoulli emission. See ND-003. |
| SYS-CUTSCENE: scripted frame identity | `soh/src/code/z_demo.c:225/:231` frame interval and equality; `:432` rewind; `:1647` dialogue-sensitive hold; `:2073` frame increment | A,D,E,G,I. Authored frame labels remain stable. Cross due boundaries exactly once, in original command order; account for hold, seek, rewind and same-frame commands. Never treat a repeated floor of script time as a new event. |
| SYS-MESSAGE: dialogue and ocarina UI | `soh/src/code/z_message_PAL.c:949/:1280` text Draw; `:1599` textDelayTimer consumption; `:3148` DrawMain and `:3551` state timer; textbox opening indexes coefficients using stateTimer `:272` | A,D,E,G,I. Some timers are also array indices. Draw drives text/gameplay progression. Separate logical script steps, continuous opening motion and button edge/cooldown; test textbox completion affects actor state once. |
| SYS-HUD-TIMERS: minigame/environment countdown | `soh/src/code/z_parameter.c:5102` Interface_Draw, static timers `:5144`; `:6052` countdown and `:6120` countup convert 20 calls into a second | D,E,G,I. This is authoritative duration inside Draw. Convert seconds and visual sliding separately; retain pause/gameover gating. Assert one decrement per second, not per display. |
| SYS-TRANSITION: fades/wipes/room changes | `soh/src/code/z_fbdemo_fade.c:44` adds integer updateRate to fadeTimer; `z_fbdemo_wipe1.c:70` uses `(speed*3)/updateRate`; transition caller `z_play.c:925` | A,D,E,G,I. Rate parameters have inconsistent directions/units; a global ratio breaks at least one family. Audit every transition implementation and entry-speed/door/camera lifecycle. |
| SYS-AUDIO: gameplay audio events and device audio | Player animation SFX `z_player.c:1819`; Epona sound-frame cursor `z_en_horse.c:611`; `soh/src/code/audio_heap.c:829` sample/update configuration | D,E,F,G,I. Timestamp gameplay one-shots and sustain/refresh events; do not multiply audio sample/mixer timebases by gameplay rate. Ocarina/rhythm timers need joint message/audio ownership and sample-clock traces. |
| SYS-RNG: deterministic random streams | `soh/src/code/code_800FD970.c:4/:12/:19/:28` shared static LCG plus variable-state versions `:47`; Gohma Draw below | E,F,G,I. Same seed is insufficient when call count/order changes. Retain exact 20-Hz sequence; trace source-site and ordinal before selecting higher-rate opportunity semantics. No global replacement RNG. |
| SYS-SAVE-SCENE: persistence/reset | `soh/soh/SaveManager.cpp` serialized SaveContext sections; `z_play.c:355/:371` Play initialization; `:614/:1136` scene/stat counters; `z_room.c:655` room timer; `Enhancements/ResetGameplayFrames.cpp:9` reset hooks | D,E,G,I. Save files are not snapshots of actor memory, function statics or RNG. Define scene-epoch reset, supported save serialization and rate-setting ownership; never serialize transient fractional accumulators accidentally into old fields. |
| SYS-PAUSE-DEBUG: pause/menu/frame advance | `z_play.c:698/:1116/:1129/:1286` frame advance and paused/gameplay/environment paths; `ovl_kaleido_scope/z_kaleido_scope_PAL.c:1295/:1322` held-page timer; `Enhancements/Cheats/EasyFrameAdvance.cpp:14` counter | D,E,G,I. Gameplay time may stop while UI/transition/audio still progress. Frame-advance must state whether it means one selected simulation step or one reference step. Debug/console mutations must enter the test log. |
| SYS-HOOKS-CONFIG: enhancement interactions | `soh/soh/Enhancements/game-interactor`, `soh/soh/Enhancements`, `soh/soh/ShipInit.hpp`; actor init/update hook points in `z_actor.c:1260/:2699` | All as applicable. Freeze fixture CVar profiles. Re-audit hook timing, gameplay stats, randomizer runtime, cheats and custom actor registration; do not claim default-mode conversion covers every toggle. |

Paths shortened to filenames in the table are under `soh/src/code` unless the
actor overlay is specified. ARCHITECTURE owns the detailed platform main loop,
controller propagation, render interpolation and CVar source map.

## Semantic inventory: Player and coupled actors

`P` below means `soh/src/overlays/actors/ovl_player_actor/z_player.c`.
An actor's CSV claim is not complete until its related shared-subsystem work is
verified. The Player needs a subclaim per row below inside its audit notes.

| Scope | Observed baseline anchors | Classes / required scenarios |
| --- | --- | --- |
| Input, locomotion, rotation, friction | `P:11776` binds `sControlInput` and runs common action logic; `P:4003` deceleration; `P:6273` knockback velocity assignment; `P:14650` slope sliding; shared physics above | A,B,C,D,E,H,I. Stationary/accelerating/running/stopping, turn-in-place, analog thresholds, air control, slopes, surface changes, roll and knockback. Input transitions occur once while held state is available every step. |
| Swimming/diving/surfacing | `P:6962` gravity plus water-dependent modification `:6999`; `:7036` probabilistic bubbles; `:13751` swim state family | A,B,C,D,E,F,H,I. Water entry/exit, surface clamp, iron boots, strokes, dives and bubble cadence; moving water and collision queries are not rates. |
| Ledges, ladders, vines, crawling | `P:4947/:4953` six-count ledge gate; `:5005` three-count gate; `:11388` delay increment; `:13289` ladder dismount; `:13370` crawl animation and sounds | A,D,E,G,H,I. Count-down/up gate phase, continuous contact, grip/release edges, root motion, landing transitions. A fractional timer cannot be used directly as an animation/index field. |
| Targeting | `P:3767` `Player_UpdateZTargeting`; `:3793/:3795/:3855` hold/decrement/reset 5/15 semantics; `z_actor.c:2718` lock-on release | D,E,G,H,I. Hold/switch targeting, talk/first-person transitions, target removal, minimum target persistence. Timer means both duration and state thresholds. |
| Combat / damage / invulnerability | `P:4468` signed intangibility contract; `:11803` negative timer increments versus positive decrement; `:11606` burn with `gameplayFrames` mask `:11665`; `:15037` melee state logic; `z_player_lib.c:1527` weapon swept quad | A,D,E,G,H,I. Damage windows, repeated contacts, shield bounce, jump attack, stick burning and fire damage pulse counts. Do not normalize signed/sentinel values into an unsigned generic countdown. |
| Epona / rider | `ovl_En_Horse/z_en_horse.c:594` speed increments; `:1028` boost convergence divided by remaining timer; `:611` sound cursor; `P:13522/:13701` mount/dismount; `soh/src/code/z_horse.c` initialization | A,B,C,D,E,G,H,I. Acceleration/braking, carrots/regen, jump and slope, fence detection, race waypoints/timing, mount/dismount and horse archery. Rider/horse order and shared attachments are joint invariants. |
| Hookshot | `ovl_Arms_Hook/z_arms_hook.c:191` countdown; `:208` distance/retraction and `:214/:216/:218` step distances; `:242` writes Player velocity; `:259` prior-position sweep | A,B,D,E,H,I. Launch, range timeout, wall strike, retract, pullable actor, Player pull, moving target/platform. Some values named velocity are one-step displacement; convert contract at both ends once. |
| Bombs/explosives | `ovl_En_Bom/z_en_bom.c:104` 70-count fuse; `:176` bounce multiplies velocity by -0.3; `:265` decrement; `:269/:318` exact fuse events; `:323` bit-pattern blink; `:356` explosion duration | A,B,C,D,E,F,G,H,I. Fuse length, pickup/throw, rebound, impact detonation, chain reactions, ten-count explosion damage interval and optional random fuse. Impact restitution is per collision, not exponential time decay. |
| Other projectiles | `ovl_En_Arrow/z_en_arrow.c:243/:247/:258` lifetimes; `:308/:429/:438` countdown paths; `:313` gravity threshold | A,B,D,E,G,H,I. Arrow/nut seed variants, parent detach, dynamic target attachment, impact and stuck-arrow expiry. Expand to boomerang, magic, enemy projectiles through actor claims. |
| Doors | `ovl_En_Door/z_en_door.c:266/:272` scaled angular step; `:281` lock timer; `:285/:297` animation-triggered opening/closing sounds | A,D,E,G,H,I. Locked/open, two directions, interrupted interaction, room crossing and coupled Player door timer. Collision and sound share animation crossing semantics. |
| Moving background example | `ovl_Bg_Hidan_Fslift/z_bg_hidan_fslift.c:77` 40-count wait; `:102/:113` 4-unit position steps; child position `:62` | A,D,E,H. Ride up/down, hookshot attachment, child block, step off/on and obstruction. Translation inheritance must use transform delta exactly once. |

## Semantic inventory: actor families and high-risk interactions

| Scope / representative inspected | Concrete dependency and implication | Required next audit boundary |
| --- | --- | --- |
| NPC dialogue/blinking; Talon `ovl_En_Ta/z_en_ta.c:1067/:1091/:1170` | Blink countdown, randomized wait and action timer; same actor owns minigame logic. A,C,D,E,F,G. | Entire NPC action graph, not only walk speed; child actors and dialogue/cutscene callbacks. |
| Enemies; Stalfos `ovl_En_Test/z_en_test.c:394/:406/:481/:655` | Attack opportunity conditioned on gameplayFrames parity, random branch, random delay and timer modulus. D,E,F,G,I. | Movement, defense, animation hit windows, damage, AI opportunity semantics, spawn/death/loot. Shared seeded RNG alone cannot guarantee cross-rate identity. |
| Bosses; Gohma `ovl_Boss_Goma/z_boss_goma.c:604/:1100/:1747` | Approach smoothing and random actions gated by `%4`/`%16`. A,C,D,E,F,G. | All boss/limb/child actors jointly; intro/death cutscenes and room-clear effects. Boss timings are not reducible to the shared actor movement helper. |
| Gohma Draw `:1968/:1987/:2087/:2104/:2134` | Random limb color consumes gameplay LCG; post-limb callback writes focus and collider spheres. E,F,H,I. | Trace even when visually cosmetic; preserve original Draw culling and callback ordering at 20 Hz. |
| Fishing `ovl_Fishing/z_fishing.c:357/:374/:2157/:2167` | File-static weather/lure/rod state; lure timer doubles as oscillation phase, modulus and duration; probabilistic fish/effect logic `:1029/:1110`; scroll `:1425`. All relevant classes. | Whole pond scenario, line/rod physics, cast/reel input timing, fish AI, bites/catch window and rewards; fresh-process reset first. |
| Shooting gallery `ovl_En_Syateki_Itm/z_en_syateki_itm.c:161/:253/:342` | Round delay 50/30, target delay 60, two independent countdowns and repeated Player freeze writes. D,E,G,H. | Controller + target actors + projectiles + reward transition. Exact opportunity/order assertions in addition to total round duration. |
| Cucco game `ovl_En_Ta/z_en_ta.c:695/:738/:771/:811` | Capture grace countdown, ten-second warning, exact intro timer equality, HUD timer and random child starts. D,E,F,G,I. | Talon, three cuccos, carry/contact rules and HUD timer side effects as one fixture. |
| Race/horse archery/bowling and other minigames | Enumerated actors and machine candidates; not individually hand-audited in this pass. | Explicit actor/controller IDs and all variants before claiming support. No inference from shooting gallery or fishing coverage. |
| Other bosses/enemies/NPCs/environment actors | Every table actor has a claim row; source candidates are available. No manual-complete claim. | Batch by shared temporal contracts after one representative passes differential gates, then verify each row. |

## Conversion ordering and exit accounting

Infrastructure/20-Hz reference capture and Draw-side-effect mapping come first.
Then shared units/math/animation and Player/camera/collision, then actor lifecycle
and simple actor cohorts, then bosses/minigames/RNG-sensitive scripts. Follow
EXECPLAN for precise phase gates; the table is not permission to implement
higher-rate behavior in this reconnaissance pass.

Read totals directly from `actor-claims.csv` rather than manually editing a
percentage. A Player island is eligible only when every reachable interaction
has passed its Player-rate, preserved-world20 or event-bridge gate; unrelated
actors may retain their complete canonical paths. Excluded interactions must be
guarded explicitly. Global scene conversion is not a pilot prerequisite.
Enhancements, Master Quest,
randomizer options and custom/mod actors are separate coverage dimensions.
An actor cannot be declared converted merely because it calls a converted
movement helper; countdowns, event opportunity, Draw writes and RNG remain.
# Pass 4B partial QA/movement/static-wall checkpoint

The subsequent targeting/fallback checkpoint preserves queued logical input
across capability scope changes and validates next-Player friendly targeting
outside the unbridged contact region. Six fallback runs and six corrected target
runs pass; earlier missing/occluded targeting coverage is retained and excluded
from acceptance. See PASS4B.md. Final canonical/render gates remain pending.

Player step/next-world QA matches continuous execution; eight representative
world opportunities stay once per world transaction. Focused movement and
static-wall cases pass at 60/120; the wall adapter includes Links House NORMAL
fixed-eye control only. Evidence, precise totals and remaining gates are in
PASS4B.md (`build/pass4b-19`, `build/pass4b-21`). Not full Pass 4B acceptance.
