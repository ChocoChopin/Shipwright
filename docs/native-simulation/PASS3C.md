# Pass 3C: Player pose, contact and the nested Player island

Status: **acceptance stopped on a new native startup crash**. Design, observation
and the new Player reference corpus are checkpointed; Pass 3C is incomplete. No Player
authority has moved, no timing primitive is implemented, and no higher-rate
gameplay is enabled. Accepted starting checkpoint: `39344b1c4601b9ea3a3b331e8de030a6fd93dfd2`.
The accepted Pass 3B executable and all previous evidence remain the reference.

## Product decision and first interactive milestone

Target runtime modes are **20-Hz reference, 60-Hz Player, 120-Hz Player**.
Unrelated world logic remains **20 Hz**. Rendering FPS is independent. Runtime
30-Hz gameplay is deferred: no settings, admission, combat fixture, deadline
jitter, nonlegacy comparison or release obligation is retained for it. The
120-unit integer timeline stays: Player steps 6/2/1, world step 6. Every 50 ms
is a real common endpoint. The standalone oracle retains inexpensive `s=2/3`
cases to detect helpers that only work for division by three. These are math
tests, not support for 30-Hz gameplay. Authentic 30-Hz menus and Link's authored
animation units are separate existing contracts and remain unchanged.

**Press B at 120 Hz** means a timestamped controller edge can begin the attack
on an intermediate Player step; action, animation, evaluated pose, sword sweep,
Player contact response and necessary control camera advance there. It must work
at 20/60/120 with the world explicitly labelled 20. Six visual samples of one
legacy slash do not qualify. Global enemy/NPC/boss/script/timer/audio/particle
conversion is not a prerequisite. The first milestone may use static ground,
the default child sword/shield and a bounded sign target; dynamic geometry,
projectiles and hostile combat remain outside admission until separately proved.
Latency acceptance must include an already-drawn sword, separating response to
B from the authored equipment-draw animation. Retain cold-equipment coverage too;
do not shorten that animation merely to make a latency measurement look better.

## Source ownership graph

Symbols below refer to the inspected Pass 3B source plus observational hooks.
File paths are repository relative; symbol names survive inserted line numbers.
Categories: **A** true authority; **B** presentation-only data; **C** data produced
by presentation and consumed by gameplay; **D** diagnostic/visual effect;
**E** unresolved or excluded profile requiring additional fixture evidence.
These letters describe ownership, not the A-I timing taxonomy.

```text
PadMgr_ProcessInputs -> GameState_ReqPadData(mode=1) -> Player_Update input copy
  -> Player_UpdateCommon
     -> prior contact/damage, timers, targeting
     -> velocity/integration -> Player_ProcessSceneCollision
     -> control-stick/camera yaw -> actionFunc -> LinkAnimation_Update
     -> queued root motion / cylinder from PRIOR body pose
  -> remaining actors -> AnimationContext_Update -> Camera_Update
  -> Actor_Draw(Player) -> Player_Draw -> Player_DrawGameplay -> Player_DrawImpl
     -> SkelAnime_DrawFlexLod (root, child/sibling traversal)
        -> OverrideLimbGameplayCommon/Default -> limb matrix
        -> PostLimbGameplay -> body/focus/attachments, sword/shield geometry
  -> remaining CPU draw -> audio control/mixer -> transaction snapshot
  -> NEXT transaction: AT -> OC -> Damage -> Player and target updates
```

| Producer / file | Named outputs and consumers | Class / units / lifetime |
|---|---|---|
| `Player_UpdateCommon`, action functions (`ovl_player_actor/z_player.c`) | `skelAnime`, upper animation, action identity, `meleeWeaponState/Animation`, `unk_844/845`, flags, yaw; later pose consumes these | A. Authored animation frames/speed, ordinal action/state, eligible-call combo counter. Actor/process reset; no conversion here. |
| `LinkAnimation_Update`, `LinkAnimation_AnimateFrame`, `AnimationContext_*` (`code/z_skelanime.c`) | Frame load, copy/interp/morph queues and joint tables; `AnimationContext_MoveActor` consumes root translation | A. Link advance uses legacy divisor/2; translation delta is already displacement. Preserve queue order and `prevTransl/prevRot`, movement flags and queue-disable bits. No second root-motion scale. |
| `Player_DrawImpl` -> `SkelAnime_DrawFlexLod/DrawFlexLimbLod` | Matrix traversal, selected mesh/LOD, face textures; post callback receives **original limb display list**, while override selects a different rendered list | B/C. Original-list presence controls body-part cursor; using selected-list visibility instead changes pose indexing. Frame-local matrices; retained outputs below. |
| `Player_OverrideLimbDrawGameplayCommon` (`code/z_player_lib.c`) | Root child scaling, `unk_6C4` offset, `unk_6C2` tilt; head/upper rotations, hand scaling; `D_80160000` cursor begins at `meleeWeaponInfo[2].base` and advances into body parts | C. Model-space transforms to world units, integer binary-angle rotations. Must preserve exact traversal/matrix arithmetic and original mesh-presence convention, not copy an interpolated render matrix. |
| `func_8008F87C` in the thigh override | Floor raycast, changes **live** thigh/shin/foot `jointTable[].z` plus local rotation; hot-floor fire effect | C + D. This is persistent joint mutation, not a pure IK renderer. Next animation copy/morph may consume it. Preserve canonical late evaluation and retain before/after tables. Static floor subset first; hazard surfaces excluded. |
| `Player_PostLimbDrawGameplay` every original nonnull mesh | `bodyPartsPos[]`, `leftHandPos` | C. World positions persist across transactions; cylinder height and yShift use prior head/feet plus current vertical displacement, swimming uses waist/head, items/spawns use hands. |
| Head and feet post callbacks | `actor.focus.pos`, `actor.shape.feetPos[]` | C/D. Focus feeds actors/target/camera in later transactions; feet feed effects/shadow and Player helpers. Keep old values until the late pose slot. |
| Left-hand melee callbacks `func_80090A28`, `func_800906D4`, `func_80090480` | `D_80126080/8C/98` lengths; `unk_845++` for combo extension; `meleeWeaponInfo[0..2]` endpoints/active; two quads and AT registrations | A/C. Model lengths, world endpoints, signed active-window state, ordinal combo count. The draw-side increment can change later gameplay; do not label all attack counters update-owned. |
| `func_80090480` | On inactive: seed endpoints, mark active, reset AT, **no registration**. Unchanged endpoints: reset AT, no registration. Changed active endpoints: quad from new base/tip and retained base/tip, SetAT, then retain new endpoints | C. Swept geometry with a first-sample warm-up. Reset by `func_80832318`; previous/current endpoints are semantic history, not cache. `meleeWeaponState > 0` gates two damaging quads; negative state can still generate trail geometry. |
| `EffectBlure_AddVertex`, `EffectBlureShip_ChangeType` | Trail from weapon 0 endpoints, suppressed when shielding/default trail disabled | D sharing C inputs. Once per reached legacy pose. Trail implementation must not become a second weapon-history owner. Global effects remain world20; future bridge needs ordered trail samples. |
| Right hand/sheath and `Player_UpdateShieldCollider` | `shieldMf`, `shieldQuad` world vertices, material; shielding registers AC then AT; child Hylian back shield uses sheath callback | C/A. World geometry, material enum. Shield pose is not evidence of a blocked incoming attack. Retain registration order and canonical resets. |
| Left-hand held actor branch | Arrow actor world position/rotation; carried-actor rotations; otherwise `mf_9E0`, `unk_3BC` | C/E. Writes another actor during Player draw. Hookshot right hand similarly updates held actor and `unk_3C8`; not admitted by a sword-only fixture. |
| Right-hand shared item reference | `sGetItemRefPos` from hands; held actor world position; `giObjectLoading=false` in item draw | C/E. Scene/item lifecycle and object resource dependencies. Exclude carry/get-item/hookshot/projectile profiles before extraction rather than discarding writes. |
| Bow/slingshot string branch | `unk_858`, `unk_85C` from hand distance | C/E. Shared action fields, not safely cosmetic because a bow string uses them. Explicitly outside the first sword/shield profile. |
| `Player_Draw` outer work | `damageFlickerAnimCounter`, reflection scale flip, selected first-person/crawling callback, frozen/get-item draw, hook predicates | B/D/E. Flicker is eligible-draw byte phase; reflections can evaluate pose twice with negative scale and must not be accidentally admitted. Preserve ordinary legacy fallback wholesale. |
| `Actor_DrawAll` / `Actor_Draw` | projected/culling flags, isDrawn, flagged audio, object segment, shadow work | A/C/B. These outer slots remain unchanged. No global Actor_Draw purity requirement; offscreen admission cannot be inferred from one visible Player. |

The bounded closure includes the ordinary child standing/walking/slash/shield
pose and its static-floor IK. Equipment swaps, first-person, crawling, reflection,
held actors, custom models/limb hooks, cosmetic geometry scaling, stick/bow/hammer,
hookshot, water, climb and damage effects have source ownership entries but are
**not fixture-admitted**. Merely listing a branch is not proving it safe.

## Sword contact and canonical transaction contract

`Player_ProcessControlStick` combines stick angle with the **previous completed
camera's** input yaw. Item-button selection uses `sItemButtons[0]=BTN_B`, the
item/equipment-change path sets `sUseHeldItem`, and `func_8083BB20` gates the
request. `Player_ActionHandler_7` selects a melee animation
via `func_80837818`; `func_80837948` sets `Player_Action_808502D0`, initializes the
combo window/count, starts the animation/root-motion flags and writes toucher
damage masks on both quads. The attack action calls `func_80842DF4` on existing
contact flags and prior sword endpoints; `func_8084285C` selects active/priming/off
using the current authored frame **before** `LinkAnimation_Update` advances it.
`func_80833A20` emits swing/voice SFX on the off-to-active transition. Animation
completion chooses follow-up/recovery; B edges during the eligible action handler
can start the next attack. Do not move active-window evaluation after advance.

For canonical transaction N (input start time `6N`, snapshot endpoint `6N+6`):

1. PadMgr accumulates due edges; mode-1 request copies and clears them. Player
   later copies/masks input for cutscene/cooldown. No second edge consumption.
2. `Play_Update` room work; `CollisionCheck_AT`, `OC`, then `Damage` consume
   registrations left by update/draw N-1. `ClearContext` clears lists, not a new
   synthetic hit result. Preserve freeze/pause/transition gates.
3. Actor category traversal: switch, BG, `DynaPoly_Setup`, Player, explosive,
   NPC, enemy, prop, item-action, misc, boss, door, chest. Stable test IDs never
   reorder this traversal. `prevPos` and Player-relative geometry are sampled
   before each update. Damage is reset after that actor's update.
4. Player consumes collision flags and prior pose, moves using prior action
   velocity, performs background collision, runs current action/animation,
   queues root motion, constructs cylinder from prior head/feet, registers
   cylinder OC/AC (AT for negative invincibility), and resets sword/shield hit
   flags. These resets must not erase a bridge event before consumption.
5. Later target update consumes its AC hit; `EnKanban_Update` tests its actual
   cylinder AC flag and invincibility, reads **live Player melee animation** to
   choose cut direction, changes part flags, spawns a real piece, uses RNG and
   emits its effects/SFX. This is a critical payload alias for a future bridge.
   Enemies similarly own their damage-table response/health/action updates;
   the sign fixture does not validate a general enemy family.
6. Actor attention and DynaPoly previous-transform commit follow traversal.
   Cutscene/effects/message/interface update precede `AnimationContext_Update`;
   queued pose/root translation is completed here, after the cylinder work.
   Camera then updates, followed by environment in their original gates.
7. Full CPU draw visits Player at its original actor-list position. Skeleton
   overrides/IK and post callbacks write current pose and weapon/shield geometry,
   registering quads for collision N+1. Other actor draws, culling, HUD/message
   late authority and RNG remain in place. Audio control/mixer follows. Snapshot
   is taken only after the complete transaction.

AT overlap does not itself decrement all target health. `CollisionCheck_SetATvsAC`
sets AT_HIT/AC_HIT, hit actor/element references, hit effects and position;
AC_HARD causes bounce. Effects may occur immediately here or in the later AT
hit-effect traversal. `CollisionCheck_Damage` applies table/mask/defense semantics
to `colChkInfo.damage`; target update performs its reaction. Player recoil tests
AT_BOUNCED and static sword line tests in `func_80842DF4`; wall-hit particles,
SFX, rumble and backward velocity may occur there. Signed invincibility is a
duration with asymmetric meanings, not a boolean or render flicker.

Consequences: draw N weapons reach AT collision N+1; Player update N consumes
body pose and weapon history from draw N-1; queued root motion occurs later than
cylinder creation; camera N is too late for Player input yaw N but feeds N+1.
Enemy/prop response to draw N geometry normally occurs in update N+1, after
Player's own update in that same traversal. A later extraction must preserve
all these canonical latencies before making Player cadence finer.

`unk_845` has mixed semantics: the first values count combo attacks, while the
third attack's draw callback increments it again to shape sword extension.
Canonical extraction must retain both uses exactly. The later timing pass must
separate combo ordinal from the extension's eligible-pose phase; incrementing the
same integer on every 120-Hz pose would change reach. It is neither a generic
duration nor a counter that can simply be multiplied by three or six.

## Minimal Player / world boundary

Each admitted contact proxy contains actor **generation**, collider/element
identity, authoritative transform/geometry, masks/material, eligibility and
world generation. Between world ticks these are held at the latest authoritative
world state. Render-interpolated target transforms are never collision input.
For draw-owned target colliders, snapshot their actual last committed shape;
do not rebuild it from newer actor position and call it the same proxy.

High-rate Player collision uses a dedicated query/registration set; it must not
append six duplicate Player quads to the global legacy arrays or clear world
registrations on each substep. Keep original 20-Hz code as an explicit exact
path. At high rate, partition Player-involving pairs from world-world pairs so
the canonical boundary cannot process them twice. Preserve pair/element order,
`TOUCH_NEAREST` replacement, AT/AC masks, shield priority and OC mass/correction
semantics. A generic unordered list of intersection points is insufficient.

Proposed contact record (no implementation in this pass):
`{sceneEpoch, playerAttackEpoch, authoredHitOpportunity, playerStep, time_q,
sequence, worldProxyGeneration, attackerGeneration, targetGeneration,
collider/element IDs, damageFlags, attackAnimation, hitPos, normal/material,
bounce/effect, responseState}`. Capture **attackAnimation at contact** because
the sign currently reads it from live Player during target update. Canonical
mode retains that live read after Player update, even if a transition occurred
since geometry generation. The proposed high-rate bridge instead binds the
producing attack; ND-016 records this explicit semantic choice. Include any
other target-specific aliases in its admission contract. Never defer raw Player
or collider pointers and dereference changed state next world tick.

Player-side confirmed contact/bounce, shield state and static-wall feedback are
eligible on the next Player contact-consumption phase, without waiting for the
next world tick. Damage/AI reaction/spawn/drop/world RNG is consumed at the next
world boundary, in legacy category/list/element order with timestamp/sequence
ordering for multiple due events. Keep separately named detected, reserved,
committed and rejected events; a speculative overlap cannot claim target damage.
Before consumption validate target generation/liveness/eligibility. Destruction,
scene reset and profile exit invalidate stale events, not silently retarget them.

Dedup is by attack epoch, **authored hit opportunity**, target generation and
declared element group. Collapse six observations of the same eligible contact;
do not globally forbid all subsequent hits until B is pressed again. Legacy spin,
multi-hit attacks, target cooldown and nearest-hit replacement need explicit
opportunity contracts. The first pilot admits ordinary one-window slashes and
the sign's existing six-world-tick invincibility only. World target acceptance
reserves an opportunity; repeated high-rate observations cannot bypass it.
20-Hz compatibility retains its two quads and existing target checks exactly.

Interactions needing both sides at a finer cadence (moving platforms/riders,
grabs, push blocks, hookshot tethers, parries with moving enemy attacks, reciprocal
OC pushes, projectile impacts) are **NOT_YET_ADMITTED**, not reasons to convert
every actor. Initial static pilot forbids entering them and reports loss of
admission at a controlled boundary. Do not silently degrade an active attack to
20 Hz or discard already accepted contact events.

## Nested scheduling, without waiting 50 ms for B

Controller service (`ControlDeck` / `PadMgr_ProcessInputs`) must be available at
Player cadence independently of world ticks and presentation. A 120-Hz callback
fed only by a 20-Hz input sample does not meet the milestone. Preserve acquisition
and consumption timestamps, and admit zero simulated input lag initially: the
existing controller lag buffer counts reads, so a nonzero setting needs an
explicit physical-duration contract before its sampling cadence changes.

Canonical mode executes the original transaction unchanged. The following is a
proposed high-rate schedule, subject to the later fixtures and timing contracts:

* At shared start `q=6N`, query/consume the previous Player pose against the
  retained world proxy; reconcile pending Player events at the legacy collision
  slot, run world-world collision/damage once, and clear only the owning queues.
  Run world switch/BG prefix and DynaPoly setup, then exactly one Player update
  in its original category slot, then the world suffix. Finish queued animation,
  target selection, camera and late Player pose in their declared legacy-relative
  slots; complete ordinary world draw/UI/audio work once. Refresh the held proxy
  generation only after all its producing world update/draw work commits.
* At intermediate starts `q=6N+2, +4` (60) or `+1..+5` (120), consume fresh
  input edges once, evaluate pending Player contact geometry against that held
  proxy, update admitted Player state/movement/static collision, complete only
  Player-owned animation work, camera/control and pose/contact generation.
  World AI/scripts/timers/effects do not run. Queue resulting target events and
  immutable Player render samples. Audio ingress is ordered; no extra legacy
  mixer wake per Player step.
* At `6N+6`, all three/six Player intervals are complete. Compare snapshots at
  this endpoint **before** beginning the next world transaction. Detect the final
  pending pose at the next contact phase exactly once, as in the canonical
  previous-pose convention. No extra boundary Player update is inserted.

This retains one Player-step pose/contact history; its shorter real-time latency
is an intended resolution consequence. The boundary Player step is split around
world suffix/animation/camera ownership, rather than running an intact world tick
then blindly calling Player six times. Future implementation must split queue
ownership too: flushing the shared animation queue between substeps would rerun
or steal world animation work. World effects/RNG may not be advanced by a helper
used only to obtain a Player pose.

World proxies carry their **actual phase availability** at the shared boundary;
the replay's end-of-transaction snapshot label is not permission to query a
future world generation. World-prefix readers retain the preceding Player
sample, while world-suffix readers see the just-completed boundary Player step.
At high rates that sample represents one smaller Player interval. Preserve this
declared read phase rather than promising identical AI decisions for a changed
Player trajectory; the first controlled target needs no general AI prediction.

At common endpoints require exact 20-Hz compatibility, fixed elapsed time,
world invocation/order, no duplicate input or contact/event identities, declared
duration/root-motion invariants and immutable presentation. Cross-rate Player
position/contact sets need not be bit-identical after intermediate input or a
new valid collision. Classify mathematical errors, expected resolution effects
and design choices separately. World RNG can legitimately differ if a new valid
hit causes a world event; do not promise identical global hashes for such cases.

## Background movement and camera closure

`Actor_UpdateVelocityXZGravity` precedes `Actor_UpdatePos` (legacy velocity gain
1.5); collision displacement is an unscaled geometric correction. Action speed
changes occur later, so an input edge can change action now while ordinary
integration uses prior action speed. Root translation from the animation queue
has another later slot. All three must be represented, not merged into one
naive `position += velocity * dt` expression.

The Player's `Actor_UpdateAll` wrapper is also in the closure: previous position,
freeze/culling eligibility, color-filter countdown, update-hook dispatch and
post-update damage reset have owners outside `Player_UpdateCommon`. A future
Player-step adapter must preserve or explicitly gate those operations; invoking
the Player callback repeatedly without its wrapper is incomplete. The first
profile excludes freeze/damage/filter transitions until their contracts pass.

`Player_ProcessSceneCollision` saves prior floor property, selects wall radius
and ceiling height, calls `Actor_UpdateBgCheckInfo`, then consumes ground/wall/
ceiling/water flags, surface type, floor pitch, conveyor state, exits and ledge
tests. `Actor_UpdateBgCheckInfo` first applies the previous/current DynaPoly
carry transform when standing on dynamic ground, then wall sweep against prevPos,
ceiling from prevY plus displacement, floor snap/landing tests and water entry.
Player then tests the interact wall, wall angle, ledge height/floor/ceiling and
delay before choosing climb/step actions. Some flags are refreshed; others are
retained/cleared conditionally. Preserve previous position **per Player step**
separately from the world actor's previous sample in a multirate implementation.

Initial admission requires scene/static floor and wall IDs, ordinary boots,
nonhazard/nonconveyor dry surfaces, no dynamic rider/push/grab, no water/ledge
transition/scene exit. Static floor/wall queries and pose IK belong PLAYER_RATE.
Moving DynaPoly is excluded even if its actor has not moved in one fixture;
`DynaPoly_Setup`, interaction flags and transform-history commit currently run
once per world traversal. An independent moving-platform design is needed later.
The static-wall reference proves its tested wall/floor path, not all slopes,
ceilings, stairs and ledges. Additional coverage is an admission prerequisite.

`Player_UpdateZTargeting` reads held/press Z, the prior `Attention_Update`
candidate, lock-on leash and target flags; it also writes targetPriority on the
target. Candidate selection/leash queries and Player lock-on state must respond
at Player cadence against held authoritative target proxies. Target-priority and
world-facing changes require the event bridge. General NPC attention/Navi/world
decision logic can stay 20; separating selection from reticle/Navi effects is
part of the later closure. Default switch-target semantics must be retained.

`Player_ProcessControlStick`/movement yaw use `Camera_GetInputDirYaw`. Player
requests camera mode before `Camera_Update`, which computes displacement-derived
`xzSpeed/speedRatio`, background tests, smoothing, eye/at and inputDir after the
animation queue. These gameplay camera fields must share Player cadence; merely
interpolating its picture leaves input direction coarse. Preserve previous-camera
feedback at 20; use one previous Player-camera sample at high rates. Convert the
implicit per-update speed units and bounded smoothers only in the later timing
pass. The bounded closure includes `CAM_MODE_NORMAL` (0), `CAM_MODE_TARGET`
(1, parallel), `CAM_MODE_FOLLOWTARGET` (2, friendly target) and **`CAM_MODE_STILL`
(18)**. `Player_UpdateCamAndSeqModes` selects STILL during ordinary active sword
slashes, as the no-target reference confirms. Excluding STILL would interrupt
the very attack the pilot is meant to admit. Its normal outdoor setting uses
`Camera_Normal1` with separate STILL parameters; preserve its mode-entry state
and feedback, not just the mode enum. Quake/distortion, water/hot-room/cutscene cameras, non-active cameras and
enhanced free-look/gyro need individual ownership; exclude them from the first
profile. Aiming/projectile profiles are surveyed, not admitted by Z-sword tests.

## Mechanically bounded next High extraction

The next pass is **canonical-only Player pose/contact extraction**, not timing.

Keep the implementation centered in `soh/src/code/z_player_lib.c`, with a bounded
packet/interface declaration in `soh/include/player_pose.h` and direct purity
checks in `soh/soh/NativeSimulationTest.cpp`. The call site is the current
`Player_DrawImpl` skeleton slot after `sDListsLodOffset` selection. If traversal
support is needed in `soh/src/code/z_skelanime.c`, add an explicit Player-only
policy/entry point preserving root/child/sibling order and matrix-stack behavior;
leave the general actor walker unchanged. `ovl_player_actor/z_player.c` supplies
admission state but its update/action ordering is not moved in this next pass.
Incoming actor/model matrices and resource references are captured once at the
original slot; repeated emission cannot fetch a newer gameplay transform.

1. Add an explicit `Player_IsPoseProfileAdmitted` before any mutation at the
   original Player draw slot. Admit only fixture-covered age/equipment/actions,
   default resources/geometry settings, scene and static-surface profile. Reject
   pause, transition, draw-disabled, reflection, first-person/crawl, custom model
   or limb hooks, carry/get-item, alternate weapon and dynamic interactions as
   a whole. Production admission must not depend on the replay flag.
2. At that same slot call `Player_AdvancePoseContactsLegacy(play, player, packet)`
   once. Split Common/Default override and PostLimb callbacks into authoritative
   transform/IK/body/contact/attachment work and emission. Preserve the exact
   `SkelAnime_DrawFlexLod` traversal, original-vs-selected display-list distinction,
   all matrix operations, joint mutation, warm-up/no-motion collider branches,
   combo increment, AT/AC order, hook evaluation and effect/audio ingress.
3. Keep live authority in Player/collider/context storage. Packet is an immutable
   **presentation result**, never a competing collider. It contains selected
   meshes/resources, limb matrices, segment/material/face/LOD decisions and
   ordered extra emissions, with scene epoch and generation. Own it through the
   synchronous actor draw; no raw frame-arena pointer may survive into another
   transaction. Future inter-step rendering needs retained resource/arena ownership.
4. `Player_DrawPosePresentation(const packet, output)` only emits commands into
   its own frame/scratch arena. It must not query mutable gameplay, rerun IK,
   callbacks/hooks, append blur vertices, change joints/combo/flicker or register
   colliders. Preserve actor outer projection/culling/audio/shadow slots. No
   global Actor_Draw rewrite or prior-pose deletion is authorized.
5. Keep prior body/focus/weapon/camera state readable until its original consumer
   completes. Reset attack history only at the legacy reset sites; first active
   sample still has no damaging sweep. Scene/actor initialization resets generation
   and discards packets; a scene swap cannot interpolate across epochs. Unsupported
   profiles take the complete existing path before touching any extracted state.
6. Acceptance: old Pass 3B corpora exact; all new Player snapshots and full traces
   exact including phase/contact/registration records; repeated new CPU helper
   calls produce identical packet/commands and **zero live mutations**. Direct
   same-process checks cover complete Player, joint/static state, colliders/lists,
   held actors, effects, RNG/audio/hook counts and packet bytes. Display-list FPS
   replay alone cannot prove CPU helper purity. Add negative admission coverage
   and normal-exit mismatch controls. Stop on any new native crash.

Subsequent High work implements the clock/primitives and input/animation/camera
queue ownership, then the gated static Player/contact bridge. Do not retime while
establishing canonical extraction equality. The earliest credible planning
estimate is **three further implementation passes** after this reference pass:
canonical extraction; primitives/Player cadence and debugger prerequisites;
static Player integration/contact bridge with interactive acceptance. Complex
review findings may split these further; this is not a guaranteed delivery date.

## QA facilities

Player function statics remain part of the extraction closure: control-stick
magnitude/angle/world yaw, floor type/previous property, touched-wall flags,
floor distance, conveyor speed/yaw and hand/LOD/body cursor scratch. Preserve
their existing process/Player reset behavior until a documented sidecar lifecycle
replaces it. `Player_UpdateInterface`, `Player_UpdateCamAndSeqModes` and blink
RNG must be split into Player response versus once-per-world opportunity work;
the entire `Player_UpdateCommon` function is not a ready-made high-rate unit.
`shieldMf` also feeds Nutsball/Okuta/Honotrap/Twinrova reflection directions,
while hookshot consumes `unk_3C8`; these readers remain outside the first profile.

Available: fresh-process fixture launcher, exact state/animation/geometry dumps,
timestamped input records, Player phase/contact/registration trace, background
flags/polygons, camera state, full-frame snapshots, first-difference comparator
and one-command corpus capture. New Player detail is opt-in and leaves existing
fixtures' schemas and authoritative event fingerprints unchanged.

Before the high-rate pilot add a small simulation pause and single-canonical-step
control, followed by single-Player-step and next-world-boundary stepping. They
must operate on scheduler debt/clock ownership, not the current frame-advance
flag which suppresses registration. Freeze input capture/consumption policy
explicitly and display queued versus consumed edges. A simple inspector reads
committed state, animation frame, pose generation, sword endpoints/quads/contact
queue, target generation, BgCheck flags and effective Player/world Hz. Semantic
dumps suffice initially; an in-world collider overlay is optional. Capture one
recipe/input/config/build/asset receipt with traces using the existing runner;
do not build a generalized debugger or manipulate personal saves.

## Reference fixture scope and measured findings

All recipes live under `scripts/native-simulation/fixtures/player`, use three
fresh processes, fixed seed 1314083889, 60 setup transactions and canonical
20-Hz gameplay. Kokiri Forest entrance 238 supplies ordinary sword use and the
Kanban object bank; the static-wall case uses Link's House entrance 187.

| Fixture | Measured transactions | Required exercised behavior |
|---|---:|---|
| `player-idle` | 40 | Continuous pose generation and floor IK, no input |
| `player-slash` | 70 | Equipment draw, full slash, active history, recovery |
| `player-combo` | 110 | Two animation-4 attacks then animation-6 third attack; draw-owned counter progression |
| `player-shield` | 70 | R posture, live shield matrix/quad, ordered AC then AT registration |
| `player-z-slash` | 80 | Held Z plus B, camera and targeting state through attack |
| `player-turn-attack` | 90 | Opposed stick directions around a slash |
| `player-move-attack` | 100 | Movement before attack and again after recovery |
| `player-static-wall` | 120 | Original wall-movement recipe, real static wall/floor flags |
| `player-sign` | 80 | Real EnKanban placed 45 units ahead; real sword collision and cut-piece response |
| `player-z-sign` | 100 | Held Z plus real controlled sign contact |

The first B press includes equipping/drawing the sword. In `player-slash`, the
edge is due at transaction 10, attack state first appears at snapshot 14, priming
at 15, active state at 16, and recovery at 19. Snapshot K is the endpoint of
transaction K-1. The two sword quads first register during **draw transaction 16**,
then again at 17; the first active sample initializes history without registering.
This equipment/priming delay is distinct from the pose-to-collision delay.

In `player-sign`, quad registration occurs at draw transactions 21 and 22.
Collision transactions 22 and 23 each report both quads against the same sign:
four contact records, but only one target cut at transaction 22, after Player
update. Sign parts change 65535 -> 65204 and the actual target cooldown suppresses
the subsequent reaction. The bridge must preserve contact-versus-response
semantics; one overlap is not one damage event. No sign logic was replaced by a
test-only collision response.

`player-z-sign` also observes target `scene1:spawn93` and friendly-target camera
mode 2 after the Z edge. The analyzer requires a real sign identity plus that
camera mode, so a Z press with no acquired target cannot satisfy this case.

The combo starts at snapshots 14/21/28 with counts 1/2/3 and animation IDs 4/4/6.
Draw then mutates the third-attack counter on eight transactions. Idle alone
changes live joints during all 40 measured pose evaluations. Shield records show
AC registration before AT at each held-shield pose. These are observed ownership
facts, not only inferred source dependencies. Shield block/bounce against an
incoming attack, hostile targets, dynamic platforms and all equipment variants
remain outside fixture acceptance.

## Second deliberate review

This was a second source-and-evidence review by the implementing agent, not a
separate-agent approval. The following checks refined the design before handoff:

| Risk checked | Result / required later guard |
|---|---|
| Hidden draw authority | Preserve live IK joint writes, original-mesh body cursor, combo increment, attachments and weapon history; do not copy interpolated matrices. |
| Prior/current pose latency | Analyzer checks collision against prior snapshot weapon quads and Player update against prior body pose on every measured transaction. |
| Duplicate registration / moved collision | Successful registrations stay in late pose; at most one slot per quad/category; contact records remain inside the original AT slot. Player/world pair partition is mandatory later. |
| Active-window change | Keep pre-animation-advance active-window decision, negative priming and first-sample no-registration behavior. |
| Duplicate damage / target alias | Four real contacts produce one sign cut. Keep authored opportunity and target cooldown; capture attack identity in future events, preserve canonical live Player read (ND-016/017). |
| Repeated animation events / input edges | Separate queue owners and once-only marker/edge identities before substepping; never flush all world animation work at Player cadence. |
| Inconsistent target sampling | Hold committed authoritative geometry with actor and world generations; no interpolated enemy transform, no deferred raw collider pointer. |
| Camera feedback | Preserve previous camera input yaw and convert displacement-derived camera speed units only in a later pass. Source/trace review added slash-selected STILL (18) to the closure. Selection and world-facing targetPriority have separate owners. |
| Hidden DynaPoly dependence | Static profile must reject dynamic floor/wall IDs, carry, push/grab and moving-platform coupling even when one sampled transform appears stationary. |
| Mixed Player/world work | Blink RNG, HUD, sequences, effects and target reactions retain declared world opportunities; whole Player_UpdateCommon repetition is inadmissible. |
| Abandoned 30-Hz burden / fixed divisors | Runtime targets 20/60/120, shared 50-ms endpoints; no runtime30 jitter/QA/UI contract. Keep s=2/3 oracle adversary; no new timing implementation or /3-/6-specific gameplay patch. |

## Validation and evidence binding

Runtime source checkpoint: `2a977ba63b46dc140e181d12dd643ccb46a602c7`.
The Release build used base `39344b1c4` plus the six captured observational source
edits. `build/pass3c-evidence/runtime-binding.json` verifies each raw compiler
input hash and its LF/CRLF-equivalent committed blob; this is not a claim that the
embedded build-version string was regenerated after the commit. Executable:
27,045,888 bytes, SHA-256
`8cc49fc4e3ebd3d2978f45f70bd2bd5bf7f857c2a41e911f40acebe0510b1d1e`.
One candidate copy is retained under `build/pass3c-evidence/reviewed-runtime`;
launches use the original `x64/Release/soh.exe` location. The accepted Pass 3B
reference remains untouched; this candidate has not passed final acceptance.

Dependencies are unchanged: libultraship
`c6bbb8c328938c115f4a1cbeaca3d00a4502269d`, Torch
`2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. The build retains MSVC 19.44.35228,
Windows SDK 10.0.26100.0 and CMake 4.4.3; tooling uses Python 3.12.5.
Per-run provenance binds the existing assets: `oot.o2r` (33,569,565 bytes,
`a058767a2f4f8f415b099a5c5189a9bf974a068f331b88131afea8df24e6a997`),
`soh.o2r` (4,443,452 bytes,
`51c8b166b913fdc745901fc48a2ca6261e480e3e8c0715ae0f2d946adb982712`),
and `gamecontrollerdb.txt` (609,830 bytes,
`e606134678e6b3fdbdec9081289a1f0ba3e25d53b59b2aa3cdfe69bf491ad18f`).
No proprietary content, local output or submodule pointer is committed.

The new Player phase audit covers **2,580 transactions / 2,610 snapshots** in
30 runs. It observes 2,451 pose-time joint mutations, 24 combo-counter mutations,
5,466 Player registrations, 24 raw contacts and six real target cut responses.
These counts are reference coverage, not elapsed-time conversion or a general
combat acceptance claim. `player_state_result.json` binds each inspected trace.

The source inventory is 2,432 files / 697,204 lines, 1,524 candidate files /
63,859 candidate lines, 429 resolved table actors and one separately registered
actor. All 430 claims remain unclaimed. Two scans are byte-identical across all
four generated files; heuristic counts are not completeness proofs.

### Completed gates and crash stop

| Gate | Result / retained path under `build/` |
|---|---|
| Release build and source binding | PASS; `pass3c-evidence/build-01`, `runtime-binding.json` |
| Python tooling | 129 PASS; `pass3c-evidence/python-tests-final.log` |
| Native CLI / TLUT / coverage counter | 43 / 31 / 20 PASS; `pass3c-cli-01`, `pass3c-tlut-01`, `pass3c-coverage-01` |
| Original corpus, purity off/on | 36 + 36 PASS, exact Pass 3B reference; `pass3c-original-off-01`, `pass3c-original-on-01` |
| HUD/message corpus, purity off | 24 PASS plus strict phase/paint analysis; `pass3c-draw-off-01` |
| New Player corpus and phase analysis | 30 PASS; `pass3c-player-01` |
| HUD/message corpus, purity on | **INCOMPLETE**: nine successful runs, one failed startup, 14 unattempted; `pass3c-draw-on-01` |
| Inventory | Four outputs byte-identical across two scans; `pass3c-evidence/inventory-binding.json` |
| Presentation matrices / remaining controls / startup acceptance | **NOT RUN** after campaign stop |

The tenth purity-on draw attempt, `hud-zero-input/run-001`, logged native
exception **0xc0000005** at 2026-10-03 18:55:46 local time. The runner later
recorded its 120-second timeout. `snapshots.jsonl` is empty; the partial trace,
process log, crash-handler log and staged inputs remain intact. A timeout bucket
in the corpus receipt does not establish an environment-only failure: the saved
crash-handler log confirms a native exception. Its cause is **undetermined**.

The sequential driver stopped at this failed gate. Only saved failure metadata
and the crash-handler log were read to classify it. No root-cause analysis,
reproduction, source fix or further engine launch followed. The completed count is
**135 successful canonical processes**, with one separately retained failed
attempt. Those successes do not close the remaining acceptance requirements.
`pass3c-evidence/crash-stop.json` binds the failure files and completed-gate
receipts; `campaign.json` remains stopped. No final passing receipt is issued.

Earlier tooling-only failures are also retained: `python-tests-01.log` had stale
40-case CLI expectations after adding three checks; `python-tests-02.log` hit a
Windows sandbox file-replacement denial. The count correction and execution with
repository-local temp outside that wrapper passed the final 129-test suite.
All prior Pass 2/3A/3B failures and accepted references remain preserved.

Successful runs released their staged asset links/copies. The 60 new Player
snapshot/trace files retain their pre-compression hashes while lossless Windows
file compression reduces 2,157,975,438 logical bytes to 290,144,256 stored bytes.
No original assets or failed evidence were deleted. Free space at the stop receipt
was 40,232,890,368 bytes (about 37.5 GiB).

**Resume boundary:** obtain user direction before investigating or reproducing
the native crash, as required by request section 20 and AGENTS.md. Preserve this
failed output permanently; any authorized continuation uses fresh paths and
explicitly reconciles completed versus outstanding counts. Then complete the
remaining compatibility/purity/matrix/control/startup gates and a final receipt
audit before accepting Pass 3C. The next High extraction specified above remains
unstarted. The estimate of three implementation passes follows completion of
this reference pass and its unresolved validation prerequisite.
