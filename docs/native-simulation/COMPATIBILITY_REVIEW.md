# Player120 compatibility review

Reviewed 2026-10-30 against gameplay checkpoint `4c16f238e` (based on `8dbb8a1d9`).
This is the working review list requested by the user, not a qualification
certificate. All48 entries below have an ownership/evidence review. Detailed
source inspection was concentrated on the unconverted action paths and their
callers; this is not a claim to have audited every line of all429 actors.

The intended architecture remains fresh Player120 controls over world20 actors,
with smooth presentation where supported. Making every enemy simulate at120 is
not required. Nor does a smooth rendering result prove correct action timing.
Historical admissions are not reintroduced as live gameplay restrictions.

**Evidence:** H = prior user acceptance of the stated example; E = bounded engine
check documented in TESTING.md/WORLD_INTERACTIONS.md; S = source review only.
**Open code** means a concrete unconverted operation exists; its exact visible
impact still needs a focused native case. **Check** means a remaining dependency
or coverage question, not a demonstrated bug. H/E never covers an entire row's
possible variants. Successful historical cases were not rerun just for this list.

Unless otherwise named, Player symbols are in
`soh/src/overlays/actors/ovl_player_actor/z_player.c`. Source symbols are given
instead of brittle line numbers. Existing evidence is linked from
[TESTING.md](TESTING.md) and [WORLD_INTERACTIONS.md](WORLD_INTERACTIONS.md).

## Core and presentation

| ID | Reviewed area and source owner | Finding / remaining check |
| --- | --- | --- |
|01|Fresh controls and submission: `PlayerTemporal_AdvanceIntermediate`, OTRGlobals host deadlines|H/E movement, latest next-frame fixture passes. Same-step software submission is established; physical input-to-photon latency is not measured. Check action-specific consumers below.|
|02|Canonical20, rate changes, pause/lifetime: PlayerTemporal `PlayBoundary`, `InvalidateScope`, `ActorDestroyed`|E historical canonical/pause checks; current bean111 hashes exact. Pause/transition/death invalidate high scope. Check pause/equipment/load while an attachment/action is active.|
|03|Animation frames/morph/root motion: `z_skelanime.c` LinkAnimation adapter, animation queue|H/E ordinary movement, attacks and aim. Main/upper Player skeleton clocks are adapted; manual clocks are not automatically covered (39). Root motion is an authored delta, not another velocity to scale.|
|04|Normal/control/aim camera: `z_camera.c` `PlayerCamera_AdvanceControl`, `Camera_Subj3`|H/E normal and settled aim. Unsupported camera modes use world dispatch and ordinary interpolation. Check special room cameras, low health, quakes, water distortion and moving geometry; do not force the narrow fast-camera closure onto them.|
|05|Hands, bow and attached arrow: `z_player_lib.c` post-limb work, `first_person_view`, `EnArrow_Draw`|H accepted after earlier shake/lag repairs. Check hookshot, bottle and carried models separately; bow acceptance does not prove those attachments.|
|06|Horse/rider and CPU skin: `Player_Action_8084CC98`, `z_skin.c`, frame interpolation|H/E mounted motion/skin. Horse gameplay stays20; rider/horse presentation shares interpolation. Jump/fence/archery transitions remain a check.|
|07|Ground shadows, decals and depth: `Actor_Draw`, shadow callback, renderer depth path|H prior terrain/path issue; S shadows remain world-drawn. Check moving-platform/held-object alignment. No speculative depth-bias change in this pass; preserve separate vanilla A/B ability.|
|08|HUD/message/letterbox/target indicators: interface/message extraction, `Letterbox_Interpolate`, `Attention_Update`|H/E message/letterbox fixes. Target selection and several projected indicators remain world-owned. Check reticle alignment against current camera/Player, especially moving hostile targets.|
|09|Pacing/resource/renderer lifetime: OTRGlobals, DXGI/D3D11, resource cache, graph allocation|H accepted ordinary pacing; S no renderer change this batch. Existing shader/readback/buffer fixes retained. Crowded/effect-heavy scenes are a remaining check, not grounds for a new broad performance campaign.|
|10|Cosmetic dynamics/effects/audio: `Player_UpdateBunnyEars`, Player pose/blur pulses|E Bunny Hood spring/two RNG draws now run only at world opportunities;20/120 checks pass, including244 intermediate zero-RNG samples. Ear dynamics remain20. Blur/status effects have separate world/pulse ownership. Audit repeated level-SFX/rumble requests with a bounded event count, rather than suppressing legitimate per-action events.|

## Movement and traversal

| ID | Reviewed area and source owner | Finding / remaining check |
| --- | --- | --- |
|11|Idle/walk/run/turn/target movement: Player movement helpers, `PlayerTemporal_AdvanceMotion`|H/E accepted responsive control. Keep input at120; preserve continuous gain/caps. Remaining action-specific braking is not covered merely by the shared integrator.|
|12|Roll, sidehop, backflip, jump attack: action/landing branches and animation markers|S shared animation/motion adapted, but raw per-call speed/turn changes remain in action branches. **Check** braking/recovery/landing and cancel boundaries; do not blindly scale jump impulses or positional corrections.|
|13|Falls, autojump, land, fall damage: scene collision and hit-response setup|E basic motion/small hit only. Check low/high ledges, fall stun, hard landing and water landing. Injury and state-transition thresholds are not generic velocity scaling.|
|14|Static walls, corners and slopes: `Player_ProcessSceneCollision`, `PlayerContact_ResolveBlockingBody`|H/E wall/body examples. Check acute corners, low ceilings and running diagonally into geometry. Continuous constraint correction must not reapply reciprocal world impulses.|
|15|Vines/ladders: `Player_Action_8084BF1C`, `Player_Action_DismountLadder`|H earlier vine timing correction; S animation completion adapter/camera handshake retained. Check sideways movement, top/bottom dismount and interruption.|
|16|Ledge hang/climb/drop: `Player_Action_8084BBE4`, `8084BDFC`|S root-motion/marker ownership reviewed; raw shape-yaw step remains. Check grab/drop/climb timing and world-platform attachment. No independent native fixture yet.|
|17|Crawl entry/travel/exit: crawl actions, camera handshake/interpolation cut|H crawl movement and exit-flash fix. Parked vanilla black wedge remains cosmetic. Check reverse exit and another tunnel without redoing a capture campaign.|
|18|Hover/ice/conveyors/steep sliding: common friction, hover stamp, `Player_Action_SlideOnSlope`|E hover hold/countdown/cap fixed; ice shares that source but not a scene test. E dedicated steep-slide acceleration/yaw now uses elapsed steps; bounded20/120 checks pass. **Open code:** uphill pushedSpeed still has a raw step. Longer120 terrain path enters a neighboring void and is not qualified. Conveyors enter composed velocity; inspect a real conveyor before claiming equivalence.|
|19|Moving/rotating/falling platforms: `func_800433A4`, `DynaPoly_UpdateBgActorTransforms`|E real Fire Temple elevator20/120:70 world updates, rise272, zero support-height drift. No platform code change. **Check** rendered alignment, step-on/off, rotating/falling platforms; this lift does not qualify those variants.|
|20|Push/pull blocks and switches: `8084B898/8084B9E4`, `func_8084B840`, `func_8002DFA4`, ObjOshihiki|E producer now adds intent only on world opportunities. Real magnitude-sensitive Armos statue20/120:35 pushes/50 pulls, max intent2, max world step1.5;625 intermediate ownership checks. ObjOshihiki uses its sign. Other block types/switches remain checks.|

## Combat and damage

| ID | Reviewed area and source owner | Finding / remaining check |
| --- | --- | --- |
|21|Sword slashes/combo/thrust: melee window, animation end/marker adapter, pose history|H/E basic/combo/targeted thrust and signs. Check jump/crouch slashes, adult heavy sword, wall bounce, moving/armored target. Warm-up and sweep dedup remain required.|
|22|Spin wind-up/release/magic: spin action and charge adapter|H/E adult standing boundaries fixed and accepted. Check quick-spin, moving entry, child charge, low/no magic and interruption; do not infer those from full-charge success.|
|23|Shield/deflection: `PlayerContact_RegisterShield`, `Player_UpdateShieldCollider`|E geometry/order; H ordinary play only. AC-before-AT registration remains unique. Check actual hostile block, projectile reflection and Mirror Shield; existing shield fixture does not prove parry behavior.|
|24|Enemy contact/attack damage: `ConsumeWorldSweeps`, world CollisionCheck|E generic shape bridge, grass/sign/body examples. World target response stays20. Check a mobile enemy, hard shield, multi-part actor, repeated attacks and culling/destruction; no every-enemy claim.|
|25|Small/large knockback and invulnerability: `func_80837C0C`, `8084377C`, common timers|E new small-hit countdown20/120 fixed; body/status timers world-owned. Check actual enemy contact and large knockback separately. Small-hit fixture calls the real public request but is not an enemy encounter.|
|26|Fire and electric visual status: `Player_UpdateBodyBurn/BodyShock`|E prior status-cadence checks; durations/RNG/damage opportunities remain world-owned. This does **not** validate the electric action below. Check real ignition, water extinguish and equipment loss.|
|27|Frozen and electrified actions: `8084FB10`, `8084FBF4`, `func_80832594`|E repaired passive waits/damage; real damage-entry20/120 checks pass. Freeze96 passive opportunities, one fresh A bonus,19 damage pulses; electric20 recovery opportunities. Actual enemy hit and analog struggle still need checks.|
|28|Grabs/stuns: `8084F308`, `func_80832594`, enemy world update|S button escape increments represent edges, not durations; analog angular accumulation needs a boundary test. Check ReDead/Wallmaster/LikeLike behavior, release and lost equipment. Do not divide button presses by six.|
|29|Bombs/bombchus and self-hit: explosive actor world update, body AC|H held-bomb damage now works. Check throw/drop/fuse, bombchu release, explosion during other actions and one damage reaction. World fuse must stay20.|
|30|Death/fairy revival/game-over: `PlayBoundary`, death/revival actions|S DEAD/game-over suspend high updates; raw revival counters are therefore not automatically rate bugs. Check actual death with/without fairy and restart; no intentional native-fault tests.|

## Items and water

| ID | Reviewed area and source owner | Finding / remaining check |
| --- | --- | --- |
|31|Bow/slingshot/fire/ice/light arrows: aim increment, EnArrow|H/E normal aim/release. Projectile flight/collision remains world20. Check elemental magic/ammo, canceled shot and switch/enemy hit; no blanket projectile retiming.|
|32|Hookshot/longshot: `Player_Action_80850AEC`, `ArmsHook_Shoot`|E repaired per-step displacement, pitch and continuous rumble ownership. Real hook actor latch/release20/120 checks pass, pull capped30/5 units. Actual hookable and moving target, longshot range remain checks.|
|33|Boomerang: item launch/animation markers, EnBoom actor|S actor travel/return remains world20. Check release once, catch while moving, stun/collect, target loss and held presentation. Bow release evidence does not cover boomerang.|
|34|Hammer/sticks/nuts: shared melee/item handlers, `8084E604`, burning-stick helper|E stick lifetime only. Check hammer switches/floor impact, stick contact/break/torch and nut release/stun. Animation markers are adapted; actor response/damage mask still needs exercise.|
|35|Bottles/drinking/catching/releasing: `SwingBottle`, `8084EAC0/EED8/EFC0`|S catch is latched, release uses frame markers, meter healing is world-owned. E water scoop audio now fires once on integer-bucket entry;20/120 real-shoreline C-item checks pass. Check catch window, one inventory update, fairy/bugs/fish/blue-fire and empty-bottle message closure.|
|36|Pick up/carry/throw/heavy blocks: `80846050/46120/46260/46358/46408/464B0`, post-limb heldActor|E failed/heavy lift20/120 now preserve15/21 world recovery changes; canonical heavy251 hashes exact. E ordinary pickup alignment8/48 elapsed-step checks now pass. Held transforms still need current-pose presentation checks separate from actor world motion. Check pot/cucco/bomb and silver/gold lift handoffs.|
|37|Bean/trade-item exchange: `ActionHandler_0`, early offer sidecar, `8084279C`|E new real bean20/120 fixture passes one-item consumption, switch and80 world ticks; q61 input acceptedq61. Check one NPC trade and canceled/wrong item; all early actor categories are not thereby qualified.|
|38|Din/Nayru/Farore spells: `808507F4`|E repaired signed wait branches without changing phase ordinals. Real Din/Farore C-item20/120 casts pass magic/spawn/hold checks. Nayru/Fast Farore and return warp remain checks.|
|39|Fishing/shooting gallery: `80850C68`, `8084FA54`, world minigame actors|E manual fishing blend now uses Player duration; gallery closing keeps two world increments. Focused20/120 cases pass with controlled owner handoffs. Full cast/reel/catch and NPC minigame progression remain manual checks.|
|40|Swim/current/dive/Iron Boots: `8084D610/D7C4/D84C/DAB4/DC48`, motion composition|H/E surface/current behavior accepted. E swim entry wait/turnaround braking/surfacing pitch now use their owning clocks; bounded20/120 checks pass. Check dive limits, release/resurface, Iron Boots, underwater ledges/items and carried actors. Do not label all water done.|

## Progression, interfaces and variants

| ID | Reviewed area and source owner | Finding / remaining check |
| --- | --- | --- |
|41|NPC/sign/message/mount A edges: talk closure, mount/early offer sidecars, world input retention|H/E latest NPC/closure/mount regression passes. Check moving NPC, choice, cancellation and simultaneous nearby offers. Keep single-consumption edges and world message ownership.|
|42|Doors/chests/get-item: EnDoor/DoorShutter, `8084E6D4`, item-parent handoff|E real locked EnDoor timing/key/switch fixed. Check shutters, chest size/age, item presentation and control return. Get-item's short counter has mixed phase/wait meaning; inspect a real sequence before changing.|
|43|Ocarina/song handoff: `8084E3C4`, message/ocarina update|S world message/audio owns song recognition; Player animation adapter owns pose. Check enter/cancel/play, song-triggered actor and warp; fresh C-input does not prove song completion.|
|44|Warps/voids/cutscenes: `WaitForCutscene`, `8084F698/F710/F88C`, cue dispatch|E void waits now retain ten world opportunities;20/120 checks pass. Source inspection finds warp-song/Farore return draw=NULL suspends high steps, so their raw waits are not automatically high-rate defects. World cue frames must not be replayed as six logical events. Test representative entry/exit, not every story movie.|
|45|Pause/equipment/save-load/scene transitions: lifecycle invalidation, input mask, playthrough launcher|S/E historical pause/core and isolated-launch checks; new ordinary startup passes. Check equip mid-action, reload, room changes and retained attachment/offer invalidation. No personal saves used by fixtures.|
|46|Pickups, shops and environmental switches: Actor offers, world response|S generic bridge is only infrastructure. Check pickup once, buy/pay once, floor switch persistence and item-triggered switch. Beehive `ObjComb_Wait` deliberately shakes for damage mask0x4001F866 and breaks for other accepted hits: reported failure needs exact weapon/hit, not an assumption that all hits break it.|
|47|Age/equipment/masks/enhancements: Player action/resource choices and hooks|S default ordinary paths only. Child/adult, shield/sword variants and Bunny Hood are not interchangeable evidence. Optional randomizer/free-look/custom models/hooks remain separate compatibility work; no exhaustive enhancement matrix.|
|48|Busy scenes and presentation continuity: world actor rendering, target camera, resources|S/H accepted ordinary session is the baseline. Check one busy encounter and a room transition for visible pops, shadow/attachment mismatch and warmed pacing. New skins/morphs/effects may need their own interpolation; a screenshot alone cannot prove motion quality.|

## Agent-owned next repair order

1. Completed bounded hookshot/frozen/electric/spell repairs (32,27,38); retain
   explicit real-target/enemy-hit/variant limitations above.
2. Completed bounded push/pull accumulation repair and real lift support checks
   (20,19). Other platform variants and rendered alignment remain checks.
3. Completed bounded fishing/gallery manual clocks and carry recovery (39,36); full minigame progression remains a manual check.
4. Completed swim/dedicated slide, mask spring/RNG and bottle event repairs
   (18,40,10,35). Uphill slowdown, ledge yaw and analog struggle remain agent-owned
   checks; preserve the unresolved longer slope trajectory evidence.
5. Focused contact/progression and rendering gaps in the tables, using the player
   route below. Fix demonstrated defects; avoid launching a configuration matrix.

For each repair: identify units and producer/consumer/reset, implement narrowly,
run one20 reference and one120 case when timing is the issue, assert boundaries
online, retain compact receipt, and test only the adjacent shared dependencies.
The rows distinguish repaired and still-open code findings. The source review
covered all48 areas; that is not full implementation or gameplay qualification.
No full-game completion is claimed.

[PLAYTEST_ITINERARY.md](PLAYTEST_ITINERARY.md) turns remaining experience checks
into short sessions; it does not ask the user to rediscover these known code gaps.

After the remaining repairs, provide named direct-boot gameplay scenarios with
age/equipment/position prepared, a short action/expected-result checklist and
verified versus pending setup status. Then, last, try one actual high-rate Stalfos
in the Forest Temple bow arena: movement/animation/contact120, original tactical
choice/reaction cadence. No enemy code has been changed for that experiment yet.
