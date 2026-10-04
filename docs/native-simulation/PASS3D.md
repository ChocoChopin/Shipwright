# Pass 3D: canonical Player pose/contact extraction

Status: implementation in progress; no acceptance claimed.
Starting checkpoint: `ce54b58bb94d5fc695494fb70a6c6aa1e98bd3d7`.
Dependency pins remain libultraship `9280b17ddc504da6630892a46440e86be41ac571`
and torch `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`.

## Ownership

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
to 10,000 model units on each horizontal axis.

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
Full-pass acceptance is still pending the review corrections and focused gates.

The subsequent source review explicitly excludes non-normal game mode and
airborne poses, adds an airborne negative admission case (14 total), and restores
the borrowed `flexLimbOverrideMTX` pointer after the synchronous traversal. The
legacy arithmetic/callback ordering is unchanged. Final-executable canonical
checks cover the six presentation cases so that their FPS comparisons use the
same executable on both sides. The four remaining canonical cases reuse this
complete 30-run checkpoint; they are not represented as rerun on the later build.

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
are still pending at this second source checkpoint.

## Next authorized boundary

The next proposed pass is Player timing-unit and opportunity/reset contracts,
retaining an exact canonical path and the capability boundary. Subsequent work
must integrate fixed Player cadence, input/camera scheduling and the world-20
contact bridge before interactive 120-Hz sword authority can be claimed. At least
two further implementation passes remain; allow a third if integration and
interactive qualification need separate scope. None of that work starts here.
