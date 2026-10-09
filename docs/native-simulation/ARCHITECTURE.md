# Native simulation architecture

## Pass 4B implementation in progress

`PlayerSchedulerCore.hpp` owns fixed 6/2/1-quanta interval ordering. Internal
fixture dispatch now runs a narrowly gated Player closure at intermediate
boundaries; public selection and full Pass 4B acceptance remain pending. The synchronous
`PlayerAnimationQueue` isolates intermediate Player requests from the drained
world queue; the shared-boundary Player still uses the original world queue and
drain location. Queue flags are saved/restored, root deltas are applied once with
their original units, and overflow is reported without a native fault.

The animation adapter identifies only the bound Player's main/upper skeletons.
An admitted high-rate context will select authored gain `stepQuanta / 4`; the
canonical context keeps the existing expressions. Actual before/after frame
intervals distinguish loops, endpoint clamps and held morph frames. Marker queries
are pure; logical SFX, equipment-change and lunge consumers have distinct event
identities even when they share an authored frame. These high-rate branches run
only after the whole-profile predicate succeeds. PASS4B.md
records partial validation; this is not a claim of playable high-rate support.

The prepared motion adapter owns velocity-to-displacement conversion and the
unclamped affine gravity continuation. Collision corrections and root deltas are
not rates. Explicit angle fields keep signed fractional steps. Six-quanta owners
retain combo/target countdown and pose-extension/blur opportunities. World guards
cover selected interface, RNG, sequence and floor-audio work. The intermediate
wrapper omits outer Actor/Player updates and global collider registration.
Current runtime evidence covers isolated idle and ready-sword slash at 120 Hz;
it does not qualify general movement, contacts or the entire island.

The bounded control-camera entry point separates NORMAL0 control/view updates
from the world camera dispatcher, interface, quake and environment operations.
Its fractional gain, angle and timer owners reset with mode/scope/lifetime.
Player head/focus angles also have explicit fractional owners. Intermediate input
has a port-zero peek/consume path that retains unsupported/menu edges and avoids
world retrace/rumble callbacks. Input delivery scope can be rebound to the same
live Player without discarding queued logical edges. The host merges Player and
render deadlines. Five immutable intermediate packets fit within one synchronous
world draw arena; presentation changes only its pointer to the latest committed
packet. Admission loss withholds remaining intermediate starts and resumes the
legacy path at the next world boundary. The canonical path still uses the
original engine transaction. QA stepping and remaining acceptance are unfinished.

## Pass 4A temporal foundation

`PlayerTemporalCore.hpp` supplies exact rate/time/context and opportunity types;
`PlayerTemporal.cpp` labels original canonical work through an observational
adapter. Player/world elapsed clocks are distinct, step intervals share the
120-quanta availability timeline, and world work remains six quanta. Generation
ownership covers scenes, actor reuse, equipment/admission and authored action/
animation/attack changes. No sidecar value drives movement, animation or collision.
Capability is canonical-only even when an isolated test requests 60/120.
Player creation/destruction cancels only its own in-flight temporal interval;
an open world interval belongs to the world/scene owner and still commits.

The replay QA gate admits a whole transaction before input and commits after
full graphics/audio work; it never uses the collision-suppressing frame advance.
Inspection is separate from preserved snapshots/traces. Input edge delivery and
contact records are foundations, not active high-rate scheduling or bridging.
PASS4A.md records contracts and validation; `player-temporal-units.json` is the
bounded machine-auditable unit review. Existing late-slot extraction below remains
authoritative. High-rate scheduling requires a separately authorized pass.

## Pass 3D accepted canonical extraction

The admitted Player path now splits the existing late skeleton slot into
`Player_AdvancePoseContactsLegacy` and `Player_DrawPosePresentation`. The former
performs the original override/post order once through a Player-only traversal;
the latter emits captured meshes/matrices and the child bracelet without reading
Player or calling gameplay. Original versus selected limb lists remain distinct.
Live IK joints, pose outputs, sword history/quads, shield registration, combo
extension and blur remain canonical authority. Unsupported profiles execute the
unchanged general walker and callbacks. See PASS3D.md for the complete preflight,
packet lifetime and validation evidence; no Player cadence has changed.

## Pass 3B accepted bounded extraction

The bounded driver selects `Interface_IsCountdownProfileAdmitted` at the old
timer slot. `Interface_AdvanceCountdownLegacy` owns the main state/seconds,
hidden call counters, integer XY slide, retained digits and ordered warning
requests. `Interface_DrawCountdownPresentation` writes only its caller's
display list and paint observation. The sibling switch remains the fallback.
Counter units are eligible legacy HUD calls; process initialization, setters
and STOP lifetime are unchanged.

`Message_IsPlainTextProfileAdmitted` validates English null-talker 0x1043,
the complete resource fingerprint/control grammar, relevant default settings,
admitted modes and the full decoded page before mutation. Opening, growth,
next-page and closing modes do not read stale decoded storage. Plain traversal,
page/DONE transitions, SFX ingress, END icon/DoAction and the entry-mode icon
timer advance in `Message_AdvancePlainTextLegacy`. Update-owned work stays in
Message_Update. View allocation and shared icon-flash evolution run once.
`Message_DrawPlainTextPresentation` emits packet-based textbox, glyph, shadow
and icon commands with private observation output.

Both predicates restrict ordinary scenes to Link's House and Kokiri Forest;
pause, transition, freeze, frame advance and NoUI fall back. Countdown also
rejects hazard/subtimer/count-up/minigame and nondefault timer placement.
Messages reject other IDs (including 0x305F), talkers, choices, nonblack boxes,
nondefault text-speed/skip/spacing/color settings, language changes and unsupported
control/mode profiles. Production admission is independent of native test mode.

Packets are stack-owned and consumed synchronously in the original late slots.
Texture references retain existing frame resource lifetimes. The HUD still sees
the old DoAction state before message END. No diagnostic phase labels are added
to the canonical trace. Global draw purity and higher-rate gameplay remain
unimplemented. The complete bounded acceptance is recorded in PASS3B.md.

Status: Phase 1 now implements an opt-in canonical 20-Hz replay/observation path.
60/120-Hz Player simulation and broader draw-authority extraction remain proposals;
30-Hz gameplay is deferred;
the bounded countdown/message extraction above is accepted. Executed
build/corpus outcomes are recorded separately in TESTING.md. Original numbered
source anchors refer to upstream `HarbourMasters/Shipwright` commit
`9eafd15fe1382c5a41e881f1b6ea87345c797d18` and its pinned `libultraship` submodule;
re-resolve symbols in the instrumented source. The build receipt records the
exact submodule identity.

Read this with [TIMING_SEMANTICS.md](TIMING_SEMANTICS.md), [TESTING.md](TESTING.md), [CONVERSION_LEDGER.md](CONVERSION_LEDGER.md), [RNG_AUDIO_AUDIT.md](RNG_AUDIO_AUDIT.md), and [EXECPLAN.md](EXECPLAN.md). Sections labelled **Observed** describe inspected engine behavior; **Implemented Phase 1** describes the test-only boundary. Sections labelled **Proposed** remain contracts to implement and test.

## 1. Architectural decision

Pass 3C's [ownership and scheduling contract](PASS3C.md) and
[machine-readable admission graph](player-island.json) refine the first pilot.
They explicitly split PLAYER_RATE, WORLD_20HZ, EVENT_BRIDGE, PRESENTATION_ONLY
and NOT_YET_ADMITTED dependencies. Their canonical reference pass is accepted
under the user's explicitly reduced matrix/startup scope recorded in PASS3C.md.
Player authority still occupies its original late draw slots. A listed target
rate is not implemented support.

Use one deterministic fixed-step Player/control island with target rates `{20, 60, 120}`, over unrelated world logic retained at 20 Hz. Represent elapsed time on the existing 120-quanta-per-second timeline. Keep rendering and audio ownership explicit. Preserve the executable canonical compatibility path and authentic menu/transition cadences. Runtime 30-Hz gameplay is deferred; cheap s=2/3 mathematical tests are not runtime support. See PASS3C.md for the bounded dependency graph and exact boundary schedule.

Do not implement this by replacing `R_UPDATE_RATE`, speeding up the outer loop, or invoking Player several times after an intact actor pass. Input, action, movement, gameplay pose, weapon/contact state and necessary camera/static collision must form a dependency-closed Player step. Enemy AI, NPCs, scripts and unrelated actors remain world-20 by design. Global high-rate world conversion is not a prerequisite. The UI must distinguish Player Hz, world Hz and rendering FPS.

The hardest prerequisite is extracting **authoritative work from draw functions** without changing its position in the original transaction. Engine drawing currently performs collision registration, state-machine advancement, timers, culling decisions, audio calls, and RNG draws. A conventional `update(dt); draw(alpha)` refactor is unsafe until these dependencies are measured and separated.

## 2. Observed desktop execution path

```text
mainproc                                      soh/src/code/main.c:141
  Graph_ThreadEntry                            soh/src/code/graph.c:509
    while WindowIsRunning
      RunFrame                                soh/src/code/graph.c:437
        GameState_Init / overlay changes      soh/src/code/graph.c:453
        Graph_StartFrame                      soh/src/code/graph.c:476
        PadMgr_ThreadEntry                     soh/src/code/graph.c:478
        Graph_Update                          soh/src/code/graph.c:480
          reset display-list arenas           soh/src/code/graph.c:285
          GameState_ReqPadData                 soh/src/code/graph.c:296
          GameState_Update                    soh/src/code/graph.c:297
            OnGameStateMainStart              soh/src/code/game.c:269
            gameState->main                   soh/src/code/game.c:271
              Play_Main                       soh/src/code/z_play.c:1652
                Play_Update                   soh/src/code/z_play.c:1684
                interpolation StartRecord     soh/src/code/z_play.c:1689
                Play_Draw                     soh/src/code/z_play.c:1690
                interpolation StopRecord      soh/src/code/z_play.c:1691
            GameState_Draw and common work    soh/src/code/game.c:342
            OnGameFrameUpdate; frames++       soh/src/code/game.c:357
          Audio_Update                        soh/src/code/graph.c:392
            ocarina / SFX / sequence control   soh/src/code/code_800EC960.c
        Graph_ProcessGfxCommands              soh/src/code/graph.c:487
          audio-thread notification           soh/soh/OTRGlobals.cpp:1795
          rational interpolation frame count  soh/soh/OTRGlobals.cpp:1802
          RunCommands                         soh/soh/OTRGlobals.cpp:1842
            HandleEvents once for batch       soh/soh/OTRGlobals.cpp:1775
            interpolate matrices; present     soh/soh/OTRGlobals.cpp:1783
          wait for audio processing           soh/soh/OTRGlobals.cpp:1847
        ProcessSaveStateRequests; return      soh/src/code/graph.c:492
```

This is a synchronous port path. Although `main.c` retains N64 thread setup, `osCreateThread` and `osStartThread` have empty implementations in `soh/soh/stubs.c:112` and `:115`. Message receive does not honor a blocking wait on an empty queue (`libultraship/src/libultraship/libultra/os_mesg.cpp:39`). `PadMgr_ThreadEntry` explicitly calls one retrace handler and breaks (`soh/src/code/padmgr.c:425`); its old message dispatch is under `#if 0` at `:428`. Do not build the new scheduler on the assumption that the original N64 IRQ thread controls desktop simulation.

### Implemented Phase 1: the observational transaction

`NativeSimulationTest_Init` recognizes `--native-sim-test <fixture.json>` and
`--output <fresh-directory>` before `OTRGlobals` initialization; `--trace` enables
verbose JSONL attribution. Without the test flag, observers return immediately
and ordinary initialization, clocks, input, audio worker and rendering remain on
their original paths. There is no native-rate settings control in this pass.

The fixture boot recipe initializes a semantic debug save and enters the existing
Play overlay through `GameState_Init`. It does not restore heap bytes or a runtime
savestate. A bounded number of setup transactions precedes tick zero. Optional
Player placement, a real HUD timer initializer, message notice, loaded-object
Ice Keese spawn or real ocarina memory-note initialization runs once at that
boundary. Separate processes own all static and
singleton reset state.

`RunFrame` calls `NativeSimTest_BeginFrame` immediately before PadMgr and
`NativeSimTest_EndFrame` after graphics/audio completion and save-state request
processing. A measured transaction must retain `R_UPDATE_RATE == 3`, exactly one
`Play_Update`, and exactly one complete `Play_Draw`; failure is explicit. Tick
identity advances by six 120-Hz clock units after the complete transaction. This
clock labels real 20-Hz engine work and does not introduce hidden substeps. The
initial state is tick zero; later snapshots are end states at `tick * 6` units.

The normalized provider is in `NativeSimulationTestInput.cpp`, selected by
`PadMgr_HandleRetraceMsg` before hardware reads. Every due timestamped transition
passes through the existing `PadMgr_ProcessInputs` edge accumulator. Several
transitions at one boundary preserve final held state and accumulated press and
release masks; no additional gameplay update is inserted. The ordinary consuming
pad request still delivers and clears edges. Host frame hotkeys are ignored in
test mode. The fixture timeline, not SDL polling or renderer speed, supplies input.

Phase observations bracket update, CPU draw, actor draw, HUD draw, message draw,
`Audio_Update` control, buffer production and GPU presentation. Stable actor
identity uses scene epoch and spawn ordinal, while snapshots retain existing
category/list order. Nested actor observer scopes preserve attribution through
spawns. These observations do not move contact registration, culling, timers,
effects or RNG out of their original CPU-draw callbacks.

The audio control stage was implicit in the first call-graph draft and is now
explicit: `Graph_Update` calls `Audio_Update` after CPU command generation and
before `Graph_ProcessGfxCommands`. It advances ocarina/controller state and
processes sound/sequence commands. `GameState_Init` also invokes it once. The
later audio blocks advance synthesis/sequencers; these are distinct stages.

The semantic schema contains selected explicit Player, camera, actor-base,
collision, world, RNG and audio fields; floats carry exact IEEE-754 bits. It is
not a generic serializer for every actor action or private timer. Cumulative
RNG/logical-audio fingerprints work without verbose logs. Presentation counts
and phase labels remain diagnostic trace records and do not independently alter
authoritative event hashes. The runner supplies SHA-256 and field-level diffs.
Coverage and passing runtime evidence must be read from TESTING.md, not inferred
from these hooks.

Legacy scheduler evidence still explains the divisor: graphics tasks copy `R_UPDATE_RATE` into framebuffer `updateRate` (`soh/src/code/graph.c:264`), scheduler swap preparation copies it to `updateRate2` (`soh/src/code/sched.c:25`), and retrace processing decrements that countdown before swaps (`:319`, `:338`). On the desktop path, `Graph_ProcessGfxCommands` explicitly derives the source cadence as `60 / R_UPDATE_RATE` (`soh/soh/OTRGlobals.cpp:1807`). Rendering backends pace presentation; e.g. DXGI `IsFrameReady` advances its timestamp (`libultraship/src/fast/backends/gfx_dxgi.cpp:791`, `:823`) and `SetTargetFps` changes that timestamp's denominator (`:1013`). Audio completion also participates in pacing. These are source facts, not a measured guarantee of wall-clock precision or jitter.

### Existing interpolation does not resimulate gameplay

`GetInterpolationFPS` reads the interpolation CVar and optionally display refresh/VSync limits (`soh/soh/OTRGlobals.cpp:1022`). `Graph_ProcessGfxCommands` raises presentation to at least the authentic source cadence and treats the value 20 as an "original" sentinel (`:1810`). Its integer phase accumulates render samples between game transactions (`:1818`), resetting when FPS or divisor changes (`:1814`).

`Play_Draw` records matrix operations once per source transaction. `FrameInterpolation_StartRecord` moves the current recording into the previous recording and initializes a new operation tree (`soh/soh/frame_interpolation.cpp:452`). Recording is enabled by **render FPS != 20**, not by a simulation-rate comparison (`:457`). Child labels contain pointers and caller-supplied epochs (`:467`); camera discontinuities increment an epoch (`:486`). The interpolation function walks the previous/current operation trees and returns matrix replacements (`:444`). `RunCommands` re-executes the same display list with those replacements; the endpoint uses the original matrices (`soh/soh/OTRGlobals.cpp:1783`). `Fast3dWindow::DrawAndRunGraphicsCommands` may skip a dropped presentation, otherwise runs GUI/interpreter/swap (`libultraship/src/fast/Fast3dWindow.cpp:196`). It does not call `Play_Update`.

Consequences: increasing the existing FPS slider cannot improve authoritative collision or state-decision cadence. Rendering lower than native simulation is not supported by the present `max(original, target)` policy. Matrix trees are neither whole-world snapshots nor a save state. Pointer lifetimes and graphics-arena lifetimes must be handled before buffering or asynchronously consuming recordings.

## 3. Observed authoritative order and clock domains

### Gameplay transaction

`Play_Update` is controlled first by frame advance (`soh/src/code/z_play.c:698`). Its order matters:

1. Count input statistics, handle transition state machines, and process pause entry (`:703`, `:757`, `:1110`). A transition can write the authentic divisor during this same call (`:766`).
2. Reset animation work queues and update object availability (`:1119`, `:1122`).
3. If gameplay is not paused/debug-frozen, increment `gameplayFrames` and gameplay statistics (`:1126`). Finishing-blow freeze has its own decrement/parity branch (`:1140`).
4. In the ordinary active branch, run room work, AT collision, OC collision, and damage; clear collision registrations (`:1155` through `:1167`). These consume registrations prepared during the preceding transaction, including draw-produced geometry.
5. Run `Actor_UpdateAll` unless all actors are halted (`:1171`), then cutscene processing (`:1176`, `:1179`), regular effects (`:1182`), and soft-sprite effects (`:1185`).
6. Run room debug work, skybox work, pause/game-over/message state as appropriate, interface update, queued animation work, sound sources, letterbox, and fade (`:1194` through `:1253`). Some of this continues when the actor/world branch is paused.
7. Outside that gated block, invoke the camera hook, update non-active cameras followed by the selected camera when allowed, then environment (`:1264` through `:1286`). A debug camera can update while paused.
8. `Play_Main` then builds draw commands and executes draw-side state mutation (`:1689`). Common `GameState_Update` work and its frame hook/counter follow (`soh/src/code/game.c:342`, `:357`).

Reordering collision after actor updates or moving draw-derived weapon geometry before collision would change canonical collision latency. More frequent collision at higher native rates will shorten the *real-time* duration of one transaction of latency; that is a resolution consequence to trace, not an excuse to reorder the 20-Hz pipeline.

### Actor scheduling and platform transforms

`Actor_UpdateAll` spawns setup actors once, then traverses category lists in order (`soh/src/code/z_actor.c:2596`, `:2630`). Category order is switch, background, Player, explosive, NPC, enemy, prop, item action, miscellaneous, boss, door, chest (`soh/include/z64actor.h:435`). Within a category it follows the linked list, not actor ID order.

Initialization, object-load readiness, ocarina/talking suppression, actor death, culling, freeze countdown, targeting, color-filter countdown, update hook, dynamic collision interaction, and damage reset all affect whether and how an actor runs (`soh/src/code/z_actor.c:2641` through `:2705`). `prevPos` and Player-relative distance/yaw are sampled immediately before that actor's update (`:2672`). Background collision is rebuilt after the background category (`:2709`); targeting/title work follows all actors (`:2729`); previous platform transforms are then committed (`:2731`, `soh/src/code/z_bgcheck.c:3013`).

Preserve these dependencies and list mutation semantics. Parallel actor jobs, sorting by stable test IDs, or batching all physics before all AI are outside this design. Stable test IDs identify actors without changing execution order. Moving-platform attachment uses previous/current transforms (`soh/src/code/z_actor.c:1692`, `soh/src/code/code_800430A0.c:76`), so it must advance once per selected world step alongside rider integration, not once per presentation.

### Existing non-20-Hz states

The canonical mode means **upstream behavior**, not forcing every callback to 20 Hz:

| Domain/state | Observed source cadence/control | Required compatibility treatment |
| --- | --- | --- |
| Normal initialized game state | `R_UPDATE_RATE = 3` in `game.c:438`; `60/3 = 20` | Canonical world reference |
| Opening, title/logo, scene select, pre-NMI, sample | Set divisor 1 in their init functions (appendix) | Preserve authentic 60-Hz game-state updates |
| Pause menu entry | Divisor 2 in `z_kaleido_setup.c:60` | Preserve 30-Hz menu cadence and suspended world clocks |
| Pause exit/save menu exit | Divisor 3 restored in kaleido and BetterSaveMenu (appendix) | Restore the appropriate world domain; avoid overwriting selected native Hz |
| Special gameplay framebuffer transition | Divisor 1 at `z_play.c:766`, restored in transition branches | Trace exact entry/exit timing; preserve compatibility domain explicitly |
| Gameplay frozen / paused / frame advanced | Distinct guards in `z_play.c:698`, `:1126`, `:1140`, `:1266` | Separate world, UI, environment/camera, and debug policies |
| Audio engine | Approximately 60-Hz audio work blocks grouped by divisor | Separate audio clock from world step |
| Presentation | FPS/refresh/VSync setting | No authority over elapsed game time |

`gameState->frames` increments after every `GameState_Update` (`game.c:358`) and starts at zero (`:418`). `play->gameplayFrames` increments only in the active branch (`z_play.c:1129`) and is reset during initialization (`:371`). Neither is interchangeable with elapsed time or presented frame count. Hooks tied to these counters need their own ledger entries.

## 4. Observed input, movement, animation, and camera

### Input propagation and consumption

`PadMgr_HandleRetraceMsg` obtains controller data, updates mouse state, then calls `PadMgr_ProcessInputs` (`soh/src/code/padmgr.c:309`). The port's `osContGetReadData` clears the pad array and asks the control deck to fill it (`libultraship/src/libultraship/libultra/os.cpp:29`). The deck pumps SDL input, respects blocked game input, and reads connected ports (`libultraship/src/libultraship/controller/controldeck/ControlDeck.cpp:56`). Controller mapping samples buttons, both sticks, and gyro, then feeds a per-read simulated-input-lag buffer (`libultraship/src/libultraship/controller/controldevice/controller/Controller.cpp:26`). That lag is expressed in sampling calls today; leaving it unchanged changes real-time lag at native rates.

Pad processing accumulates press/release edges. The game-state request uses consuming mode 1 (`soh/src/code/game.c:252`); the request copies the accumulated `Input` and clears stored press/release fields (`soh/src/code/padmgr.c:370`). Other callers use non-consuming mode. Do not repeatedly hand the same `press` bits to multiple catch-up steps, and do not regenerate edges independently for Player and menus.

Player takes a copy of `play->state.input[0]`, or zeroes it for disabled/cutscene control; textbox cooldown masks selected buttons (`soh/src/overlays/actors/ovl_player_actor/z_player.c:12229`). `Player_UpdateCommon` sets the static input pointer, decrements signed invulnerability and other timers, and handles interface/targeting (`:11776`). Movement/gravity, integration, and scene collision occur in that routine (`:11935` through `:11946`); the action function is invoked at `:12085`. Cylinder collider registration follows (`:12147`). Mod/hook effects and shared static variables are part of deterministic state, not merely the Player struct.

### Shared primitives have incompatible implicit units

| Primitive | Observed formula/behavior | Conversion implication |
| --- | --- | --- |
| `Actor_UpdatePos` | Velocity times `R_UPDATE_RATE * 0.5`, plus collision displacement (`z_actor.c:1287`) | Canonical velocity contribution is 1.5 times the stored velocity. Collision displacement is a correction/constraint, not automatically another rate. |
| `Actor_UpdateVelocityXZGravity` | Rebuild horizontal velocity, add gravity once, clamp terminal velocity (`z_actor.c:1295`) | Gravity is not already divisor-scaled. Preserve velocity-before-position order. |
| `Actor_UpdatePosByAnimation` | Add animation translation delta times model scale (`z_actor.c:1336`) | Root-motion delta is already an integrated delta; avoid scaling twice. |
| `Math_ScaledStepToS` | Multiplies angular step by divisor/2 then truncates to `s16` (`z_lib.c:24`) | Needs named source units and fractional residue; `/6` at call sites loses small rotations. |
| `Math_StepToF/S` | Add constant step, clamp when reaching target (`z_lib.c:49`, `:72`) | Linear bounded movement, with separate event-crossing semantics. |
| Smooth/approach helpers | Fraction of current difference with limits (`z_lib.c:386`, `:425`, `:514`, `:554`) | Exponential treatment applies only in the uncapped regime; minimum/maximum steps and integer casts require tests. |
| Link animation | Advance/morph using divisor/2 (`z_skelanime.c:1176`, `:1214`) | Source animation clock differs from general actors. |
| General skeleton animation | Advance/morph using divisor/3 (`z_skelanime.c:1565`, `:1647`) | Use a separate named source clock, not a global replacement of every divisor expression. |
| Animation event crossing | Previous frame inferred from `playSpeed * updateRate` (`z_skelanime.c:1413`) | Advancement and crossing must use the same actual interval, including wrap and reverse playback. |
| Generic `Animation_OnFrame` | Passes literal `1.0f` (`z_skelanime.c:1922`) | Canonical general-animation assumption hidden outside the divisor search. |
| Curve animation | `animSpeed * R_UPDATE_RATE * 0.5f` (`z_fcurve_data_skelanime.c:71`) | Another explicit source clock. |

Do not change the units of all existing velocity fields in one commit. First introduce documented typed/named adapters and audit one dependency-closed subsystem. Mathematical policies, including the ambiguity between a physical integrator and a fractional embedding of the legacy discrete map, live in `TIMING_SEMANTICS.md`.

### Camera is authoritative too

Camera state produces the input direction (`soh/src/code/z_camera.c:7734`), performs background collision and tracks Player displacement (`:7546`), advances quake/distortion (`:7693`, `:7721`), and uses many bounded smoothing functions (`:79`, `:98`, `:117`, `:136`). `xzSpeed` is currently the distance moved in one camera update, not SI velocity (`:7548`). Scaling only camera interpolation will leave camera behavior and movement-relative controls wrong. Keep the gameplay camera on the selected simulation clock; interpolate a presentation copy only. Optional render-rate free-look prediction would need a separate later design and must not silently feed gameplay.

## 5. Observed draw-side authority: a blocking dependency

| Source | Effect performed during command generation | Risk from changing or skipping draw cadence |
| --- | --- | --- |
| `soh/src/code/z_actor.c:3072`, `:3085` | Project actors and write update-culling flags | Actors can stop/start simulation based on presentation frequency |
| `soh/src/code/z_actor.c:3105`, `:3121`; read at `:2665` | Write `isDrawn`; update uses it for deletion/destruction | Lifetime and resource order change |
| `soh/src/code/z_actor.c:3077` | Dispatch actor's flagged audio | Missing/repeated sound events |
| `soh/src/code/z_player_lib.c:1789`, `:1837` | Post-limb draw updates pose and melee geometry | Combat depends on pose work normally called drawing |
| `soh/src/code/z_player_lib.c:1527`, `:1554`, `:1587` | Weapon/shield quad geometry and AT/AC registrations | A headless update-only loop loses combat |
| `soh/src/overlays/actors/ovl_player_actor/z_player.c:12478` | Advance damage flicker counter | Visual time tied to command generation |
| `soh/src/code/z_parameter.c:5102`, `:6052`, `:6120` | Interface draw contains 20-count timer-second progression | Minigame/timed-gameplay durations change with draw cadence |
| `soh/src/code/z_message_PAL.c:1599`, `:3551` | Text drawing/main drawing advance message timing/state | Dialogue may advance without matching simulation policy |
| `soh/src/overlays/actors/ovl_Boss_Goma/z_boss_goma.c:1987` | Limb drawing consumes global RNG for color | Different render preparation can change later gameplay randomness |
| `soh/src/overlays/actors/ovl_Boss_Goma/z_boss_goma.c:2104`, `:2134` | Limb drawing writes focus and collider spheres | AI/targeting/hits depend on evaluated pose |
| `soh/src/code/z_play.c:1322` | Pause framebuffer preparation changes pause render state | Scene/menu state and GPU completion remain coupled |

These are concrete examples, not a complete list of impure draw callbacks. Include actor `Draw`, skeleton override/post-limb callbacks, interface/message drawing, effects/environment drawing, game-state drawing, GameInteractor hooks, and interpreter custom commands in the systematic audit.

**Proposed migration:** before enabling a high-rate Player fixture, capture the legacy transaction trace and extract the Player profile's pose/contact authority at its existing relative position. Do this at canonical rates first. Keep unrelated world command generation, culling, timers and RNG once per canonical transaction. Only Player helpers admitted by the dependency graph may run on Player substeps. Do not run all of `Play_Draw` at 120 Hz merely to obtain fresh poses. Every reachable interaction must be converted, explicitly preserved at world20 through its boundary contract, or excluded from admission; unrelated actor draw purity is not a prerequisite.

Rendering becomes read-only with respect to authoritative state only after this work passes state/RNG/transition comparison. A debug assertion comparing a curated authoritative hash before/after render preparation helps find remaining writes, but a hash is only as complete as its field coverage. Cosmetic RNG and presentation state also require explicit classification; simply deleting their calls from the global RNG sequence breaks the canonical reference.

## 6. Proposed temporal core

Names below are conceptual API names; this pass does not establish a new ABI.

```text
SimulationRate:          enum { Hz20, Hz60, Hz120 }
SimTime:                 unsigned 64-bit integer quanta (120 / second)
SimDuration:             signed/unsigned duration with explicit sentinel policy
PlayerStepContext:       { rate, stepQuanta, start, end, stepId, sceneEpoch }
WorldStepContext:        { stepQuanta=6, start, end, worldId, sceneEpoch }
LegacyClockContext:      { domain, sourcePeriodQuanta, paused, opportunityPhase }
InputTimeline:           timestamped physical/replay samples and edge sequence
SimulationTransaction:  one ordered Player/world transaction at the declared cadence
RenderPacket:            immutable presentation data + interval + epoch
AudioEvent:              logical event ID, simulation timestamp, parameters
```

| Hz | Exact step in quanta | Seconds per step | Relative normal-world step `r = 20/Hz` |
| --- | --- | --- | --- |
| 20 | 6 | 1/20 | 1 |
| 60 | 2 | 1/60 | 1/3 |
| 120 | 1 | 1/120 | 1/6 |

This integer quantum is a representation, **not a mandatory hidden 120-Hz physics loop**. A 20-Hz Player step advances six quanta once; 60 Hz advances two, and 120 Hz one. World work remains due every six quanta.

Common comparison endpoints occur every six quanta = **50 ms**. Compare real authoritative states there and observe intermediate Player response separately. Do not delay input to these endpoints. Generic math may retain s=2/3; there is no 30-Hz gameplay acceptance obligation.

Wall-clock input timestamps may use higher resolution than 1/120 second. Do not quantize human/replay input events to the simulation quantum before assigning them to step boundaries. Keep integer/rational conversion for wall-clock deadlines so repeated truncation of 1/60 or 1/120 seconds cannot accumulate drift. Simulation time advances only by committed fixed steps, never by measured rendering delta.

### Clock ownership

Separate at least these domains:

- **World elapsed time**: advances only when its existing pause/freeze policy admits the corresponding work. Finer domains may be needed for actor freeze or message blocking. A single global boolean `paused` cannot describe upstream behavior.
- **Game-state/UI time**: title, file select, scene select, pause, transitions and game over retain documented authentic source cadences initially; the selected world rate does not redefine them implicitly.
- **Legacy event/source time**: 20-Hz script frames, 30-Hz Link/curve animation units, 60-Hz retrace/audio units, and per-state integer update identities each retain named conversion and event-order rules.
- **Presentation time**: determines sample alpha and submission deadlines only; never increments gameplay counters, consumes simulation RNG, or drains input edges.
- **Audio time**: sample production and sequencing retain their own rational cadence. Logical gameplay audio events carry simulation timestamps.
- **Wall-clock/stat time**: real-world timestamps and optional time-sync enhancements are external inputs, recorded/overridden in deterministic tests.

Duration conversion must preserve when a timer is allowed to decrement. A deadline against global world time is wrong for a timer that freezes with an individual actor or only decrements in one action state. Use domain-relative elapsed counters/deadlines or pause offsets. Retain signed sentinel meanings explicitly.

### Canonical compatibility and partial conversion

At 20 Hz, keep the exact legacy operation order, event ordering, RNG sequence, cast behavior and floating-point expressions wherever possible. A mathematically equal reassociation is not sufficient for a bitwise regression gate. Converted helpers should use an identity/canonical branch when needed to retain the original arithmetic. Do not mutate old counters into elapsed-time aliases before every consumer has been classified.

During conversion, expose high rates only to explicit test fixtures whose entire dependency closure is admitted by the ledger. Keep ordinary gameplay at canonical mode until the scene/subsystem support matrix is complete. The ledger must distinguish inspected, instrumented, canonical-preserving refactored, rate-converted, and admitted/tested. Report a 60/120-Hz Player island over a 20-Hz world accurately; an inadmissible Player profile must visibly reject high-rate admission, never silently report 120 while running Player at 20.

Compatibility mode still uses authentic title/pause/transition cadence. For a state switching `R_UPDATE_RATE` mid-call, first instrument whether the old update's duration and subsequent presentation/audio grouping use the old or new divisor. Preserve observed legacy behavior at 20 before assigning explicit domain boundaries in the generalized scheduler. That subtlety must not be "fixed" incidentally.

## 7. Proposed scheduler and transaction boundaries

The long-term host loop has three independent responsibilities:

```text
pump external/window events and enqueue input samples
while authoritative world/control/audio work is due:
    select the earliest due item using declared legacy phase priority for ties
    if the item is a world step:
        latch the immutable step context and boundary input
        execute the complete ordered simulation transaction
        commit clocks, traces, events, and renderable state
    otherwise:
        execute the due authoritative control/audio work in its declared phase
service asynchronous PCM/device work without advancing authoritative clocks
if a presentation deadline is due:
    present a read-only sample from a valid committed state pair
yield/wait until next service deadline
```

This is a contract, not a prescription to copy this pseudocode before phase extraction. `GameState_Update` currently wraps drawing and common hooks; `Play_Update` alone is not a complete transaction. The actual refactor needs explicit stages for all authority discovered in Sections 3 and 5.

Audio sequencing/RNG can affect the next world step. Catch-up must therefore not run all overdue world steps and defer all audio work until afterward. The merged schedule must preserve the canonical grouping of audio blocks after their owning legacy transaction where specified; nominal 60-Hz block timestamps alone do not authorize interleaving those blocks differently in the 20-Hz reference. Establish that phase contract from traces, then define its native-rate continuation. Only PCM/device work proved independent of gameplay may run asynchronously outside authoritative ordering.

1. **Step context is immutable.** Do not allow GUI CVar writes, an actor, or a hook to change dt halfway through a transaction.
2. **No render-driven step count.** 120-Hz Player/30-FPS presentation executes four Player steps per present on average; 20-Hz world work remains separately due. Rendering FPS is not gameplay support. Rational phase handles non-integer ratios.
3. **Preserve simulation debt.** Bound the number of catch-up steps in one host iteration for UI responsiveness, but do not change dt or drop authoritative steps to catch up. If performance is insufficient, report it and run slower or pause explicitly. Record host suspension/clock discontinuities as a pause/reset of the wall-clock anchor rather than inventing a giant physics dt.
4. **Deterministic offline execution ignores wall time.** Run exactly N admitted transactions or to an exact SimTime endpoint. Rendering and audio device buffering must not determine N.
5. **Ordered event consumption.** External requests (reset/load/config changes) enter at an explicit transaction boundary, not during an actor callback. Same-timestamp ordering is documented and replayed.
6. **Do not run simultaneous worlds initially.** `gPlayState`, `gSaveContext`, static actor/camera/Player state, graphics arenas, and singleton services make isolated process runs the practical comparison mechanism. This is not a requirement to make the entire engine reentrant.

### Input boundary policy

Use a timestamped sample/event queue. For an interval beginning at `t`, consume events with timestamps `<= t` in stable `(timestamp, sequence)` order and latch the resulting level/axis state. Derive a transition edge once for that consuming boundary; never repeat it on subsequent catch-up steps. Define an initial sample at `t=0`. A newly arriving live event is consumed at the first not-yet-committed eligible boundary, and its observed availability must be recorded so replay does not pretend the system knew it earlier.

This policy makes cross-rate input timing explicit: the same event may first affect different boundary times at 20 and 120 Hz. That response-latency difference is expected. Tests that need equal initial conditions may align input changes to common boundaries; latency tests deliberately avoid doing so. Specify treatment of multiple press/release edges between two boundaries (legacy accumulated bit behavior versus an ordered edge queue) and preserve the canonical contract. Do not silently discard short taps or replay one press multiple times.

Controllers, keyboard/mouse/gyro and menu interception must share the policy. Pumping desktop events at render cadence alone is insufficient for native input response. Controller sample history and simulated lag need time-based interpretation or explicitly retained update-count semantics with distinct labels. Test injection must bypass live devices at one documented layer, rather than fighting physical input through several hooks.

### Rate selection and switching

Initially latch the selected rate at a cold launch or controlled fixture/scene reset. A developer-only setting may show a pending request, but effective rate is separate and logged. A reset boundary avoids half-converted timer residues, animation histories, pending input edges, audio grouping and interpolation pairs. Supporting live mid-action rate changes is not required to support three fixed-rate Player modes.

Only after duration/state migration is complete should live changes be considered. Commit a request at an explicit quiescent common timeline boundary (50 ms is common to all supported rates), after completing the old transaction and with no load/save/transition operation in flight. Keep elapsed time and duration deadlines invariant; invalidate presentation history, retain ordered pending input, and record the effective change. Do not round elapsed time forward, rescale velocities blindly, discard remainders, or clear timers. Actor/animation state whose source-unit meaning changes must have a declared migration rule. Freeze/pause domains may require a stronger boundary than elapsed-time alignment alone.

## 8. Proposed rendering and audio separation

### Presentation

After authoritative pose/culling work has been extracted and its consumers converted, commit an immutable presentation snapshot/packet with `(sceneEpoch, stepStart, stepEnd, rate, ownership)`. Current matrix-recording interpolation may remain as an adapter, but its gating must be based on valid samples and selected presentation policy rather than `renderFPS != 20`. The `20 = original` presentation sentinel must not be confused with `worldHz = 20`.

Keep an adjacent committed state pair for interpolation and an explicit camera/actor discontinuity epoch. At render rates below simulation, retain the latest suitable pair; do not interpolate across skipped scenes or reused actor pointers. At render rates above simulation, alpha refers only to that pair. Interpolation normally adds up to one simulation interval of visual latency; choose and measure the scheduling convention deliberately. Extrapolation/input prediction is separate and not required for this pass.

Graphics display lists reference transient matrices, arenas and resources. Do not queue raw `Gfx*` indefinitely while the next simulation step overwrites its backing arena. Either retain/double-or-triple-buffer ownership through consumption or copy the needed immutable data. Repeated presentation must not execute an authoritative hook twice. Test changes in render FPS, VSync, window focus, dropped presentations and resizing against the same simulation trace.

### Audio

Current audio work is explicitly tied to source transactions: one wake at `Graph_ProcessGfxCommands`, then `R_UPDATE_RATE` blocks of approximately 60-Hz audio work (`soh/soh/OTRGlobals.cpp:1040`, `:1059`). The fixed stack buffer only reserves three blocks (`:1065`). At 120 Hz, `60 / Hz` is one half, so a replacement integer divisor cannot work. Running one full audio block every 120-Hz world step would double audio time.

Give the audio engine a rational 60-Hz work clock (two simulation quanta per block) and preserve its sample-buffer control independently from world steps. At 20 Hz that represents three blocks per normal-world interval, at Player 60 one, and at Player 120 alternating due/no-due blocks. Define ordering at shared deadlines and keep the canonical sequence of event dispatch, audio logic and RNG calls. Device fullness may change sample counts as upstream does, but deterministic tests need a controlled sink and recorded logical events; an audio event hash alone does not prove the audio engine deterministic.

Audio is not safely assumed cosmetic: audio RNG/timing and ocarina/minigame decisions require the audit described in `TESTING.md`. Decouple worker buffer production with bounded ownership and synchronization; do not let an audio thread read a mutable global divisor while another thread changes it. Sequencer/control state affecting gameplay must have deterministic ownership even if PCM mixing is offloaded.

**Implemented Phase 1 test envelope:** the worker is not started in test mode.
At its existing wake boundary, the frame thread executes
`AudioMgr_CreateNextAudioBuffer` `max(1, R_UPDATE_RATE)` times with a fixed 528
samples per block, then performs the unchanged GPU submission path. It discards
PCM, retaining actual synthesis/sequencer/control code. Hardware feedback and
thread overlap are therefore controlled test inputs; the production path still
uses its existing worker and 528/560 selection. This is not the proposed rational
sample scheduler: fixed 528 at 60 blocks yields 31,680 samples per nominal second,
so the oracle does not qualify exact 32-kHz playback or audio-device behavior.

Only the gameplay scene seed, audio RNG count operands, lazy ShipUtils default
seed and C-library startup seed/calendar are overridden. Existing RNG arithmetic
and call order remain; the trace names generator API, phase, actor and ordinal,
not raw caller addresses. Audio snapshots include task/reset/command counters,
four sequence players' selected timing/volume fields and the memory-game song's
explicit note fields. [RNG_AUDIO_AUDIT.md](RNG_AUDIO_AUDIT.md) documents the exact
seams and unobserved internals. No global platform-clock substitution is used.

## 9. Configuration and extension surface

Observed existing FPS UI is `CVAR_SETTING("InterpolationFPS")` in `soh/soh/SohGui/SohMenuSettings.cpp:381`, with values from 20 to the presentation maximum and "Original" formatting. Legacy CVar migration already maps `gInterpolationFPS` to `gSettings.InterpolationFPS` (`soh/soh/config/ConfigUpdaters.cpp:62`, `:72`). Console variables persist in config (`libultraship/src/ship/config/ConsoleVariable.cpp:243`, `:279`). `R_UPDATE_RATE` itself is an `s16` debug register, not a validated simulation enum (`soh/include/regs.h:9`, `:51`; `soh/include/z64.h:93`).

Proposed initial key: `gDeveloperTools.NativeSimulationRate`, validated against exactly `{20,60,120}`, default 20, with a separate effective rate in diagnostics. This name is a proposal and can be finalized with the phase-1 API. Invalid persisted values must fall back deterministically to 20 with a diagnostic. The production-facing setting should only appear after admission gates; label simulation Hz independently from rendering FPS. Refresh-rate matching must never change simulation Hz.

GameInteractor hooks, enhancers, cheats, networking, statistics and randomizer logic execute within these paths. Give hooks an explicit step context or documented clock, preserving canonical hook order. Audit mutable config read inside every tick and freeze the test configuration manifest. Do not count hook conversion complete merely because the base C actor was converted.

## 10. Baseline direct `R_UPDATE_RATE` source inventory

At this base, `rg -n R_UPDATE_RATE soh/src soh/soh soh/include -g '*.[ch]' -g '*.cpp' -g '*.h'` reports **59 matching lines**. These are all direct token matches, including one definition, a comment and the debugger row. Register aliases or equivalent constants require the broader semantic inventory; 59 is not the number of timing assumptions.

Phase 1 adds test-only reads for cadence admission and deterministic audio
grouping. The table preserves the original source audit; it is not a current
matching-line total after instrumentation. No existing rate assignment or
gameplay use was converted by those additions.

| File | Lines | Role |
| --- | --- | --- |
| `soh/include/regs.h` | 51 | Macro definition in SREG(30) |
| `soh/include/z64animation.h` | 258 | Documents general skeleton animation source units |
| `soh/src/code/game.c` | 438 | Default divisor 3 |
| `soh/src/code/graph.c` | 264 | Legacy framebuffer/scheduler metadata |
| `soh/src/code/z_play.c` | 766 | Special transition sets 1 |
| `soh/src/code/z_play.c` | 916, 967, 1001, 1032, 1070, 1096 | Transition branches restore 3 |
| `soh/src/code/z_play.c` | 925, 1250, 1253 | Transition, letterbox and fade update arguments |
| `soh/src/code/z_kaleido_setup.c` | 60 | Pause entry sets 2 |
| `soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope_PAL.c` | 4774, 4839 | Pause paths restore 3 |
| `soh/soh/Enhancements/QoL/BetterSaveMenu.cpp` | 150 | Enhanced save-menu path restores 3 |
| `soh/src/overlays/gamestates/ovl_title/z_title.c` | 159 | Sets 1 |
| `soh/src/overlays/gamestates/ovl_opening/z_opening.c` | 40 | Sets 1 |
| `soh/src/overlays/gamestates/ovl_select/z_select.c` | 1899 | Sets 1 |
| `soh/src/code/z_prenmi.c` | 64 | Sets 1 |
| `soh/src/code/z_sample.c` | 91 | Sets 1 |
| `soh/src/overlays/gamestates/ovl_select/z_select.c` | 970, 977, 989, 996, 1002, 1008, 1157, 1165, 1177, 1185 | Menu vertical movement directly proportional to divisor |
| `soh/src/code/z_actor.c` | 1288 | Position velocity multiplier divisor/2 |
| `soh/src/code/z_lib.c` | 26 | Angular step multiplier divisor/2 |
| `soh/src/code/z_fcurve_data_skelanime.c` | 71 | Curve animation divisor/2 |
| `soh/src/code/z_skelanime.c` | 1178, 1199, 1215, 1231, 1441 | Link morph/animation/crossing divisor/2 |
| `soh/src/code/z_skelanime.c` | 1567, 1588, 1632, 1648, 1664, 1681 | General morph/animation divisor/3 |
| `soh/src/overlays/actors/ovl_player_actor/z_player.c` | 8077, 8501, 8818, 10243, 15326 | Player motion/animation/cutscene paths using divisor/2; audit each formula, including inverse use |
| `soh/src/overlays/actors/ovl_Bg_Hidan_Hamstep/z_bg_hidan_hamstep.c` | 265 | Platform frame-divisor calculation |
| `soh/src/overlays/actors/ovl_Boss_Va/z_boss_va.c` | 1989 | Boss calculation divisor/2 |
| `soh/soh/OTRGlobals.cpp` | 1059 | Audio frames per source update |
| `soh/soh/OTRGlobals.cpp` | 1807, 1814, 1845 | Presentation source cadence, change detection and remembered divisor |
| `soh/soh/Enhancements/debugger/valueViewer.cpp` | 61 | Debugger displays mutable s16 "Framerate Divisor" |

The integer storage and `60 / R_UPDATE_RATE` expression cannot represent 120 Hz (`0.5` retraces per update). Setting zero is a divide-by-zero hazard in presentation; changing the register to floating point would still leave countdowns, ABI assumptions and audio grouping incorrect. Introduce the new clock separately, then retire or adapt each legacy use by role.

## 11. Acceptance gates and unresolved architectural risks

Before native Player execution is admitted, require:

1. Repeated canonical runs produce stable explicit snapshots and traces under controlled seeds, clocks, input, assets, configuration and audio sink.
2. Observational instrumentation and authoritative-draw extraction preserve canonical ordering/state/RNG; GUI/window/presentation rate changes do not change the authoritative trace after separation.
3. Shared helper tests cover the three runtime scales and cheap adversarial s=2/3 cases, source-unit families, clamps, wraparound, negative/sentinel timers, zero/one-step durations, and cast residue. Canonical arithmetic retains the original results.
4. Native input edges are consumed once; real-time duration/movement/animation invariants are checked at common authoritative boundaries and event traces explain intermediate differences.
5. Scene/pause/menu/transition/save-load boundaries have explicit clock/reset ownership. No effective rate changes halfway through a transaction.
6. 120-Hz simulation with lower rendering executes all authoritative steps; higher rendering executes no extra authority. CPU/GPU/audio backlog and dropped presentations are observable.
7. Results distinguish conversion bugs, expected resolution consequences, and unresolved design semantics. The ledger and divergences document identify the chosen policy for each admitted fixture.

Highest-risk unresolved details are the full draw mutation inventory, collision-registration phase preservation, global/audio/cosmetic RNG coupling, integer fractional residue storage and reset, camera displacement units, asynchronous resource readiness, and actor/hook update order. Cross-platform bitwise floating-point reproducibility is not proven by selecting a fixed dt; initially pin executable/toolchain/config and compare same-build runs. Performance at 120 Hz is a separate measured gate, not implied by correctness of timing formulas.

Complete the canonical replay evidence and its documented coverage before a
small canonical-preserving draw-authority extraction. That extraction is Phase 2;
it is not included merely because Phase 1 records draw mutations. Do not begin
bulk actor conversion or present 60/120 Player modes as working before the applicable gates.

## Pass 3A: bounded High-pass authority extraction specification

This historical appendix defined Pass 3B's implementation scope, now accepted
above. Pass 3A itself added observation and fixtures only. Source line anchors
below refer to `b0b79271eb4fe9649d020ea86f68ba37a8513b3b`, before the additive
observers; resolve by function name after that insertion. [PASS3A.md](PASS3A.md)
owns measured acceptance, startup reliability, review findings and the handoff.
[TESTING.md](TESTING.md) owns executable commands and fixture evidence.

The combined canonical order is input -> collision/actors -> `Message_Update` ->
`Interface_Update` -> world/actor CPU draw -> HUD preamble -> **late countdown
authority** -> timer presentation/HUD remainder -> **late message authority** ->
message presentation -> common frame hooks -> audio control/mixing -> graphics
command replay -> snapshot. The two new operations are separate ownership seams
inside the existing late overlay pipeline. They do not make all CPU draw pure.

### Findings that constrain the design

1. The main timer is authoritative. Its state and seconds are consumed by actors, Player item processing, messages, and audio. In particular, `STOP` is an observable boundary between actor updates; replacing it with immediate `OFF` loses behavior.
2. `timerDigits[4]` is mixed state, not disposable formatting. The warning decision consumes the previous displayed ones digit before the current call regenerates digits. A pure renderer that generates digits first would change audio.
3. The `MOVE` state mixes authority and presentation. Integer X/Y motion and the one-second divider advance in the same call through deliberate switch fallthrough. Its final call can enter `TICK` and immediately decrement seconds or enter `STOP`.
4. The countdown has its own gated cadence late in HUD drawing. Frame advance, `IREG` freeze and freeze-flash can suppress world work while this HUD code remains reachable. Pause, message, cutscene, transition, game-over, minigame, shooting-gallery and one scene switch are separate gates.
5. The exact current authority slot is inside `Interface_Draw`, after the HUD preamble and subtimer respawn handling, before timer geometry and `Message_Draw`. Moving it to `Play_Update`, or even indiscriminately to immediately before `Interface_Draw`, is not established safe.

### State inventory and units

| State | Classification / ownership now | Units and relevant behavior |
|---|---|---|
| `gSaveContext.timerState`, `timerSeconds` | Authoritative shared main timer; setters, actors, update and draw all participate | State enum (`z64save.h:177`), integer seconds. `STOP = 10`; `DOWN_INIT/PREVIEW/MOVE/TICK = 5/6/7/8`. Never multiply enum values by rate. |
| `subTimerState`, `subTimerSeconds` | Authoritative shared subtimer | Separate enum at `z64save.h:196`; suspended behind any handled main state; includes trade expiration, actor halt, message, transition, inventory writes. Outside first admission. |
| `sTimerStateTimer`, `sTimerNextSecondTimer` | Hidden authoritative phase counters, formerly function-static in `Interface_Draw:5144–5145` | Integer counts of eligible HUD calls. `PREVIEW` and `MOVE` each use 20; the second divider uses 20. Static process lifetime, not a scene/save field. |
| `sSubTimerStateTimer`, `sSubTimerNextSecondTimer` | Hidden authoritative subtimer phase counters at 5146–5147 | Same eligible-call units, plus 40 for `STOP` and respawn paths. Outside first admission, but observed to detect interference. |
| `timerDigits[5]` | Persistent display and warning-control state at 5148 | Digits are minute tens, minute ones, colon (`10`), second tens and second ones. Old element 4 drives sounds at 6068/6074; new values are prepared at 6295–6315. `SetTimer` does not reset them. |
| `gSaveContext.timerX/Y[2]` | Presentation coordinates coupled to authoritative state-machine phase | Integer logical HUD coordinates. The main timer starts at `(140, 80)`. `MOVE` divides remaining distance by the current state counter, truncating through C integer division, then snaps X to 26 and Y to 46 or 54. Do not replace this with interpolation during compatibility extraction. |
| `sEnvHazard`, `sEnvHazardActive` | Authoritative hazard selection/latch at 185–186 | Hazard enum and boolean-like `s16`. Update chooses the hazard; an active latch turns expiry at zero into `health = 0` and a damage callback. Both are excluded from ordinary-countdown admission. |
| `timerId`, local digit visibility decision | Per-call render packet retaining legacy branch history | The main timer is initially selected even when `STOP` turns it `OFF` and a subtimer becomes drawable. Recomputing `timerId` solely from the final state changes this existing behavior. |
| Icon textures, colors, cosmetic margins, rectangles | Presentation | Preserve placement, color and visibility while moving only admitted timer authority. The `HIDDEN` timer position still performs timer work. |
| `sCUpTimer`, `sCUpInvisible`, interface counter digits, do-action fields | Preamble presentation/mixed evidence; not claimed converted | The C-Up timer still changes in `Interface_DrawItemButtons:4268`; HUD formatting overwrites interface `counterDigits`; do-action state/rotation advances in `Interface_Update`. `Interface_Draw` remains stateful after countdown-only extraction. |

`SaveContext` timer field layout: `soh/include/z64save.h:353–358`. Interface fields: `soh/include/z64.h:760–803`.

### Exact canonical schedule and boundaries

At baseline, `Interface_SetTimer(seconds)` (`z_parameter.c:3803–3813`) sets the main timer's X/Y to `(140, 80)`, clears `sEnvHazardActive`, writes the seconds, and selects `DOWN_INIT` for any nonzero value or `UP_INIT` for zero. It does not immediately reset either hidden main counter or the digits. On the first eligible draw, `DOWN_INIT` resets both main counters to 20 and chooses `PREVIEW`. If a message blocks that draw, the `INIT` state and stale hidden values persist until eligibility resumes.

Number calls from the first eligible `INIT` call as `n = 0`:

| Eligible call | Transition / result |
|---|---|
| 0 | `INIT → PREVIEW`; both counters become 20; digits are prepared from the initial seconds. |
| 1…19 | Preview counter runs from 19 to 1; the second divider remains 20. |
| 20 | Preview counter reaches 0, resets to 20 and selects `MOVE`. This call has no movement or second-divider decrement. |
| 21…39 | `MOVE` adjusts X/Y using divisors 20 through 2 and decrements the state counter. Fallthrough decrements the second divider from 19 through 1. |
| 40 | `MOVE` uses divisor 1, snaps X/Y to `(26, 46)` or `(26, 54)`, resets the state counter to 20 and enters `TICK`. Fallthrough reaches divider 0; seconds decrease and the divider resets to 20. |
| 60, 80, 100, … | Subsequent eligible second boundaries. |

The decrement is guarded by `timerSeconds != 0`; the zero test follows the divider's reload to 20. At zero, the state becomes `STOP`, a hazard may kill the player (outside admission), `sEnvHazardActive` is cleared, and no warning sound is requested. With no subtimer, the visibility test suppresses timer geometry and **does not refresh digits**. The next eligible call handles `STOP → OFF`; it does not return through the main switch's default branch or advance a subtimer in that same call.

For the existing one-second recipe, the required phase trace at tick 40 is `STOP`, and snapshot 41 after that transaction retains `STOP`. The next actor update observes it; phase tick 41 changes to `OFF`, and snapshot 42 records `OFF`. PASS3A.md owns runtime acceptance of these source-derived requirements. For one second, `TICK` (8) is never visible in a snapshot: `MOVE` (7) enters 8, then `STOP` (10), in the same call. The five required states are 5, 6, 7, 10 and 0.

Warnings at `z_parameter.c:6067–6083` use **post-decrement seconds but pre-regeneration ones digit**:

* New seconds greater than 60: `NA_SE_SY_MESSAGE_WOMAN` only if the old ones digit is 1 (for example, 71 → 70).
* Otherwise, new seconds at least 11: `NA_SE_SY_WARNING_COUNT_N` only if the old ones digit is odd (for example, 13 → 12; 12 → 11 does not beep).
* New seconds from 1 through 10: `NA_SE_SY_WARNING_COUNT_E` on every decrement, including 11 → 10.
* Zero: no warning. Do not change the zero branch into an urgent beep.

The twelve-second fixture's predicted values at ticks 40/60/80/100 are 11/10/9/8, with sounds only at 60/80/100. The seventy-two-second fixture's predicted values are 71/70/69/68, with a sound only at tick 60 (`NA_SE_SY_MESSAGE_WOMAN`). The existing ten-second fixture beeps on each 9/8/7/6 boundary. Audio requests must retain their place before `Message_Draw` requests and the later `Audio_Update`.

### Gates and complete legacy phase order

Outer routing before the timer code:

* Play_Main invokes Play_Update, then starts interpolation recording and Play_Draw (`z_play.c:1691–1703`). Its draw is not suppressed merely because FrameAdvance_Update returned false at line 700.
* Play_Draw has its general debug gate HREG80/82 at line 1360; special transition/pre-render paths can jump to overlays at 1435/1461 or skip overlays at 1610. The overlay debug gate HREG80/89 is at 1619.
* Play_DrawOverlayElements first performs pause-menu draw if needed at 1293–1295, then Interface_Draw only in GAMEMODE_NORMAL at 1297–1300, then Message_Draw at 1303–1305, then game-over light drawing at 1307–1308.
* Interface_Draw returns early for GameInteractor_NoUIActive at 5168; the outer `debugState == 0` check gates its main body at 5179. `MinimalUI`/hidden timer position does not itself gate the timer.

The inner timer gate (`z_parameter.c:5968–5973`) requires all of: `pause.state == 0`; `pause.debugState == 0`; inactive game-over; `msgMode == NONE`; no `PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR`; transition trigger and mode both OFF; `!Play_InCsMode`; `minigameState != 1`; `shootingGalleryStatus <= 1`; and no Bombchu Bowling scene with switch 0x38 set.

`Play_InCsMode` (`z_play.c:1725`) is non-IDLE csCtx OR Player_InCsMode. Player_InCsMode (`z_player_lib.c:512`) includes Player_InBlockingCsMode plus `unk_6AD == 4`. Blocking predicates at 505–509 include player dead/in-cutscene flags, csAction, transition start, loading flag, hookshot flight, and active magic-spell item/action. Observe raw dependencies or the exact pure boolean; do not substitute csCtx.state alone.

The main countdown MOVE/TICK inner branch additionally requires `msgLength == 0` at 6051. The outer msgMode NONE and inner msgLength zero checks are distinct. INIT/PREVIEW do not check length. Countdown and digit preparation are both inside the inner timer gate, so disabling it also freezes digits/layout and omits timer geometry. The pre-gate subtimer respawn branch at 5927–5966 is gated differently and must not be swept into the new countdown gate.

Ordinary transaction order, with current source anchors:

1. RunFrame Graph_StartFrame; replay BeginFrame; PadMgr sampling (`graph.c:483–486`).
2. GameState_ReqPadData consumes edges; GameState_Update enters Play_Main (`graph.c:297–298`, `game.c:252–271`).
3. FrameAdvance gate, transition logic, pause entry, animation reset/object bank; gameplay branch increments gameplayFrames; freeze-flash may skip actor work (`z_play.c:700,1112–1155`).
4. Collision AT/OC/damage/clear then Actor_UpdateAll and world effects (`z_play.c:1157–1195`). Actors read the previous completed HUD transaction.
5. Pause/gameOver update or Message_Update; Interface_Update, animation queue, sound-source update, shrink/fade (`z_play.c:1229–1255`). Interface_Update invokes OnInterfaceUpdate hook at `z_parameter.c:6526`, detects hazards6679–6690, starts/cancels environmental timer6818–6831.
6. Camera/environment updates (`z_play.c:1266–1289`), then CPU world drawing, Actor_DrawAll1533, later draw-end hooks1616.
7. Pause draw if any; Interface_Draw preamble5180–5925; subtimer respawn5927–5966; **the proposed late countdown authority slot**5968–6316; timer geometry6318–6384; existing HUD epilogue.
8. Message_Draw at `z_play.c:1304`; remaining Play_Draw work/camera finish and total-gameplay-timer overlay.
9. OnGameFrameUpdate and state.frames++ (`game.c:357–358`); Audio_Update (`graph.c:393–395`).
10. Graph_ProcessGfxCommands (`graph.c:495`) performs pinned replay audio synthesis before presentation requests; savestate request processing then replay EndFrame at500–501. Existing 60/120 presentation loops replay the display list and do not repeat this CPU HUD slot.

Consequences: a message that closes in Message_Update can permit HUD INIT/countdown in the same transaction. A mode change in Message_Draw happens after HUD and cannot retroactively suppress/advance this timer. Actor timer failure actions happen on the next actor update. Do not move HUD authority before collision/actors or after Message_Draw/audio.

### Reset/lifetime and excluded sibling behavior

* Process start zeroes the four C statics and digit array. SaveContext_Init (`z_common_data.c:6–7`) clears SaveContext but does not reset these statics. Fresh-process replay is the current reset proof.
* SetTimer resets XY/seconds/state/hazardActive but defers counter initialization and does not reset digits. A second SetTimer while message-gated must keep this exact partial-reset behavior. No mid-run reset recipe is implemented in Pass 3A.
* The interface constructor is `func_801109B0` (`z_construct.c:10`, 86–119): some active main/sub timers reposition to X 26, Y 46 or 54; hazard tick may become INIT for respawn -1/1; all main up states 11…15 become OFF. It does not reset the hidden counters/digits. Do not introduce a generic per-scene timer reset.
* File selection clears main/sub states (`ovl_file_choose/z_file_choose.c:2564–2565`); GameOver death-start clears both states and eventInf[1] bit 0 (`z_game_over.c:31–36`); neither clears hidden statics here.
* Existing savestate overlay-static list (`savestates.cpp:246…`) has no parameter/HUD-static serializer. Do not claim these observations make snapshots restorable.
* Main UP uses separate MOVE/TICK behavior at 6087–6135, reaches 3599 then FREEZE (15), and remains there: its assigned 40 counter is not decremented by the main FREEZE case. It has no msgLength gate inside its tick branch. Do not generalize countdown behavior to it.
* Main STOP with an active subtimer resets both sub counters to 20 and sub XY to 140/80, chooses down/up PREVIEW based on `substate <= STOP`, then clears main (`6136–6152`). This resumes/reinitializes suspended subtimer semantics and uses a main-selected local timerId for that call. Exclude it from initial admission.
* Sub expiration (`6232–6245`) may start 0x71B0, halt actors and set RESPAWN; pre-gate respawn logic at 5927–5966 initiates transition, restores equipment, spoils trade items and loads icons. Sub up reaching 240 can start 0x6083/change an event flag; minute boundaries request sound; sub STOP (6) clears after 40 calls. The entire dependency closure is outside the first pass.
* Environmental timer shares down MOVE/TICK and can kill at zero (6062–6065); Interface_Update can start or cancel it, and SetSubTimer can clear hazardActive. Exclude all hazard states 1…4 and a true hazard latch from the pilot.

### External readers/writers and hidden callers

This list comes from repo-wide references to timerState/Seconds, subTimerState/Seconds, timerX/Y, and the three public timer APIs; it distinguishes the ordinary countdown closure from siblings. Paths beginning actors/ are under `soh/src/overlays/actors/`.

| Family / exact baseline anchors | Consequence |
|---|---|
| `actors/ovl_Obj_Roomtimer/z_obj_roomtimer.c:53–80` | Setter at60; destructor/successSTOP at54/70; secondszero produces abyssSFX, voidout, actor kill77–81. Next-actor-update latency matters. |
| `actors/ovl_Bg_Po_Event/z_bg_po_event.c:234–235,324,350–353` | Timer for falling-block puzzle; start, cleanupSTOP, successSTOP, zero puzzle state reset. Actor-local/static puzzle closure excluded. |
| `actors/ovl_En_Diving_Game/z_en_diving_game.c:109,131–151,426–428,519–520` | CleanupOFF, STOP loss startsmessage/cutscene, successOFF, start50+BREG2, exactseconds10 fast-tempo request. |
| `actors/ovl_En_In/z_en_in.c:443,586,663–668,938` | Start60; STOP triggers audio/state transition/OFF; sub<6 changes talk behavior. |
| `actors/ovl_En_Ta/z_en_ta.c:241,704,738–751,811` | Start30; seconds10 music tempo; zero ends minigame/startsmessage/cutscene; cleanupOFF. |
| `actors/ovl_player_actor/z_player.c:2614–2621` | STOP directly suppresses Player_ProcessItemButtons. This is in even a neutral-room countdown closure; observe STOP through the next update. |
| `z_message_PAL.c:1968–1973,2375–2384` | RACE_TIME/MARATHON_TIME text decoding reads main/sub seconds. Simple admitted0x1043 has no such token; other messages excluded. |
| `actors/ovl_En_Horse_Game_Check/z_en_horse_game_check.c:133,145,216,303–304,333,388,405–409`; `ovl_En_Ma3/z_en_ma3.c:84–97,125,152–159` | Main count-up start0,180-second failure, forced240/freeze, score recording and STOP. Outside down-count admission. |
| `actors/ovl_En_Po_Relay/z_en_po_relay.c:146,272,293,340–351`; `ovl_Bg_Relay_Objects/z_bg_relay_objects.c:168` | Dampe count-up, stop/freeze, reward/highscore comparisons. Outside admission. |
| `actors/ovl_En_Syateki_Niw/z_en_syateki_niw.c:496` | MainOFF writer; not a harmless presentation read. |
| `actors/ovl_En_Hs/z_en_hs.c:132`, `ovl_En_Kz/z_en_kz.c:493`, `ovl_En_Mk/z_en_mk.c:98,265`, `ovl_En_Mm2/z_en_mm2.c:109,116,197,238,269–277` | Trade/marathon setters180/240/0, OFF writes, highscore comparisons. Subtimer closure excluded. |
| `actors/ovl_En_Ds/z_en_ds.c:94`, `ovl_En_Go/z_en_go.c:873`, `ovl_En_Go2/z_en_go2.c:1070`, `ovl_En_Zl2/z_en_zl2.c:1613`; `ovl_En_Zl3/z_en_zl3.c:2504,2520,2547,2593,2681`; `ovl_En_Eg/z_en_eg.c:53` | SubOFF setters, tower escape starts180/zero completion, explosion/game-over interaction. Outside admission. |
| `Interface_SetSubTimerToFinalSecond`: `z_parameter.c:3793–3799`; callers `ovl_Demo_Kankyo/z_demo_kankyo.c:829`, `ovl_player_actor/z_player.c:15136`, `soh/Enhancements/QoL/PauseWarp.cpp:63` | Mutates seconds to1 or239 based on eventInf bit, leaving divider untouched. Future reset tests must not rearm20 implicitly. |
| `soh/Enhancements/Minigames/DivingGameTimer.cpp:18`, `IngoRaceOnce.cpp:26`, `DampeBothPrizes.cpp:20`; `TimeSavers/SkipCutscene/Story/SkipBlueWarp.cpp:163–164` | Hook-controlled setter/OFF/score/hazard writes; defaults pinned, no blanket enhancement acceptance. |
| `soh/Enhancements/tts/tts.cpp:133–136`; `TimeDisplay/TimeDisplay.cpp:85,107–124` | Timer reads for accessible/presentation output; keep read-after-write order. |
| `soh/Enhancements/debugger/debugSaveEditor.cpp:799–817` | Direct mutable UI references to timer states/seconds; excluded from replay. |
| `soh/SaveManager.cpp:2678–2683` | Old-layout compatibility struct declarations, not evidence that current JSON save captures hidden timer state. |

Only constructor plus parameter code reference timerXY in runtime C source. Public timer APIs are declared `functions.h:1096–1098`. Static counters/digits/hazard variables have no external direct access until the observation-only API below.

### Smallest safe future admission and explicit phase contract

Recommend initial first-extraction scope: GAMEMODE_NORMAL, main states DOWN_INIT/PREVIEW/MOVE/TICK/STOP (and quiescent OFF for output), positive SetTimer argument 1…3599, subTimer OFF, hazardActive false, environmental hazard NONE, known ordinary room/default config, and no native-rate change. The single ordinary two-page message is allowed as a HUD gate interaction; its own authority extraction has a separate message design. No race/trade/environment/minigame actor capability is claimed. Pause/transition, world-freeze, NoUI and alternate enhancement/language profiles remain outside initial admission until their direct gates are established; the complete legacy path remains responsible for them.

The future late authority operation must be reached once at the **old timer slot** after HUD preamble/legacy sub-respawn branch and before timer rendering/Message_Draw. It may mutate only admitted main timer state/seconds, hidden main counters, main XY, the legacy digit cache, and the already-existing `hazardActive = false` write at zero; it may issue the same warning sound request exactly once. It may read all eligibility inputs and healthCapacity for layout. It must not mutate Player/actors, collision, animation, RNG, PCM/task scheduling, message state, sub timers, inventory, scene/transition, or health in this admission. Side effects outside that allowlist fail the pilot gate.

For 20-Hz compatibility, keep integer call counters and all C expression/switch ordering initially. Label elapsed time as quanta of 1/120 second, but do not replace 20 with float dt; each eligible legacy call has six quanta, and suspension preserves all divider/state fractions. A later fixed-rate scheduler can admit a derived compatibility event every six quanta only after handling the separate gate clock. This extraction is not permission to enable higher-rate Player simulation.

Separate a render packet (visible, chosen timerId, prepared digits, XY, color decision) from timer transitions. Render must use the packet produced for this same legacy transaction and may emit geometry only; it must not reload/decrement counters, regenerate persistent digits, decide sounds, clear STOP, or alter the next authority decision. Preserve stale digit behavior at zero and all false-gate cases. The packet must latch branch-local facts instead of recomputing them from the final state.

Proposed named interfaces for the High pass (design only):

* `Interface_IsCountdownProfileAdmitted(play)` is a pure scope decision; never keyed to NativeSimTest_IsEnabled. Unsupported profile stays on the original branch without partially running either path.
* `Interface_AdvanceCountdownLegacy(play, &packet)` owns the exact admitted mutation/sound allowlist above at phase `hud.countdown.late_authority`. Its output is `InterfaceTimerPresentation` for this same transaction, including the original visibility/selection decisions.
* `Interface_DrawCountdownPresentation(play, const &packet)` (C uses a const pointer) emits only timer geometry, phase `hud.countdown.presentation`; it cannot mutate the authority allowlist or call the old transition branch. Packet validity lasts only through this synchronous CPU build.

Implementation sequencing: first these separate authority/presentation functions at the exact old call point; then only if necessary introduce an explicit late-UI transaction coordinator preserving preamble→timer authority→timer render→HUD epilogue→message order. Extracting a function while its legacy driver remains in Interface_Draw is a scoped ownership step, not a claim that CPU draw as a whole is pure or skippable. Splitting the larger Interface_Draw must preserve local render scratch, matrix state, hook order and every unsupported sibling path. Do not move the whole timer block (which includes hazard/sub/trade behavior) merely for a cleaner API.


### Recommended bounded first extraction

Select **English 0x1043**, two pages, black textbox, variable position, null talkActor
through the existing recipe. Actor talk state/callbacks are not covered. Retain
0x305F quicktext/fade regression coverage on its original implementation.

Local TextFactory-format inspection of pinned `oot.o2r`: 0x1043 is the shortest
two-page standard black/blue candidate with ASCII/NEWLINE/BOX_BREAK/END only:
64 bytes, 27/34 printable characters, BOX_BREAK27/NEWLINE42/END63; byte SHA-256:
`78f1089ee57149110c55bbf0ce4aba5f4a87774cbaf077e45ea4703a892002ab`.
0x305F uses QUICKTEXT_ENABLE/COLOR/QUICKTEXT_DISABLE/FADE(60)/END, so cannot prove
ordinary crawl or page input. No proprietary text payload is copied into source.

Admission must be explicit. The proposed High pass should initially admit the
plain English profile exercised by 0x1043: no credits, no language replacement,
no custom-message substitution, standard black textbox, no choice/ocarina,
no TextSpeed/SlowTextSpeed/SkipText modifications, and controls restricted to
NEWLINE/BOX_BREAK/END. Keep other profiles on the existing code. Validate the
whole message/page profile before moving any of its work; an unexpected control
cannot partly execute in a new walker and then fall back to the old walker.
Fixture admission should fail clearly on a changed resource/profile. Production
fallback retains legacy behavior; no higher-rate support is inferred. The
implementation must not key authority to NativeSimTest_IsEnabled: the test flag
only controls observation, not which gameplay semantics are extracted.

### Actual transaction order and gating

1. RunFrame/PadMgr latches the normalized held/press/release state. Message reads
   `play->state.input[0]` directly. Player makes a separate local copy and may
   mask A/B/C-Up for `textboxBtnCooldownTimer` (`z_player.c:12229`); that local
   mask does not consume or clear the message input edge.
2. Play_Update performs collision consumption/reset, Actor_UpdateAll, cutscene
   and effects. Actor code and Player can read `Message_GetState` from the
   preceding completed transaction before this frame's Message_Update.
3. `Message_Update` (`z_play.c:1237`) runs only if the surrounding update path
   reaches it and neither pause/debug pause nor game-over chooses its alternate
   branch. `Interface_Update` follows at1243, then queued animation, sound-source,
   fade, camera and environment work.
4. FrameInterpolation_StartRecord brackets Play_Draw, which retains world,
   actor pose/collision/RNG, effects and OnPlayDrawEnd work. Those are not moved.
5. Play_DrawOverlayElements at1292 calls pause drawing if required, Interface_Draw
   for normal game mode, then Message_Draw at1304, then game-over light fading.
   HUD countdown work therefore observes state after Message_Update but before
   Message_Draw. Its `msgMode==NONE` gate can resume in the same transaction that
   update closes the message, but cannot see a later draw-owned transition early.
6. Message_DrawMain and text/icon drawing currently mutate message state during
   CPU command construction. Common game-state/frame hooks then run; Audio_Update
   control and the existing three test mixer blocks follow. Preserve all logical
   audio ingress ordering relative to actors, HUD and other messages.

First new authority hook: **message.late_authority** at the current Message_Draw
site, after the entire HUD call and before message presentation construction.
This is a late overlay authority phase, not an extra invocation of Message_Update.
Keep it inside the exact current Play_Draw reachability gates (HREG80/82/89,
transition/pause-buffer branches) and interpolation record bracket. Do not move
it to Play_Update, before Actor_DrawAll, before Interface_Draw, or after audio.
HUD needs its own still-earlier exact internal split, described by the HUD agent;
the provisional combined order is HUD preamble -> HUD timer authority -> HUD timer
presentation/remainder -> message late authority -> message presentation.

Message_Draw lacks an ordinary pause gate and calls DrawMain even when NoUI hides
the display-list link; Interface_Draw returns for NoUI. MSGMODE_PAUSED idles both
switches, except DrawMain's earlier language check. FrameAdvance_Update can skip
update while CPU draw runs: no new world-paused guard. Start cannot pause an active
message (Play_Update:1112 requires msgMode NONE); actual pause sets R_UPDATE_RATE2
and needs separate cadence admission. Retain the current replay rejection.

### Exact selected state machine

`Message_StartTextbox`2854 -> Message_OpenText2681 initializes raw message data,
type/position, lengths, HUD saved visibility, statics and drawing counters; start
sets msgMode START, stateTimer=0,textDelayTimer=0,talkActor and ocarina sentinels.
Message_Init in z_construct:142 establishes scene state, font resources and YREG31.
Keep both in their existing locations. File statics have process lifetime;
Message_OpenText resets some but not all. Never introduce a blanket reset.

| Legacy owner | Reads and exact writes relevant to 0x1043 | Ordering consequence |
|---|---|---|
| Update START4444 | increments sMessageStartFrameCount; null talkActor admits immediately; selects textbox target registers; GrowTextbox computes geometry/alpha and increments stateTimer, then START resets stateTimer=0 and selects GROWING | Do not omit the first grow calculation or combine it with the next eight calls. |
| Update GROWING / GrowTextbox257 | coefficient table is indexed by old u8 stateTimer; alpha rises, timer increments; equality8 selects STARTING; current/target geometry registers are updated | Timer is an index and state identity as well as duration; no dt conversion. |
| Update STARTING4540 | msgMode=NEXT_MSG; Interface_SetDoAction(NEXT) when YREG31==0 | Interface_Update later in this same frame may advance action-label rotation. |
| Update NEXT_MSG4546 / Decode2239 | resets textDelay/textDelayTimer/textUnskippable and sTextFade; increments sTextBoxNum; fills decoded buffer/glyph resources; stops at BOX_BREAK or END; msgMode=DISPLAYING,textDrawPos=1,decodedTextLen=terminator index, layout start Y set | Terminator is not included by textDrawPos==decodedTextLen. |
| Update DISPLAYING4562 | B press (default SkipText0), standard box,YREG31==0,!textUnskippable sets sTextboxSkipped=true and textDrawPos=decodedTextLen | Draw still needs its normal post-loop step before the terminator is visited. Skip flag persists to the next page. |
| DrawText1280 normal glyph/default1584 | if mode DISPLAYING and i+1==textDrawPos and textDelayTimer==textDelay, emits Audio_PlaySfxGeneral(0); glyph layout reads current cursor/color | Preserve even zero-valued audio ingress and its position in event order. No message RNG call exists. |
| DrawText BOX_BREAK1330 | while DISPLAYING, unskipped path emits SFX0, selects AWAIT_NEXT, loads triangle icon; skipped path selects NEXT_MSG, clears textUnskippable, increments msgBufPos; returns immediately | No post-loop crawl after the terminator branch. Unskipped first arrival does not yet draw the icon. |
| DrawText END1514 | while DISPLAYING, selects DONE; default end type emits MESSAGE_END, loads square icon, calls Interface_SetDoAction(RETURN) if csCtx idle; returns | This call is after HUD command construction, so moving it before HUD changes same-frame action-label observation. |
| DrawText post-loop1597 | calls VB_TEXT_CRAWL_FASTER; default false: if delayTimer0, textDrawPos=i+1 and delayTimer=textDelay, else --delayTimer | Runs only when no earlier control returns. Preserve evaluation count and the old exclusive draw bound. |
| Update AWAIT_NEXT4581 | Message_ShouldAdvance checks A/B/C-Up press and emits MESSAGE_PASS; selects NEXT_MSG, clears textUnskippable, increments msgBufPos | Decode occurs next update, not by falling through this switch. Held A does not re-emit a press. |
| DrawMain WAIT_NEXT3362 and DONE4164 -> DrawTextboxIcon580 | renders text using entry mode, then icon; icon increments shared msgCtx.stateTimer (u8) after its graphics work | Shared timer increment is authority. First DISPLAYING->WAIT/DONE transition did not enter either icon branch; using post-authority mode would show/increment icon a transaction early. |
| Update DONE4588 | non-fade/default end and YREG31==0: ShouldAdvanceSilent then DECIDE sound and CloseTextbox | No MESSAGE_PASS for ordinary final close. Fade path instead decrements stateTimer before testing zero. |
| CloseTextbox187 | if msgLength!=0, stateTimer=2,msgMode=CLOSING,endType=DEFAULT, emits SFX0 | First closing update is next transaction. |
| Update CLOSING4623 | --stateTimer; exits while nonzero. At zero restores HUD visibility subject to cutscene/camera conditions; clears msgLength/mode/textId/stateTimer, action overrides, end type and last-played-song; retains original inventory/ocarina branches | Message_GetState reports CLOSING only for mode CLOSING with timer1, exposing exactly one canonical interval to earlier actor updates. |

`Message_GetState`3042 is an alias API, not a direct numeric view: zero msgLength
means NONE; DONE depends on textboxEndType; AWAIT_NEXT means AWAITING_NEXT;
CLOSING means closing only at timer1. Other active states commonly return
DONE_FADING even when no actual fade exists. Serialize the derived value as well
as raw mode/timer; do not reimplement the mapping differently.

For the admitted ASCII profile, glyph layout also writes textPosX/Y, default
textColorRGB, and unk_E3D0=0. Spaces add TextSpacing (default6); NEWLINE resets X
and adds line spacing with choiceNum branches. These are presentation scratch
with legacy visible outputs, not time integrators. Message_DrawTextBox reads
geometry/alpha and configures view/display lists but does not advance the chosen
message state. Icon flash colors, 12-call flash timer and character-size globals
are presentation state; **its stateTimer++ is mixed authoritative state**. The
first extraction must advance cosmetic flash evolution once during packet
preparation and latch its resulting colors, size and scale, along with the shared
message-timer write. Repeated packet presentation must not run the flash timers
or modify the character-size globals again. This is presentation preparation,
not a new gameplay timer or rate conversion.

### Presentation packet is mandatory, not a post-state redraw

The new phase must execute one logical traversal in the old order and produce a
bounded immutable per-transaction message presentation packet. Packet decisions
must use the original entry msgMode/textDrawPos and the actual control traversal,
including early returns. Store the ordered drawable glyph/layout/color entries,
textbox/view parameters and icon-presence/type/position decision. A glyph list is
bounded by the existing 200-byte decoded buffer; no renderer pointer enters replay
hashes. Keep transient buffer/resource references valid only through this same
synchronous CPU build; do not queue them across future transactions.

The authority traversal owns mode/cursor/delay/shared timer, font-icon selection,
DoAction change and ordered logical audio ingress. Presentation consumes the
already-selected glyph/icon plan without calling GameInteractor predicates,
Message_ShouldAdvance, Message_CloseTextbox, audio ingress or the old mutating
Message_DrawText again. Hook results must be evaluated once at their legacy
authority position. For this first profile default-disabled text hooks can be
explicitly excluded; do not silently rerun them to derive layout. Pure visual
cursor/color outputs may be maintained in a dedicated presentation view; preserve
their currently observed final values and display order without writing authority.

For the admitted plain-text traversal, the mutation allowlist is concrete:
`msgMode`, `msgBufPos`, `textUnskippable`, `textDrawPos`, `textDelayTimer`, and the
icon branch's shared `stateTimer`; the existing `font.iconBuf` selection; and the
existing END-triggered `Interface_SetDoAction` effects. The latter includes
`unk_1F0`, `unk_1EC`, `unk_1F4`, `doActionSegment[1]` and global `gSegments[7]`
through `Interface_LoadActionLabel`, with its existing hook preceding those
writes. Preserve conditional execution and order; do not turn unchanged target
labels into additional hook/resource operations. `textDelay` remains an input in
this profile, which excludes its control token. Opening, decoding, input handling
and closing writes remain in their existing update functions.

Once-only presentation preparation also owns the legacy final `textPosX/Y`,
`textColorRGB`, `unk_E3D0`, `sCharTexSize/Scale` and icon flash statics, without
changing their storage lifetime or reset policy. These may not be advanced or
rewritten by repeated presentation. `Message_SetView` is another impure helper:
`View_SetViewport` and `func_800AB2C4` (`z_view.c:137`, `:520`) write viewport/flags,
cached viewport/projection values and projection pointers while allocating and
linking render data. Retain that setup once before the text traversal at its
legacy position, and carry the resulting view data/commands in the same-frame
packet. Do not call it repeatedly on the live message view. A repeated renderer
may write its own command buffer and frame-local allocation output; it must not
write live message/HUD/segment/view state or cosmetic clocks. The High purity
gate must cover these fields and appearance outputs even where the current
Pass3A snapshot does not serialize them; raw pointer equality is not a replay
oracle. All other gameplay/resource side effects are outside this allowlist.

Proposed High interfaces: pure `Message_IsPlainTextProfileAdmitted(play)` chooses
the whole legacy fallback or `Message_AdvancePlainTextLegacy(play, &packet)` at
`message.late_authority`; `Message_DrawPlainTextPresentation(play, const &packet)`
(C uses a const pointer) consumes `MessagePlainTextPresentation` at
`message.presentation`. Neither admission nor authority depends on the test flag.

Concrete traps rejected by this design: advancing textDrawPos then drawing to its
new value displays one extra character immediately; inspecting new DONE mode draws
an end icon one transaction early; moving SetDoAction before Interface_Draw changes
the already-built HUD; counting iconTimer in both new phase and DrawTextboxIcon
duplicates authority; moving mode completion before Actor_UpdateAll removes a
legacy frame of actor/message latency. Whole-MessageContext save/restore around the
old draw is not a solution: statics, audio, hooks, font resources and HUD writes
would escape the copy. A dry-run pass that calls then repeats hooks is also wrong.

### Dependencies, aliases and deliberate exclusions

- Message_Draw's sole caller is Play_DrawOverlayElements; DrawMain's sole caller
  is Message_Draw. DrawText/JPN dispatch also serves ocarina, outside admission.
  Start/Continue/Close actor/script/HUD callers remain in place.
- Kokiri helpers select0x1043; no special closing case exists. Null-talker coverage
  excludes those actors. No Rand/Random/osGet call exists in z_message_PAL.c;
  later sound processing can advance audio RNG, so ingress/order must match.
- TextSpeed.cpp hooks VB_ENABLE_QUICKTEXT, VB_FIX_TEXT_SPEED_SOFTLOCK and
  VB_TEXT_CRAWL_FASTER mutate the same context via global gPlayState. Default1 is
  inactive, but these are aliases to account for. SkipText changes B from press to
  held; TextSpacing affects layout. Freeze manifest values explicitly for fixtures.
- OnOpenText may replace ID/buffer; OnDialogMessage precedes update dispatch;
  OnSetDoAction precedes action-field/resource writes. TTS reads message state.
  Preserve order; arbitrary hooks/randomizer dialogue need separate admission.
- Language switching inside DrawMain3292 calls OpenText/Decode repeatedly and
  restores selected state. Exclude it, Japanese/credits, continuation/TEXTID,
  choices, TEXT_SPEED, AWAIT_INPUT, delayed breaks, persistent/event text, item
  icons, dynamic time/name substitutions, custom resources, ocarina and cutscene
  conversations from first profile. Existing legacy behavior stays intact.
- Actors/Player/HUD read Message_GetState; HUD also reads mode/length separately.
  SetDoAction sets target,state1(3 if paused),rotation0,label1; Interface_Update
  later rolls rotation/current label. Preserve same-frame visibility.
- Heart healing, quest rollover and stored Saria-song close branches stay update
  owned; the fixture does not exercise them and extraction must not simplify them.


### High-pass boundaries and acceptance

Implement `Interface_IsCountdownProfileAdmitted`,
`Interface_AdvanceCountdownLegacy` and `Interface_DrawCountdownPresentation` in
`z_parameter.c`, and `Message_IsPlainTextProfileAdmitted`,
`Message_AdvancePlainTextLegacy` and `Message_DrawPlainTextPresentation` in
`z_message_PAL.c`, with C-compatible packet declarations. Keep `z_play.c` overlay
reachability/order and all update entry points intact. Begin with functions at
the exact legacy slots; moving the whole HUD preamble is not authorized.

The message predicate must admit the complete validated profile before traversal,
including language/default enhancements and controls. Keep unsupported text on
the legacy path from the start. The countdown predicate likewise excludes hazard,
subtimer and count-up families before any mutation. Neither predicate may depend
on the replay flag. Authority runs once per reached canonical transaction, not
once per GPU submission. Presentation may be repeated only from its immutable
packet; it must not traverse a mutating legacy text/timer function.

Preserve the outer diagnostic scopes `draw.interface.begin/end` and
`draw.message.begin/end` for full legacy trace comparison. Name the internal
ownership phases `hud.countdown.late_authority`, `hud.countdown.presentation`,
`message.late_authority` and `message.presentation` in the APIs and optional
separate diagnostic records. Do not change the existing event/RNG phase labels
merely to rename ownership. This permits complete Pass 2 and Pass 3A traces to
remain exact. The bounded High pass must not require a replacement canonical
trace schema or a phase-normalization exception. Retain every tick, ordinal,
payload, actor and ordered event. Never omit phase comparison, discard events,
or accept only final hashes.

Compare all optional draw-state fields, including actual paint metadata, and
require the pure presentation helpers to leave their authoritative subsets
unchanged. One changed character, early icon, duplicate warning, altered STOP
lifetime, modified event/RNG order, or new pause gate fails acceptance. Preserve
the actual timer clock/digit emission metadata and glyph/icon appearance
fingerprints captured in Pass 3A. The 20/60/120 presentation matrix only replays
the already-built display list: High must additionally invoke each new CPU
presentation helper repeatedly with one frozen packet and require identical
paint plus no authority/event/cosmetic-clock advancement. This extra purity gate
is not established by the existing FPS matrix; its future interface is below.
Preserve the old executable/fixtures and stop extraction on the first mismatch.
For any new native crash, retain evidence and stop for user direction before
debugging or reproduction. Classify startup evidence using its exact launch path;
a pre-scene stall is not automatically the historical TLUT AV, and a completed
transaction mismatch is not excused by startup history.

Retain six-quanta canonical transactions and integer expressions in this pass.
The future 120-unit clock distinguishes gated HUD/text eligible time from world
time; it does not convert byte ordinals, enums, array indices or icon counters
into seconds. True 60/120 Player simulation and a general catch-up scheduler remain
later work. All actor pose/collision/culling, Keese draw RNG, unrelated HUD,
nonadmitted text/ocarina and audio scheduling remain authoritative at their
current seams. The exact commands and remaining admission gaps are in TESTING
and PASS3A; completing this specification does not authorize starting High work
in the current pass.

### Direct CPU-helper purity gate (implemented in Pass 3B)

The following Pass 3A contract is now implemented and validated within the
bounded Pass 3B envelope. PASS3B.md owns results and limitations. The native flag
`--verify-presentation-purity` extends the existing `--native-sim-test` invocation,
with a matching `run_corpus.py run --verify-presentation-purity` forwarding option.
Consume and validate it before ROM extraction, reject its use without native test
mode, and retain the existing fresh-output checks. Flag-off production admission,
authority arithmetic and diagnostics must remain unchanged. The flag enables
extra verification calls; it must never determine which gameplay profile uses
the extracted functions.

For every admitted countdown or message packet produced during setup and
measurement, execute the authority and once-only presentation preparation once
at their existing slots. Freeze the resulting packet through the synchronous
CPU build. Run the ordinary presentation helper once, then invoke that same new
helper two extra times with the same const packet. Each extra call receives an
independent scratch command/allocation/matrix context with the same initial
render state and private paint-observation sink. Do not rerun Play_Draw,
Interface_Draw, Message_DrawMain, packet preparation, hook predicates or the
authority traversal. Do not submit the extra commands to the renderer, link
them into the normal display list or advance interpolation recording. Scratch
capacity exhaustion is a failed verification, not a passing empty output.

The test adapter must provide scratch output directly to the helper, rather than
temporarily swapping live PlayState, GraphicsContext, message view or segment
state and restoring it afterwards. The pure helper may append its own command
and allocation output. Its packet and all live inputs remain read-only; restoring
a forbidden mutation after a call cannot satisfy the gate.

Immediately before and after each of the three helper calls, compare named live
fields covering the complete extraction closure, not only the current snapshot
subset. Include main/sub/hazard timer state, hidden divisors and digit caches;
message cursor/mode/delay/shared timer and file statics; font-icon selection;
HUD DoAction fields and resources; the global segment alias; message view,
viewport/projection cache and flags; final text cursor/color/scratch outputs;
character-size and icon-flash state; input edges; gameplay/audio RNG state and
counts; and logical event/audio-ingress counts and fingerprints. Also verify that
the packet's named contents remain unchanged. Pointer-valued live aliases may be
compared within this process to detect a write, but raw pointers and padding
must not enter portable receipts or cross-process hashes. Source review must
account for any reachable mutation target omitted from these direct checks.

Compare each call's ordered timer, glyph, textbox and icon emission metadata:
presence, count, selected digit/glyph/icon/resource identity, position, size,
scale, colors and view parameters. Normalize only scratch allocation addresses
to the corresponding resource or allocation identity. Preserve all scalar bits,
sequence order and packet decisions. An invisible countdown packet emits nothing.
Source review during Pass 3B clarified that the legacy message START/CLOSING
path still emits segment/setup commands when it paints no textbox, glyph or icon;
all three calls must preserve those exact commands and emit no visible paint.
Both kinds pass the same no-live-mutation checks. Extra calls use
private observations and must not alter existing snapshot fields, phase records,
event/RNG ordinals or audio scheduling.

Write a separate schema-versioned `purity.json` alongside the normal output.
Record fixture/config/source/executable identities, fixed repeat count two,
helper/admission identities, setup/measured packet counts, visible/invisible
counts, successful repeat comparisons and the first failing tick/helper/field
or emission. The runner must retain and hash this receipt, require nonzero
exercised coverage of both helpers across the selected fixture suite, and fail
on missing/incomplete receipts or a failed comparison. A direct violation stops
the run with nonzero status and retained evidence; it must not be repaired by
rearming authority or rewriting a reference. The receipt contains metadata only.

Run the original twelve fixtures and all eight draw-state fixtures with the flag
off and on in fresh processes, at least three repeats each. Require exact
canonical snapshots and complete existing phase/event/RNG traces against the
preserved references and between flag-off/on candidate runs. Run the draw-state
analyzer on both, and validate the purity receipts separately. The existing
20/60/120 presentation and trace-off matrices remain separate required gates.
The unsupported 0x305F fixture must retain the complete legacy path and report
zero admitted message packets, while selected 0x1043 fixtures positively exercise
message repeats; a suite that only exercises fallback cannot pass. Add direct
negative admission coverage for the excluded timer/message profiles before
acceptance. No part of this test admits higher-rate simulation or general draw
purity.
