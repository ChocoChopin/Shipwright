# Player/world interaction implementation

## Playthrough handoff (2026-10-29)

The accumulated human-accepted gameplay changes are pushed as `57cac8a50`;
normal title/file-select startup with separate persistent saves is available via
[PLAYTHROUGH.md](PLAYTHROUGH.md). New timer corrections keep burning sticks,
fishing recovery, shock/body burn, Hover Boots countdown and door waits on world
opportunities, while movement/input stays at Player rate. Intermediate disabled
input now follows the original outer policy. Footstep extinguishing events are
retained across the world boundary without changing other flag consumers.
The focused20/120 status pair and existing next-frame control case pass; see
TESTING.md for exact coverage, omitted mechanics and executable identity.

Next: ordinary human progression. Prioritize observed problems in enemy damage/
recovery, doors/keys, ledges/platforms and item transitions, using focused20-Hz
comparisons as necessary. Do not start another broad validation campaign.
Source review confirms grotto beehives deliberately shake rather than break for
normal/elemental arrows and slingshot seeds. Explosives, boomerang, hookshot and
sword-slash bits use the break path. That does not establish whether the user's
reported complete lack of hit/wobble was another defect; revisit if observed.

## Current human acceptance and next qualification

2026-10-19: the user confirmed dialogue closure, first-person hand/bow motion, and held-arrow alignment. The accepted executable is SHA256 625659423a25e8351daecf654054bb9169ba3c903b3fc1d605f5a2002495f808. The earlier aiming visual failures below remain historical evidence. Next: checkpoint the accumulated work, then a short real-combat/ranged-interaction session; specifically revisit the reported unresponsive grotto beehive.

The user accepted the corrected spin release timing after the leading-edge fix:
same intended timing, finer release precision. Aiming and surface swimming are
also human accepted. Earlier pending-acceptance statements below are historical.

Next prioritize real enemy combat/recovery, then vines/ledges and moving-world
coupling, then item interactions and progression transitions. Use representative
mechanics rather than every actor/item combination. For timing-sensitive actions,
check just before and at eligibility, cancellation and recovery, not only an
interior successful input. Use120 for the ordinary play session and20 for targeted
comparisons; retain compact fixtures only for demonstrated regressions.

## Adult spin timing repair (2026-10-16)

Leading-edge correction now validated (`build/spin-boundary-02`): preserve the
initial legacy0.02 charge gain, then integrate subsequent gains at Player rate.
The earlier fractional initial gain introduced a phase offset. When eligibility
is reached with B already released, execute the spin that same Player step.
No new state, input queue delay, lowered threshold or world actor retiming.
Six focused cases pass: release atq96 cancels at20/120 (700ms held); release atq97
spins at20/120 (708.33ms held), with high-rate execution onq97 itself and canonical
execution onq108. Both consume zero magic. Existing interior release and full
charge120 cases pass; wind-up remains150ms at both rates. Raw charge thresholds
now resolve atq97/112/322 versus canonicalq102/114/324; the world-owned charge
effect still samples on the original world opportunities. Source/engine checks
do not claim measured controller-to-display latency or human acceptance.
Final executable SHA256:
`f874abb17dca346c302a9a830d6ae940f5df2f5aa425fb1f25bab0b96835dbfa`.
Local adult human session: `build/human-spin-boundary-02`. No new native crash.
Keep `spin-boundary-01` below as the pre-fix failure evidence.

Pre-fix human report and focused boundary check: the earliest non-magical
release is STILL too late at120. The previous successful early fixture tested
inside the window, not its leading edge. On the same executable, B down atq12
and release atq97 (708.33ms hold) produces a non-magical spin at20 (executedq108,
charge0.14, magic unchanged) but cancels to idle at120 (q97, charge0.08667).
At20 the release is consumed atq102 after that tick's charge increment reaches
0.12; the ready-state branch wins before release is handled on the next tick.
At120 release is seen immediately while fractional charge is below0.1, so the
cancel branch wins. This was an eligibility/timing mismatch, not slow
input acquisition. `build/spin-boundary-01` preserves the paired receipts and
graceful semantic failure; no crash, rebuild or gameplay edit was part of this
confirmation. Inputs: `.tmp/mechanic-fixtures/spin-boundary-{20,120}.json`.

Magic-cost source audit: charge actor params0x200 request2 units once using
MAGIC_CONSUME_WAIT_PREVIEW at charge>=0.1. Magical release sets consumption;
crossing0.85 selects larger radius/damage without another request or larger cost.
The two magical stages therefore have the same cost. Ordinary non-magical
release<=0.15 kills the charge actor without consuming the reservation.

Human swimming/aiming acceptance is retained. Spin charge was adding0.02 per
Player update rather than per50ms, making120-Hz charging six times too fast.
`func_80844E3C` now scales only this increment; B-release checks stay Player-rate.
`Player_Action_80844E68` uses the existing legacy-completion opportunity for its
wind-up while the animation pose continues to advance at Player cadence.
The original20-Hz branches and EnMThunder/magic/effect cadence are unchanged.

Focused adult standing full-charge/early-release fixtures passed at20 and120.
Final wind-up is150ms at both rates. From fixture start, charge-ready occurs
at850ms at both rates; magical/full thresholds occur at950/2700ms at20 and
975/2725ms at120 (continuous fractional charge versus discrete20-Hz increments).
Early release at900ms executes a spin at that exact Player opportunity with
charge0.14/0.12333 and magic48 unchanged. The tight non-magical window is restored.
Moving charge uses the same corrected increment, but moving/quick-spin variants
were not separately tested. Human acceptance of this spin build is pending.

Evidence: `build/spin-charge-03` (four focused runs), `spin-charge-04` (only the
two affected120-Hz cases after matching wind-up completion), and the latter's
`timing-comparison.json`. `spin-charge-02` retains the graceful observer failure:
EnMThunder clears the request flag within a world tick, so observing the actual
spin action is necessary. `spin-charge-01` was a preflight fixture error (sword
setup requires100 ticks); no engine ran. No native crash. No broad regressions.
Final executable SHA256:
`7374dec368a1562e3c5208403feb3d55b65a20680d098b1827fa101d41d3e6b4`.
Source base788b5d9b1 plus working changes; submodule pins unchanged.
Ordinary adult Zora River startup/session: `build/human-spin-01`, Player120/VSync,
profiling off. This is a local human-test build, not a published checkpoint.

## Aim and surface-swimming repair (2026-10-16)

Human follow-up accepted swimming and aiming. The next reported defect is adult
spin-attack wind-up/charge timing, including loss of the non-magical release window.

Weapon/unarmed aiming increments now use elapsed Player time with signed-angle
remainders; pitch target filtering retains its target and scales its response.
Settled first-person camera views update on each Player opportunity, with the
world-owned late draw handshake preserved. Bow20/120 turn by exactly1430 pitch
units and -1430 yaw units over the same500ms constant-stick interval. Bow120 and
slingshot120 both pass the first-due-step aim AND prepared-camera assertion.
The existing next-frame-motion120 fixture also passes.

High-rate motion now composes the original current/conveyor push velocity before
integration. Swim acceleration/drag, buoyancy and current convergence use elapsed
time. One real Zora River run at20 and120 each records63 water/current ticks;
surface-swim speed differs by at most0.01358 over the initial30world ticks. The
route subsequently leaves water and reenters: those shore/reentry trajectories
differ and are not exact whole-route equivalence evidence. Diving and all water
transitions are not claimed qualified. Human feel remains to be checked.

Evidence: `build/swim-01`, canonical bow in `build/aim-swim-01`, final aim/movement
checks in `build/aim-swim-04`; native animation helper53/53. Retained graceful
failures in `aim-swim-01` (held weapon base-animation observer), `aim-swim-02`
(draw handshake) and `aim-swim-03` (normal-camera dynamic-geometry exclusion
incorrectly applied to settled aim). No native crash occurred. No broad matrix
or successful repetitions were run. Success receipts are compact.

Final local executable SHA256:
`01490b0b36df2ec1ab666775a70420f74d4bd61c072031e607da1411a3067c30`.
Source base remains788b5d9b1 plus the working changes; dependency pins unchanged.
`build/human-aim-swim-01` is the separate ordinary adult Zora River session,
Player120/VSync enabled, profiling off, normal bow equipped on C-left.

## Mounted pilot and remaining gameplay scope (2026-10-15)

The user subsequently accepted the direct mounted ride and carrot fix: "Works
fine." New live reports are over-sensitive aiming at Player120 and weak swimming
with missing current. Those are the current focused implementation/validation work.

Human title-screen inspection accepted the rider vibration fix. Epona's skin
vertices still update at world20; matrix interpolation does not interpolate that
deformation. The skin and horse drawing sources match the local upstream ref.
Horse steering/movement remain world20, not next-frame Player120 control. First
judge an ordinary ride; high-rate horse authority is not a completion prerequisite
unless the resulting controls warrant that conversion. Smooth horse presentation
remains outstanding.

The human ride exposed lost carrot presses: intermediate Player input consumed A
before EnHorse_CheckBoost could read it at world20. The mounted input policy now
retains A until the world opportunity, alongside the existing dialogue policy.
It does not change horse timers, steering cadence, or on-foot input ownership.
The accumulator resets through its existing world-consumption/scene lifecycle.

`gDeveloperTools.NativeHorseRideStart` is a one-shot debug scene-select entry:
temporary adult debug save, Hyrule Field, existing AREG(6) mounted-entry path.
Normal dismount clears that register. No personal save is loaded. The matching
fixture uses `mounted_horse`; the original ranch-escape entrance was rejected as
a test setup because its cutscene disables carrots at both Player20 and120.
Retain `build/horse-carrot-01`, `horse-carrot-02`, and
`horse-carrot-canonical-01` as graceful observer/setup failures, not crashes.

Remaining work is compatibility qualification, not a promise to retime every
world actor. Prioritize these representative play sessions:

1. Horse riding/carrot/dismount; close vine-speed and blocking-NPC vibration reports.
2. Real enemy sword/shield/damage/recoil and invulnerability, plus dodge/jump/landing.
3. Doors, room/scene transitions, get-item/carry/throw and ranged-item controls.
4. Swimming/diving, ladders/ledges, pushing and one moving-platform interaction.
5. A short normal progression route exercising the above together, save/reload,
   pause and cutscene return to control.

These are unqualified areas, not claims that each is broken. Reuse existing sign,
talk, bomb, crawl and next-frame movement evidence; repeat only affected cases.
Keep the accepted pacing, world20 behavior and parked black-wedge decision.
The mod is a playable experimental build, not yet generally gameplay-qualified.

Mounted-input validation: build13 succeeded; executable SHA256
`e76a80c8ba053412bdbea4b4f4074affd4f0fdef6a04d5385965e314fee6c935`.
`build/horse-carrot-05` passed one30-tick mounted run each at Player120 and20:
one world A edge and exactly one carrot consumed. The120 press starts at quantum61,
between world boundaries. Native animation/input checks passed53/53. Earlier
`horse-carrot-03/04` retain failed setup evidence: the ordinary debug save
did not set EPONA_OBTAINED. The final setup explicitly sets it immediately before
Actor_InitPlayerHorse. Do not infer that register-reset timing alone explained
those failures. No native crash occurred. `build/human-epona-02` is the direct
mounted human session; horse controls remain world20 and human acceptance is pending.

## Live mechanics follow-up (in progress, 2026-10-15)

The human pilot reported fast vine climbing, vibration against the blocking
Kokiri and on the title-screen horse, a crawlspace softlock, and an overhead bomb
that did not hurt Link. Controlled, ridden Epona must also run at Player cadence;
that requires a separate coupled horse/rider conversion, not just smooth drawing.
Do not interpret the earlier collider checks as qualification of these mechanics.

Current build: `bf2a52d4d6219f291dd6daeecc87467ffa875b7d97e1b16132abff87997810ae`,
from `f8e253081` plus the current mechanics and presentation changes;
dependencies unchanged. Build09 succeeded. This follow-up is not yet a completed
or pushed checkpoint. NPC constraint, vine-cycle and rider-presentation changes
remain under qualification.

- Human session `build/human-mechanics-01` confirmed that an overhead bomb
  reduced health224 to216, entered the damaged action and applied invincibility.
  The user confirmed it works now and did not before. The earlier failure's cause
  is **not established**. Retain `bomb-receipt.json`; no more bomb variants are
  warranted. Earlier automated successes did not explain the human failure.
- The same live session confirmed the camera correction alone did not fix
  crawling. Link froze at z1097.299 while the game continued rendering. The matching
  `mechanics-05/crawl` failure shows completed entry animation, speed1.8, and both
  ceiling and floor contact. The floor solver restores previous XZ when the
  shortened high-rate downward step causes a false ceiling/floor squeeze.
- High-rate grounded crawling now preserves the canonical ceiling-clearance
  envelope using the existing 50-ms velocity/gravity units. Movement, walls,
  ceiling and floor queries still execute at Player cadence. Canonical20 takes
  the unchanged branch. The ordinary world camera dispatcher owns the crawl-mode
  handshake. Shared animation SFX markers beyond the active clip return no event,
  instead of marking a valid action as a scheduler failure.
- `build/mechanics-06/crawl` passed one260-world-tick Player120 run:193 crawling
  ticks,125 crawl-camera ticks, exit at z1450.200 and normal idle control. The
  matching canonical20 route previously passed in `mechanics-04/traversal`.
  `human-mechanics-02` also records a human-controlled trip through and back,
  with exits at z1459.429 and z1037.230. The user confirmed: "Crawling works."
  The separately reported single-frame crawl-exit flash also occurs with Player120
  disabled at render120, absent at20FPS. Build08's camera change did not resolve
  the flash. The live recording shows the missing reset at the start of the exit
  shot. Build09 moves the reset to that exact first `Camera_Demo9` evaluation;
  Player20 crawl still matches all261 saved boundaries. Letterbox fills/scissors
  animate at render cadence. In `human-mechanics-04`, the user confirmed the
  crawl-exit flash is fixed. Targeting-bar acceptance was not separately stated.
  No further capture runs, per user direction.

### Parked cosmetic issue: black wedge in the crawlspace

The user reports a small black wedge along the top of the far opening while
crawling, disappearing near the end. It also occurs at original20FPS. The two
screenshots supplied on2026-10-15 show the affected and clear views. The precise
cause is not established; inspection found an invisible room transition and
overlapping scene backdrop geometry, but neither establishes the offending draw.
No application change was made for this issue. At the user's request, defer it
and move to Epona. Do not restart capture or geometry investigations by default.

Pause checkpoint: the user is switching to High effort. Epona implementation has
not started. Leave the currently running human test session alone; resume with
the coupled horse/rider work after the user continues.

Next, per the user: qualify Epona with coupled horse/rider authority, then play
the game and address concrete failures as they appear. Do not substitute a broad
preemptive actor campaign for actual progression. Existing vine/NPC/rider changes
remain tracked above until qualified; horseback presentation is not ridden120-Hz
gameplay support.

Failed approach fixtures, observer assumptions and scheduler diagnostics remain
in `build/mechanics-01` through `mechanics-05`. No native crash occurred. Successful
cases use compact receipts; the bounded failed-run rings are retained. No broad
matrix, repetition or new pacing campaign was run. Do not repeat bomb tests merely
because the build changes. Finish the remaining reported mechanics with focused
checks and live motion review.

The user accepted pacing on 2026-10-15 and authorized ordinary world collision
and NPC interaction. The accepted source checkpoint is `7cd77f92d`, with
libultraship `5ad2ca80`. Preserve input cadence, movement and presentation pacing.

## First implementation slice

- Accept a talk-button edge at the Player step using the most recent world-owned
  offer. NPC offer production and message progression remain world20. Retain
  message-owned button edges until the world consumes them; do not defer rolling
  or other ordinary Player input to world20.
- Register the Player body and shield with the existing world collision pass.
  Geometry continues to follow Player updates; collision response, damage and
  reciprocal object displacement execute once per world opportunity.
- Retain every produced sword sweep within the current 50-ms transaction and
  deliver it through the engine's existing collider-shape dispatch at the next
  collision opportunity. Keep the already-qualified sign adapter separate.
  Preserve source damage data through target consumption, avoid duplicate hits
  from substeps, and cancel pending work on Player/scene invalidation.
- Keep canonical20 arithmetic and ordering unchanged. No world actor, NPC, AI,
  global collision pass or dynamic-platform update runs six times faster.

## Focused acceptance

Use compact online receipts: a real NPC conversation with an intermediate A edge
and message advancement; body blocking; a non-sign cuttable object; ordinary
incoming damage/shield coverage; the existing sign and next-frame fixtures;
canonical idle/slash regression. Add cheap native shape/lifetime checks where
they directly cover the generic sword path. Default one run per case. No pacing
campaign, broad configuration matrix or successful per-substep trace archive.

General collider participation is not proof of every enemy-specific attack,
moving-platform attachment, climbing, swimming, projectile or item action.
Record the tested interactions and remaining gaps separately at handoff.

## Implemented ownership

`Player_ActionHandler_Talk` accepts an explicit edge against the latest NPC offer
on a Player step. `PlayerTemporal_WorldInputButtons` and the existing PadMgr edge
accumulator retain dialogue-owned buttons until the message dispatcher consumes
them. Ordinary movement and action input still reaches the next Player step.

`PlayerContact_RegisterBody` and `PlayerContact_RegisterShield` update membership
without adding duplicate entries or repeatedly resetting collision flags. Their
geometry follows Player cadence; the original world collision pass owns overlap
displacement, incoming damage and shield responses. Inactive membership is removed.

`PlayerContact::Detect` captures produced sword sweeps, including damage fields.
`ConsumeWorldSweeps` uses the original four AC shape dispatchers after world AT,
before damage accumulation and actor updates. It filters stale actors and already
delivered attack/opportunity/target-collider identities. AC hit source pointers
refer to stable per-target storage, retained through culling until replacement or
target destruction. Scene reset clears that storage. Player invalidation cancels
pending sweeps. The existing sign adapter retains its producing-animation data;
the nearest sign distance and reservation identity arbitrate competing objects.
This does not add intermediate world collision passes or retime world actors.

## Validation on 2026-10-15

Incremental Release builds succeeded. Final executable SHA256:
`1a8221aa455ced19ad6bb9469ab9329ec9b6c196ca089f653319202f7085ddc7`.
Built from `7cd77f92dc83a66f4a696a7558a8710ae95305b2` plus this interaction
change. Dependencies remain libultraship
`5ad2ca80a138df8bcded4edde0b674fd39663a34` and Torch
`2ab12fe9660aec04e02ee89fe81baed304a1a1d6`. The local corpus receipts contain
the exact tracked diff, fixture, executable and asset hashes; assets are not committed.

| Focused gate | Result | Local evidence below `build/` |
| --- | --- | --- |
| Native shape/lifetime/response checks | 33/33; includes both sign/generic nearest-contact directions, reciprocal mass, incoming damage and shield block | `player-world-03/contacts/player-world-contact-checks` |
| Real grass at Player120 | One bridged contact, one cut, target destroyed; 45 world ticks | `player-world-03/contacts/player-world-grass-120` |
| Existing Player120 sign | Pass after final nearest-contact correction | `player-world-03/contacts/player-contact-sign-120` |
| Asset-free animation/input ownership | 53/53 | `player-world-03/input` |
| Real Kokiri conversation at Player120 | A at quantum61 starts Player response at61; textbox opens62; all three subsequent dialogue edges reach world; NPC updates100 times in100 world ticks | `player-world-02/focused/player-world-npc-120` |
| Player120 body contact | Nine observed overlaps; NPC updates55 times in55 world ticks | `player-world-01/objects/player-world-body-120` |
| Next-frame movement | All four sampled input changes consumed, moved pose and rendered at their first due submission | `player-world-02/regression/player-next-frame-motion-120` |
| Canonical idle / slash | Exact 41 / 71 common-boundary comparisons against retained references | `player-world-02/canonical-idle`, `player-world-02/canonical-slash` |

The final build changed generic-contact nearest arbitration and added its native
checks. Only contact checks, sign and grass were rerun for that change. Earlier
NPC, body, movement and canonical results cover unchanged relevant paths; they
are not represented as executions of the final binary. Each case ran once per
relevant implementation change. No configuration matrix or pacing campaign ran.
The existing general Python/tooling, HUD corpus and exhaustive enemy/item suites
were not rerun: this pass did not change that tooling or HUD logic.

The initial NPC run exited gracefully with a test-observer failure, retained at
`player-world-01/npc`: it incorrectly equated textbox opening with the first
Player response. The recorded Player action accepted A at61 and the textbox
opened at62. The observer now records both separately and still requires Player
acceptance at61. No gameplay timing was changed to satisfy that observation.
No new native crash occurred. All successful evidence is compact online receipts
and ordinary runtime logs; no new full per-substep snapshot/trace archive.
Total retained pass evidence is about 40 MiB, including the failed observer run.
`player-world-03/retention.json` records the retained directories and empty
deletion manifest; the corpus runner released its staged runtime archives.

Ordinary startup of the final binary succeeded with the configured child Kokiri
scene selector, Player120 and VSync enabled. The menu selection was sent once;
subsequent live Player animation/frame counters confirm gameplay is running,
with the game foregrounded. Local session receipt:
`human-world-interactions-01/session.json`. It is left open for the user. The
automated interaction results above are not human acceptance of this new build.
Use `.tmp/Play-Shipwright.cmd` for later launches with the correct asset directory;
the direct executable remains `x64/Release/soh.exe`.

## Next gameplay work

Exercise this build in ordinary Kokiri gameplay, including NPC conversation,
blocking against actors and grass/sign cuts. The native incoming-damage/shield
checks establish collision delivery, not every enemy's attack reaction. Next
qualify one real enemy attack and its damage/recoil/invulnerability timing, then
item/door interactions and moving-platform coupling as separate focused cases.
Keep accepted pacing and next-frame Player controls intact. No claim is made
that every enemy, projectile, item, climbing or swimming action is converted.

### Mount-button report (2026-10-16)
Human reports missed A presses when mounting Epona at Player120. Prior horse fixtures start mounted and do not qualify mount acceptance. Opt-in gDeveloperTools.PlayerActionInputLog records only A-edge Player state and mount-handler entry, in the ordinary runtime log; no gameplay behavior is changed. Root cause and repair remain pending live reproduction.

The live diagnostic confirmed missed intermediate A presses reached the mount handler with a null rideActor; successful world presses had the horse offer. Epona is ACTORCAT_BG and offers before Player, whose legacy end-of-update clear removes the offer before intermediate steps. PlayerTemporal now captures the incoming offer per world transaction, borrows it only during intermediate Player updates, and restores the empty producer slot unless mounting succeeds. Actor destruction, scope reset and the next world transaction invalidate it. Horse producers remain world20 and canonical20 is unchanged. Focused verification passed once each in build/mount-offer-01: horse-mount-120 (same-step q61 acceptance), horse-mount-20, and horse-carrot-120. Live pre-fix failure log retained in build/mount-input-failure/live-before.log. Human acceptance of the fix remains pending.

### First-person aiming presentation (2026-10-17)
The candidate fix records first-person model matrices during the existing draw and rebases their interpolated presentation from the interpolated camera onto the current prepared Player camera. Gameplay pose, aim sensitivity and input cadence are unchanged. Matrix references are cleared at the next draw recording. Build succeeded; bow120, slingshot120 and bow20 passed once each in build/aim-hands-01 using compact output. These fixtures check aiming behavior, not visible hand smoothness; human motion acceptance remains pending.

### Dialogue closure and stationary aim endpoint (2026-10-17)
A reported Player120 no-empty-bottle message reopened after dismissal. Source
inspection found that Player_Action_Talk could leave TEXT_STATE_CLOSING on an
intermediate step while its world-produced automatic offer was still retained.
Closing now releases the talk action only at the ordinary world opportunity;
intermediate input also honors the original post-textbox A/B/C-Up cooldown.
No world producer or message timer runs at Player cadence. The focused
`talk-close-120` fixture injects a normal textbox close with an automatic offer,
requires intermediate talk ownership to remain intact, and requires no open
message at completion. This is a handoff regression check, not a complete Poe quest test.

The remaining stationary aiming tremor exposed a candidate precision issue:
first-person rebasing used a floating camera on interpolated frames but decoded
16.16 camera data at the empty-map endpoint. The view recorder now retains the
original floating camera for that same transaction; it is invalidated at the
next recording. The direct first-person-view check compares identical stationary
endpoints and includes the old quantized path as a sensitivity control. Gameplay
pose and aim input are unchanged. Validation results and visual acceptance pending.

Validation completed 2026-10-19: build exit0; first-person endpoint test exit0 (including quantization sensitivity control); talk-close-120 passed with five closing substeps and no reopen; aim-bow-120 and aim-bow-20 passed once each. Compact receipts: build/aim-talk-final-01. Retained initial observer failure: build/talk-close-01 (animation-hold expectation corrected, gameplay not changed to satisfy it). Executable SHA256 c036bd18ad59a94cbd6110bcb0674bd3d0b8a61a920215c4c0d5bce49d454462. Actual Poe interaction and residual hand tremor await human confirmation.

### First-person motion follow-up (2026-10-19)
Human acceptance: dialogue is fixed. Human rejection: the prior hand rebase and
precision adjustment did not fix tremor; idle breathing shakes subtly and aim
movement shakes severely. Prior passing endpoint/aim-speed checks did not prove
motion quality. Those receipts remain historical failures of visual acceptance.

The replacement captures each first-person limb matrix in the world draw's
camera space, interpolates those local endpoints, then transforms the result
through the inverse current responsive camera. It no longer multiplies separately
interpolated world-model and camera matrices. Previous data contains only value
matrices, never previous frame-arena pointers. Owner, weapon profile, matrix
count and camera-cut epoch must match to blend; a missing first-person draw
breaks continuity. Canonical20 is untouched; animation and input gameplay are
unchanged. Focused bow/slingshot runs now check displayed camera-space origin
and orientation probes online, emitting aggregate counts/error only. The direct
helper check covers changing camera transforms plus local breathing movement at
all six subframes. Visual acceptance of this replacement is pending.

Camera-space replacement validation: build and direct helper check passed. One run each bow120, slingshot120, bow20 passed (build/aim-local-01). Each high-rate run checked 4,218 displayed matrices over 222 rendered frames, 216 with local interpolation; maximum view-space probe error was 0.000367 world units (bow) and 0.000185 (slingshot). These are mathematical presentation checks, not human visual acceptance. Build SHA256 55f842ada1c19293fdb8d921332a19ab9d2dad12a5f9b8e10e121a0471a93942. No native crash. Dialogue fix remains human accepted.

### Held arrow presentation (2026-10-19)
Human accepted first-person hand/bow motion. The remaining held arrow lag is a
separate EnArrow draw still using world interpolation. The held arrow skeleton
now records a separate camera-space attachment stream only when parent and
Player heldActor agree and that Player recorded first-person hands this frame.
It uses the same presentation alpha/current camera as the bow. Attachment
appearance/disappearance cannot invalidate hand interpolation; release returns
the arrow to its ordinary world draw. Arrow update, collision and release
semantics are unchanged. Previous attachment transforms are values only and
match actor identity; frame-arena pointers remain confined to the current draw.
Focused aim-bow-release-120 requires attached matrices to be observed and then
absent after release. Direct helper checks cover bow/arrow relative motion and
attachment removal. Validation and human acceptance pending.

Held-arrow validation: build and direct hand/arrow helper passed; aim-bow-release-120, aim-slingshot-120 and aim-bow-20 each passed once in build/aim-arrow-01. Bow test observed 198 attachment matrix checks, zero remaining at completion after release, and maximum view-space probe error 0.000367. Source build warning was an explicit size conversion in diagnostics, corrected; no native crash. SHA256 625659423a25e8351daecf654054bb9169ba3c903b3fc1d605f5a2002495f808. Human visual acceptance pending.

2026-10-29 checkpoint review: accepted executable hash reverified unchanged. 152 Python tooling/math tests passed. Presentation lifetime review found no blocker; held-arrow release still follows world20 draw ownership, not substep-immediate projectile drawing. Beehive source (ObjComb_Wait) deliberately makes normal/elemental arrows and seeds shake rather than break; no-hit/no-wobble remains unverified. Normal playthrough is the user-selected next mode.
