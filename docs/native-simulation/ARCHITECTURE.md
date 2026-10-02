# Native simulation architecture

Status: source reconnaissance and proposed architecture only. No simulation-rate support is implemented by this document. Source anchors refer to upstream `HarbourMasters/Shipwright` commit `9eafd15fe1382c5a41e881f1b6ea87345c797d18` and its pinned `libultraship` submodule. Re-resolve anchors after rebasing. The build receipt records the exact submodule identity.

Read this with [TIMING_SEMANTICS.md](TIMING_SEMANTICS.md), [TESTING.md](TESTING.md), [CONVERSION_LEDGER.md](CONVERSION_LEDGER.md), and [EXECPLAN.md](EXECPLAN.md). Sections labelled **Observed** describe code inspected in this pass; sections labelled **Proposed** are contracts to implement and test, not claims about existing behavior.

## 1. Architectural decision

Use one deterministic fixed-step world simulation with selectable rates `{20, 30, 60, 120}`. Represent elapsed simulation time exactly on a 120-quanta-per-second integer timeline. Keep rendering and audio scheduling separate. Preserve an executable canonical compatibility path and its authentic non-world cadences while the conversion progresses.

Do not implement this by replacing `R_UPDATE_RATE`, speeding up the outer loop, invoking Player several times per actor pass, or continuing to update the whole world only every 50 ms. At an admitted native rate, Player, collision, actors, camera state used by gameplay, and their condition checks must actually run at that rate. Explicitly classified discrete opportunities may retain their original cadence; that exception must never become a hidden 20-Hz world scheduler.

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

**Proposed migration:** before enabling any native-rate world fixture, capture the legacy transaction trace, then extract pose evaluation, collider preparation, culling, draw-owned timers/RNG/events into named authoritative stages at their existing relative positions. Do this at canonical rates first. Initially keep the existing command-generation call once per canonical transaction so instrumentation does not accidentally suppress side effects; only repeated display-list presentation is independent. Do not run all of `Play_Draw` at 120 Hz merely to obtain fresh poses while leaving its timer/RNG consumers unconverted. An admitted high-rate fixture must have converted or explicitly time-domain-preserved all reachable draw-side mutations.

Rendering becomes read-only with respect to authoritative state only after this work passes state/RNG/transition comparison. A debug assertion comparing a curated authoritative hash before/after render preparation helps find remaining writes, but a hash is only as complete as its field coverage. Cosmetic RNG and presentation state also require explicit classification; simply deleting their calls from the global RNG sequence breaks the canonical reference.

## 6. Proposed temporal core

Names below are conceptual API names; this pass does not establish a new ABI.

```text
SimulationRate:          enum { Hz20, Hz30, Hz60, Hz120 }
SimTime:                 unsigned 64-bit integer quanta (120 / second)
SimDuration:             signed/unsigned duration with explicit sentinel policy
WorldStepContext:        { rate, stepQuanta, start, end, stepId, sceneEpoch }
LegacyClockContext:      { domain, sourcePeriodQuanta, paused, opportunityPhase }
InputTimeline:           timestamped physical/replay samples and edge sequence
SimulationTransaction:  one complete ordered authoritative world step
RenderPacket:            immutable presentation data + interval + epoch
AudioEvent:              logical event ID, simulation timestamp, parameters
```

| Hz | Exact step in quanta | Seconds per step | Relative normal-world step `r = 20/Hz` |
| --- | --- | --- | --- |
| 20 | 6 | 1/20 | 1 |
| 30 | 4 | 1/30 | 2/3 |
| 60 | 2 | 1/60 | 1/3 |
| 120 | 1 | 1/120 | 1/6 |

This integer quantum is a representation, **not a mandatory hidden 120-Hz physics loop**. A 20-Hz step advances six quanta once; a 30-Hz step advances four once. Do not subdivide every 30-Hz step into four authoritative 120-Hz steps and call that 30-Hz mode.

Common comparison endpoints for all four modes occur every `lcm(6,4,2,1)=12` quanta = **100 ms**. A 50-ms endpoint is not present in a 30-Hz run. Intermediate trace/event comparison remains useful; do not fabricate an interpolated state and call it an authoritative common sample.

Wall-clock input timestamps may use higher resolution than 1/120 second. Do not quantize human/replay input events to the simulation quantum before assigning them to step boundaries. Keep integer/rational conversion for wall-clock deadlines so repeated truncation of 1/30 or 1/120 seconds cannot accumulate drift. Simulation time advances only by committed fixed steps, never by measured rendering delta.

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

During conversion, expose high rates only to explicit test fixtures whose entire dependency closure is admitted by the ledger. Keep ordinary gameplay at canonical mode until the scene/subsystem support matrix is complete. The ledger must distinguish inspected, instrumented, canonical-preserving refactored, rate-converted, and admitted/tested. No silent fallback to 20-Hz authoritative world stepping is allowed while reporting native 60/120 support.

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
2. **No render-driven step count.** 120-Hz world/30-Hz presentation executes four world steps for every present on average; 30-Hz world/120-Hz presentation executes one world step and interpolated presentations. Rational phase handles non-integer ratios.
3. **Preserve simulation debt.** Bound the number of catch-up steps in one host iteration for UI responsiveness, but do not change dt or drop authoritative steps to catch up. If performance is insufficient, report it and run slower or pause explicitly. Record host suspension/clock discontinuities as a pause/reset of the wall-clock anchor rather than inventing a giant physics dt.
4. **Deterministic offline execution ignores wall time.** Run exactly N admitted transactions or to an exact SimTime endpoint. Rendering and audio device buffering must not determine N.
5. **Ordered event consumption.** External requests (reset/load/config changes) enter at an explicit transaction boundary, not during an actor callback. Same-timestamp ordering is documented and replayed.
6. **Do not run simultaneous worlds initially.** `gPlayState`, `gSaveContext`, static actor/camera/Player state, graphics arenas, and singleton services make isolated process runs the practical comparison mechanism. This is not a requirement to make the entire engine reentrant.

### Input boundary policy

Use a timestamped sample/event queue. For an interval beginning at `t`, consume events with timestamps `<= t` in stable `(timestamp, sequence)` order and latch the resulting level/axis state. Derive a transition edge once for that consuming boundary; never repeat it on subsequent catch-up steps. Define an initial sample at `t=0`. A newly arriving live event is consumed at the first not-yet-committed eligible boundary, and its observed availability must be recorded so replay does not pretend the system knew it earlier.

This policy makes cross-rate input timing explicit: the same event may first affect different boundary times at 20 and 120 Hz. That response-latency difference is expected. Tests that need equal initial conditions may align input changes to common boundaries; latency tests deliberately avoid doing so. Specify treatment of multiple press/release edges between two boundaries (legacy accumulated bit behavior versus an ordered edge queue) and preserve the canonical contract. Do not silently discard short taps or replay one press multiple times.

Controllers, keyboard/mouse/gyro and menu interception must share the policy. Pumping desktop events at render cadence alone is insufficient for native input response. Controller sample history and simulated lag need time-based interpretation or explicitly retained update-count semantics with distinct labels. Test injection must bypass live devices at one documented layer, rather than fighting physical input through several hooks.

### Rate selection and switching

Initially latch the selected rate at a cold launch or controlled fixture/scene reset. A developer-only setting may show a pending request, but effective rate is separate and logged. A reset boundary avoids half-converted timer residues, animation histories, pending input edges, audio grouping and interpolation pairs. Supporting live mid-action rate changes is not required to support four fixed-rate modes.

Only after duration/state migration is complete should live changes be considered. Commit a request at an explicit quiescent common timeline boundary (100 ms is available to all four rates), after completing the old transaction and with no load/save/transition operation in flight. Keep elapsed time and duration deadlines invariant; invalidate presentation history, retain ordered pending input, and record the effective change. Do not round elapsed time forward, rescale velocities blindly, discard remainders, or clear timers. Actor/animation state whose source-unit meaning changes must have a declared migration rule. Freeze/pause domains may require a stronger boundary than elapsed-time alignment alone.

## 8. Proposed rendering and audio separation

### Presentation

After authoritative pose/culling work has been extracted and its consumers converted, commit an immutable presentation snapshot/packet with `(sceneEpoch, stepStart, stepEnd, rate, ownership)`. Current matrix-recording interpolation may remain as an adapter, but its gating must be based on valid samples and selected presentation policy rather than `renderFPS != 20`. The `20 = original` presentation sentinel must not be confused with `worldHz = 20`.

Keep an adjacent committed state pair for interpolation and an explicit camera/actor discontinuity epoch. At render rates below simulation, retain the latest suitable pair; do not interpolate across skipped scenes or reused actor pointers. At render rates above simulation, alpha refers only to that pair. Interpolation normally adds up to one simulation interval of visual latency; choose and measure the scheduling convention deliberately. Extrapolation/input prediction is separate and not required for this pass.

Graphics display lists reference transient matrices, arenas and resources. Do not queue raw `Gfx*` indefinitely while the next simulation step overwrites its backing arena. Either retain/double-or-triple-buffer ownership through consumption or copy the needed immutable data. Repeated presentation must not execute an authoritative hook twice. Test changes in render FPS, VSync, window focus, dropped presentations and resizing against the same simulation trace.

### Audio

Current audio work is explicitly tied to source transactions: one wake at `Graph_ProcessGfxCommands`, then `R_UPDATE_RATE` blocks of approximately 60-Hz audio work (`soh/soh/OTRGlobals.cpp:1040`, `:1059`). The fixed stack buffer only reserves three blocks (`:1065`). At 120 Hz, `60 / Hz` is one half, so a replacement integer divisor cannot work. Running one full audio block every 120-Hz world step would double audio time.

Give the audio engine a rational 60-Hz work clock (two simulation quanta per block) and preserve its sample-buffer control independently from world steps. At 20 Hz that represents three blocks per normal-world interval, at 30 two, at 60 one, and at 120 alternating due/no-due blocks. Define ordering at shared deadlines and keep the canonical sequence of event dispatch, audio logic and RNG calls. Device fullness may change sample counts as upstream does, but deterministic tests need a controlled sink and recorded logical events; an audio event hash alone does not prove the audio engine deterministic.

Audio is not safely assumed cosmetic: audio RNG/timing and ocarina/minigame decisions require the audit described in `TESTING.md`. Decouple worker buffer production with bounded ownership and synchronization; do not let an audio thread read a mutable global divisor while another thread changes it. Sequencer/control state affecting gameplay must have deterministic ownership even if PCM mixing is offloaded.

## 9. Configuration and extension surface

Observed existing FPS UI is `CVAR_SETTING("InterpolationFPS")` in `soh/soh/SohGui/SohMenuSettings.cpp:381`, with values from 20 to the presentation maximum and "Original" formatting. Legacy CVar migration already maps `gInterpolationFPS` to `gSettings.InterpolationFPS` (`soh/soh/config/ConfigUpdaters.cpp:62`, `:72`). Console variables persist in config (`libultraship/src/ship/config/ConsoleVariable.cpp:243`, `:279`). `R_UPDATE_RATE` itself is an `s16` debug register, not a validated simulation enum (`soh/include/regs.h:9`, `:51`; `soh/include/z64.h:93`).

Proposed initial key: `gDeveloperTools.NativeSimulationRate`, validated against exactly `{20,30,60,120}`, default 20, with a separate effective rate in diagnostics. This name is a proposal and can be finalized with the phase-1 API. Invalid persisted values must fall back deterministically to 20 with a diagnostic. The production-facing setting should only appear after admission gates; label simulation Hz independently from rendering FPS. Refresh-rate matching must never change simulation Hz.

GameInteractor hooks, enhancers, cheats, networking, statistics and randomizer logic execute within these paths. Give hooks an explicit step context or documented clock, preserving canonical hook order. Audit mutable config read inside every tick and freeze the test configuration manifest. Do not count hook conversion complete merely because the base C actor was converted.

## 10. Complete direct `R_UPDATE_RATE` source inventory

At this base, `rg -n R_UPDATE_RATE soh/src soh/soh soh/include -g '*.[ch]' -g '*.cpp' -g '*.h'` reports **59 matching lines**. These are all direct token matches, including one definition, a comment and the debugger row. Register aliases or equivalent constants require the broader semantic inventory; 59 is not the number of timing assumptions.

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

Before native world execution is admitted, require:

1. Repeated canonical runs produce stable explicit snapshots and traces under controlled seeds, clocks, input, assets, configuration and audio sink.
2. Observational instrumentation and authoritative-draw extraction preserve canonical ordering/state/RNG; GUI/window/presentation rate changes do not change the authoritative trace after separation.
3. Shared helper tests cover all four rates, source-unit families, clamps, wraparound, negative/sentinel timers, zero/one-step durations, and cast residue. Canonical arithmetic retains the original results.
4. Native input edges are consumed once; real-time duration/movement/animation invariants are checked at common authoritative boundaries and event traces explain intermediate differences.
5. Scene/pause/menu/transition/save-load boundaries have explicit clock/reset ownership. No effective rate changes halfway through a transaction.
6. 120-Hz simulation with lower rendering executes all authoritative steps; higher rendering executes no extra authority. CPU/GPU/audio backlog and dropped presentations are observable.
7. Results distinguish conversion bugs, expected resolution consequences, and unresolved design semantics. The ledger and divergences document identify the chosen policy for each admitted fixture.

Highest-risk unresolved details are the full draw mutation inventory, collision-registration phase preservation, global/audio/cosmetic RNG coupling, integer fractional residue storage and reset, camera displacement units, asynchronous resource readiness, and actor/hook update order. Cross-platform bitwise floating-point reproducibility is not proven by selecting a fixed dt; initially pin executable/toolchain/config and compare same-build runs. Performance at 120 Hz is a separate measured gate, not implied by correctness of timing formulas.

The recommended next architectural action is canonical-only deterministic instrumentation and draw-side-effect tracing, followed by a small canonical-preserving extraction. Do not begin bulk actor conversion or present 30/60/120 as working before those gates.
