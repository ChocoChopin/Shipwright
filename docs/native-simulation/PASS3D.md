# Pass 3D: canonical Player pose/contact extraction

Status: complete under the focused validation scope below. Authoritative gameplay
remains 20 Hz. No human gameplay acceptance or high-rate support is claimed.
Starting checkpoint: `ce54b58bb94d5fc695494fb70a6c6aa1e98bd3d7`.
Dependency pins remain libultraship `9280b17ddc504da6630892a46440e86be41ac571`
and torch `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`.

## Ownership

| Native file | Changed boundary |
|---|---|
| `soh/include/player_pose.h` | Packet schema and bounded C interfaces |
| `soh/src/code/z_player_lib.c` | `Player_PoseProfileRejection`, `Player_IsPoseProfileAdmitted`, read-only `Player_PoseBoundsOutside` / `Player_PoseVerticesOutside` / `Player_PoseColliderOutside`, `Player_AdvancePoseLimb`, `Player_AdvancePoseContactsLegacy`, `Player_DrawPosePresentation`, late-slot branch in `Player_DrawImpl`, observational `Player_CopyPoseStatics` |
| `soh/src/overlays/actors/ovl_player_actor/z_player.c` | Read-only `Player_IsPoseActionAdmitted`; action/update bodies unchanged |
| `soh/soh/Enhancements/customequipment.cpp` | `CustomEquipment_GetPlayerPoseHook`, `CustomEquipment_PlayerPoseResourceRejection`; retain the built-in hook ID in `RegisterCustomEquipment` |
| `soh/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp` | Read-only `GameInteractor_PlayerPoseHookRejection` |
| `soh/soh/NativeSimulationTest.cpp` | Player live-state closure, negative admission, direct helper verification/control and coverage observation |
| `soh/soh/NativeSimulationTest.h` | `NativeSimTest_PlayerPoseAdmission` observation declaration |

Verification tooling changes are limited to `analyze_purity.py`, `run_corpus.py`,
`validate_purity_control.py`, and their two affected test modules. Byte-identical
traces reuse a fully validated parse; malformed identical traces still reject.
The general skeleton walker, renderer dependency, timing primitives and Player
update/action bodies have no changes.

`Player_DrawImpl` admits the complete profile before entering a Player-only
traversal at the old skeleton slot. `Player_AdvancePoseContactsLegacy` owns the
original override/post arithmetic, including static-floor IK live joint writes,
body/hand/feet/focus, attachment matrices, weapon history and warm-up, swept
quads, shield AC-before-AT registration, combo extension and once-only blur.
Player update, animation queue, camera, outer Actor_Draw, other actors, UI and
audio retain their existing positions. Pose N still feeds collision N+1 and
Player update N still consumes pose N-1.

The Player-specific traversal preserves original-list post callbacks and body
cursor semantics separately from selected meshes. The general skeleton walker
is unchanged. Unsupported profiles take that complete original walker.

`PlayerPosePacket` is zero-initialized current-frame graphics-arena storage:
evaluated matrices, selected meshes, ordered matrix indices, root marker, and
scene/player/frame identity. It owns no gameplay state. It is used synchronously
by `Player_DrawPosePresentation`; rendering references remain valid through the
same transaction's command submission. Nothing retains the packet across a
frame or scene transition. The pure emitter reads only the packet and writes
caller-owned command storage; it makes no gameplay callbacks or matrix queries.

## Validation plan and resource limits

Admission preflight uses the canonical ordinary-action function identities, child
Kokiri sword/Deku shield/Kokiri tunic and boots, default equipment table pointers,
unmodified child geometry and the two reference scenes. It rejects unsupported
flags, attachments, effects, camera modes, nearby dynamic surfaces and unknown
pose hooks. The built-in equipment hook is admitted only without replacement
geometry; the exact TimeSaver dispatcher is allowed because both pose flags take
its no-op default case. Neither exemption applies to an arbitrary handler.
The debug-save profile's child bracelet is captured in the packet. Shielding's
`itemAction == -1` is admitted only with the shielding flag; the ordinary boots'
grounded hover timer is not an active hover-boot profile.

The exact predicate is `Player_PoseProfileRejection` in `z_player_lib.c`, with
the ordinary action identities supplied by `Player_IsPoseActionAdmitted`.
Only idle, ordinary move/turn, target, shield and sword actions are accepted;
melee animation indices are 0/4/6, model groups default/sword-and-shield, hands
open/sword and open/shield, and camera modes normal/target/friendly-target/still.
The state-flag allowlists are explicit in that predicate. Equipment pointers
must match the canonical child tables. The current animation, 82 child model
resources, five equipment replacements and both admitted scene collision
resources must have no custom/alternate override. Active dynamic geometry whose
horizontal bounds overlap the Player's conservative 200-unit exclusion region
rejects even when vertically separated. The local root translation is bounded
to 10,000 model units on each axis, with actor model-space vertical offset
bounded to 1,000 units.

Pause, transitions, special actions, carrying/get-item, first person, crawling,
reflection, alternate weapons/equipment, special effects, changed model scale,
custom geometry and unknown pose hooks retain the entire legacy skeleton path.
The draw call also checks the exact default override/post callbacks and live
skeleton/joint pointers before choosing extraction. No rejection occurs after
the new authoritative traversal starts. These checks do not depend on replay.

Existing sword callbacks still select their active window at the old point,
seed the first active endpoint sample without a damaging quad, suppress an
unchanged sample, and register the two changed sweeps in their old order.
`unk_845` retains its mixed combo/extension meaning and original increment
opportunities. The original shield helper retains material/geometry and AC
before AT. Original thigh/shin/foot IK writes remain live, ordered writes;
they are not replaced by packet-only joints. Blur type/vertices and admitted
hook ingress execute once in authority; repeated packet emission cannot reach
them. Outer face/material/lighting setup, actor culling, shadow and audio slots
remain outside this bounded helper.

Negative cases start from a demonstrably admitted copied Player/PlayState; the
copy rebases its aligned joint buffer before modifying one condition. Unknown
limb-hook registration is also tested without executing that handler. Purity
observes the full Player and PlayState, relevant actors/signs, effect/blur stores,
pose scratch globals, hook invocation counts, semantic RNG/audio/input state and
event sequence. The normal emission and two scratch emissions must leave those
unchanged; no state is restored after an emission.

Use the ten accepted Player fixtures at canonical 20 Hz with three repetitions
and strict old-reference/phase comparison. Direct helper verification uses the
normal call plus two independent scratch emissions, packet bytes, ordered
commands and live gameplay closure. Representative negative admission cases and
a graceful mutation control must reject. Follow with the requested focused six
Player cases at presentation 60/120 (canonical runs cover presentation 20),
trace-disabled representatives, a bounded original/HUD regression subset,
tooling/CLI checks and ordinary startup. Preserve all failures. Do not rerun the
historically omitted 48 presentation variants or 39 startup stress runs.

Any new native crash stops the campaign and requires user direction. All
authoritative gameplay remains 20 Hz. Later timing work is not part of this pass.

## Retained implementation findings

`build/pass3d-01/idle` matched the old canonical outputs through legacy fallback,
but failed the independent non-vacuous Player purity audit. It is not extraction
acceptance. The strengthened runner now stops immediately on incomplete measured
Player extraction. `build/pass3d-02/idle` retained the overbroad hover-timer
rejection; `build/pass3d-03/player-canonical` retained the built-in global-hook
rejection. These are admission failures, not native crashes. A resource-table
identifier compilation failure is retained in `build/pass3d-01/build-02`.

The first two successful full builds completed the unchanged YAML copies.
Subsequent incremental builds use process-local `PostBuildEventUseInBuild=false`
with `baseline.py build --jobs 4` to omit only the repeated MSBuild post-build
copy event. The source YAML and runtime archives are unchanged; dependency pins
and compilation/link checks remain in force. No machine-wide setting changes.

## First validated implementation checkpoint

`build/pass3d-06/player-canonical` passes all ten accepted Player fixtures, three
fresh processes each, against the unchanged Pass 3C reference. All 2,580 measured
poses take the extracted path. Direct CPU purity passes for 3,756 packets
including setup, with 7,512 additional emissions and 13 negative admission cases
per process. Executable SHA-256:
`533113ab285bb6bf5fff0af4bce60f06319ad40ca076ed08660239a6f12d9c29`.
`build/pass3d-06/runtime-identity.json` binds the native source bytes and build.
At this historical checkpoint, review corrections and focused gates were pending.

The subsequent source review explicitly excludes non-normal game mode and
airborne poses, adds an airborne negative admission case (14 total), and restores
the borrowed `flexLimbOverrideMTX` pointer after the synchronous traversal. The
legacy arithmetic/callback ordering is unchanged. Second-checkpoint canonical
checks cover the six presentation cases so that their FPS comparisons use the
same executable on both sides. At that checkpoint four other canonical cases
reused the initial complete reference. The final contact-admission correction
subsequently reran all ten cases, as recorded below.

Strict analysis of that checkpoint preserves 2,451 joint mutations, 24 draw-owned
combo mutations, 5,466 registrations, 24 raw sign contacts and six target
responses. Four contacts in a sign run remain one cut response, not four cuts.
The combo ordinal sequence is still 1/2/3 and Z-sign still acquires the real sign
with friendly-target camera mode. The first active sword sample, no-motion
resets, prior-pose/contact latency and shield ordering pass the strict analyzer.

The reviewed executable is
`ed9ae33e5f869e23f2a7ee67aa7167c07a24ecd91b5c3096a1714f6e1bcf823a`,
bound by `build/pass3d-07/runtime-identity.json`. Fresh tooling tests pass 131/131.
The 24 texture-copy, 31 TLUT and 20 coverage-helper checks are reused only after
rechecking every source/header input hash (`build/pass3d-07/helper-reuse.json`).
No dependency or renderer source changed in Pass 3D.

The six reviewed-build canonical cases (idle, slash, combo, shield, sign,
Z-sign) pass all 18 fresh runs with exact snapshots and full traces against the
first checkpoint. Every measured pose is extracted and all 14 admission
negatives reject. Remaining presentation, broad regression and startup gates
were still pending at this second source checkpoint.

## Contact-admission review correction

The second checkpoint subsequently passed all 39 focused presentation runs:
six cases at 60/120 FPS, three repetitions each, plus trace-disabled slash.
The final source review nevertheless found that the profile must explicitly
exclude hostile/projectile ownership and unadmitted contact partners, rather
than relying only on Player action/equipment/damage flags. This was a scope
boundary defect, not a measured canonical mismatch or native crash.

The corrected predicate rejects owned child actors, enemy/boss presence,
attention-enabled hostile actors and Player projectile actor types. The normal
Navi parent link is exempt from the generic Player-owned-actor rejection.
It rejects foreign AT geometry overlapping the bounded Player pose/contact
region and overlapping non-sign AC geometry that accepts Player damage.
Read-only cylinder, sphere, triangle and quad bounds establish separation;
unknown shapes fail closed. The region includes previous position and retained
active sword endpoints so long retained sweeps cannot escape the check. This
preflight never resolves collisions or changes collision scheduling. Ordinary
distant scene hazards remain outside the region; nearby interactions use the
whole legacy path. Hostile shield block/parry remains unvalidated and unadmitted.

Six additional negative cases cover owned child, hostile scene, projectile,
foreign attack, unadmitted target and unknown geometry, for 20 total. Copied
Player colliders and their registration pointers are rebased before proving
that each baseline copy is admitted. All tests modify copies or a temporary
hook entry; none dispatches an unsupported collider to the engine.

`build/pass3d-08/runtime-identity.json` binds this candidate, executable
`8ad4e1b26b882f6fe9b2b0292178b6bf26a5f34a75569681ba950fbe698dcdc3`.
All ten canonical cases pass three fresh runs on this build: all 2,580 measured
poses are extracted, all 20 negative admission cases reject, and snapshots/full
traces match the preserved reference exactly. The completed 39-run presentation matrix is retained for the
unchanged traversal/packet/emitter; only a focused final-build presentation
check is added. These checks and broad regressions now pass.

## Final acceptance receipt

Native source commit: `857722ae8507cb84aca9ed2d823fc4ffc187fec4`, pushed to
`ChocoChopin/Shipwright`, branch `mod/native-simulation-rates`. The final handoff
commit adds only documentation and regenerated inventory. Earlier validated
checkpoints were pushed periodically; published history was never rewritten.
`build/pass3d-08/source-binding.json` binds the exact raw build inputs to committed
source (normalizing only checkout line endings). `runtime-identity.json` and
`build/build-invocation.json` retain executable, asset and toolchain identities.
The executable SHA-256 is
`8ad4e1b26b882f6fe9b2b0292178b6bf26a5f34a75569681ba950fbe698dcdc3`.
Its configure-time startup banner still says `ce54b58`; that stale label is not
used as build provenance. Source hashes and the executable hash are authoritative.
Dependency pins at the top of this document are unchanged.
Build toolchain: CMake 4.4.3, Visual Studio 2022 Build Tools, MSVC
19.44.35228.0 (tool directory 14.44.35207), native x64 Release; vcpkg remains
`3aea538b2bb21a586502c67b00eb474fdd2e3098`. Tooling uses Python 3.12.5.

| Gate | Result and retained evidence |
|---|---|
| All ten Player canonical fixtures | 30/30 fresh runs on final executable; 2,580 measured transactions, 2,610 snapshots; `build/pass3d-08/player-canonical` |
| Strict phase/contact analysis | Pass; 2,451 joint mutations, 24 combo mutations, 5,466 registrations, 24 raw contacts and six target responses; `player_state_result.json` |
| Direct Player CPU purity | 3,756 packets including setup, 7,512 extra emissions, 142,728 ordered commands; packet bytes/live state unchanged; `purity-analysis.json` |
| Negative admission | All 20 copied unsupported profiles reject in each final canonical process; the unchanged whole legacy branch remains the fallback |
| Focused presentation | 45/45 runs, 3,495 snapshots: 39 second-checkpoint runs plus six final-build runs; details below |
| Original regression | 12/12: gravity-fall, input-short-pulse, draw-rng-keese, ocarina-memory-rng, each repeated three times; `build/pass3d-08/original-canonical` |
| HUD/message regression | 12/12: hud-warning, hud-zero, message-pages-natural, message-fade-observed, each repeated three times; `build/pass3d-08/draw-canonical` |
| HUD/message direct CPU purity | Pass for countdown and message, including visible/invisible states; `draw-canonical/purity-analysis.json` |
| Tooling | 131/131 Python tests; `build/pass3d-08/tooling-tests.log` |
| Native CLI | 43/43 pre-initialization checks; `build/pass3d-08/native-cli/native-validation.json` |
| Graceful controls | Player mutation detected, normal exit 2, `player: live state mutated`; copied-output one-bit mismatch detected at tick 17 and restored copy matches; `player-purity-control/control.json` and `build/pass3d-07/mismatch-control/negative-test.json` |
| Renderer helpers | 75 prior checks reused after all source/header hashes reverified: texture 24, TLUT 31, coverage 20; `build/pass3d-07/helper-reuse.json` |
| Inventory | Regenerated/fresh: 2,434 files, 429 actor entries, zero missing and one unmapped; `build/pass3d-08/inventory-freshness.json` |
| Ordinary startup | Pass: visible main window, scene initialized, graceful close, exit 0, no forced termination; `build/pass3d-08/startup/smoke.json` |

Final snapshots and full phase traces match the first accepted extraction
checkpoint, which matches the untouched Pass 3C reference. No goldens were
replaced. All measured Player poses take extraction, so fallback cannot hide a
failed extraction. The prior 30 initial and 18 reviewed canonical runs remain
supporting history and are not counted as final-build runs.

Presentation 20 FPS is covered by canonical cases. Idle, slash, combo, shield,
sign and Z-sign each passed 60/120 presentation FPS with three repetitions on
the second checkpoint (36 runs), plus trace-disabled slash (three runs).
The final contact-boundary fix changed admission and copied negative tests;
packet schema, traversal, authority and emitter/late-slot driver are identical.
`build/pass3d-08/presentation-reuse.json` verifies that source equality and binds
the exact receipts. All ten final canonical cases still select extraction and
match exactly. Six fresh final-build runs cover shield at 120 FPS and
trace-disabled slash, each three times. The 39 earlier runs are explicitly reused,
not represented as runs of the final executable.

The final source review checked duplicate authority/callbacks, whole pre-mutation
fallback, traversal/list/cursor order, live IK, weapon history and warm-up,
registration order/counts, combo/blur/hooks/audio, frame-arena lifetime and prior
pose/contact latency. It resolved the scratch-pointer lifetime and conservative
admission issues described above. Affected canonical, negative and focused gates
then passed. No new native crash occurred. Historical failures remain preserved.

`build/pass3d-08/final-acceptance.json` binds the individual acceptance receipts.
Its SHA-256 is
`0a26f34342715b69248004f22ce54681786bc277348a0512adc34d86d3d68ab4`.
All generated evidence remains ignored and local; no ROM, Nintendo assets,
runtime save, executable or traces are committed or uploaded.
Completed Pass 3D JSONL evidence is losslessly NTFS-compressed, with hashes
verified for every newly compressed file. The 301 retained files contain
8,967,476,402 logical bytes and occupy 3,602,481,201 stored bytes; free space at
handoff measurement was 24,260,431,872 bytes. `build/pass3d-08/compression.json`
records the operation. No evidence or original assets were deleted.

## Deliberate omissions and limits

- No runtime 30-Hz tests or 60/120-Hz gameplay, timing scaling or movement changes.
- No previously omitted 48 Player variants or 39 startup-stress runs.
- No second full 39-run presentation campaign after the final admission-only
  correction; source equality, all-ten exact canonical coverage and six fresh
  focused runs justify reuse.
- Broad regression uses four original and four HUD/message fixtures, each three
  times; it does not claim a fresh complete original/HUD cross-product.
- No fresh renderer-helper execution where every input/dependency is unchanged.
- Negative admission proves predicate rejection on copied profiles and verifies
  the whole legacy branch by source review; it does not simulate every rejected
  profile end to end. Hostile block/parry and unadmitted contact partners remain
  on legacy draw.
- No human gameplay, GPU/pixel or audible acceptance. Startup master volume was
  zero. Automated canonical behavior and startup are separate from human play.

The 20 negative cases cover pause, transition, reflection, first person, crawl,
carry, get-item, hammer, Hylian shield, dynamic floor, airborne, frozen,
draw-disabled, owned child, hostile scene, projectile, foreign AT, non-sign AC,
unknown collider shape and unknown limb hook. Unsupported production profiles
execute the original skeleton callbacks without a partially prepared packet.

## Next authorized boundary

The next proposed pass is Player timing-unit and opportunity/reset contracts,
retaining an exact canonical path and the capability boundary. Subsequent work
must integrate fixed Player cadence, input/camera scheduling and the world-20
contact bridge before interactive 120-Hz sword authority can be claimed. At least
two further implementation passes remain; allow a third if integration and
interactive qualification need separate scope. None of that work starts here.
