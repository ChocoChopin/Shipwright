# Pass 3C: Player pose, contact and the nested Player island

Status: design and canonical reference instrumentation in progress. No Player
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
camera's** input yaw. The ordinary B item-action path selects a melee animation
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
the sign currently reads it from live Player during target update. Include any
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
pass. Quake/distortion, water/hot-room/cutscene cameras, non-active cameras and
enhanced free-look/gyro need individual ownership; exclude them from the first
profile. Aiming/projectile profiles are surveyed, not admitted by Z-sword tests.

## Mechanically bounded next High extraction

The next pass is **canonical-only Player pose/contact extraction**, not timing.

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

## Validation, reference inventory and second review

Pending execution and final evidence binding. Fixture and review findings will
be recorded here before acceptance; this section is not a passing test claim.
