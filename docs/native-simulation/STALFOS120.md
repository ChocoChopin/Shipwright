# One-enemy temporal experiment

## Current outdoor playtest

The `stalfos` launcher now uses open Hyrule Field (authored entrance505), with
one ground-following type2 Stalfos at a time. After actual actor destruction,
40 world updates elapse before a replacement spawns on checked nearby static
ground. Health refills on each successful spawn. Pauses/cutscenes/transitions
do not create extra rounds; scene reset clears the arena owner. This is an
opt-in disposable test (`NativeStalfosArena=2`), not a change to ordinary saves.
Selecting Original20 restores both Player and enemy cadence; rendering stays120.
Right bumper now toggles20/120 for both. Player death or leaving Hyrule Field
automatically requests a fresh arena scene with full health. This is a normal
scene teardown/reload, not an in-place resurrection. User authorizes closing the
running game gracefully for replacement builds and reopening it afterward;
do not repeatedly request that the user close the executable.

Hole walking in the prior venue was original type1 behavior: `EnTest_Update`
clamps both actor Y and floorHeight to home Y, already present in the pre-change
`da4f927f0` source. The first solo encounter used type1; the subsequent reviving
pair used type5 and was not the converted enemy. The outdoor test does not use
that encounter sequence. Its owned actor follows actual floors even during
the death helper's temporary parameter change to type1. Other variants retain
their original behavior. The same explicit high-rate work now admits this owned
type2 actor as well as the earlier type1 experiment.

The historical Forest Temple fixture and first-pass evidence below remain.

The opt-in `stalfos` playtest boots the Forest Temple upper bow-room duel
(room6, above the pit), using its actual platform controller and type1 Stalfos.
It clears the lower encounter only in the disposable debug save. Ordinary saves
and ordinary enemies do not enable this experiment.

`NativeStalfosHz` enables one live type1 actor when Player's high-rate scheduler
is active. Original20 uses the original actor path. Invisible, ceiling and
reviving-pair variants remain world20. This is not a general enemy conversion.

## Ownership

- The real EnTest action callback, RNG, decision/reaction opportunities, integer
  timers, head tracking, speed/heading selection, damage consumption and death/
  spawn logic stay world20. There is no repeated AI callback between world ticks.
- Movement integrates the selected velocity and gravity at Player steps. Units
  remain velocity per legacy movement unit and gravity per50ms; displacement uses
  the original1.5 gain. The affine vertical integration retains the full-step
  endpoint when no floor/terminal constraint intervenes. OC correction is consumed
  once. The arena type1 actor retains its original home-height floor constraint.
- Main and shield animation advance by elapsed duration, including morphs and
  fractional authored-frame sampling. Original normal-animation callers retain
  their old rate; the explicit synchronous duration entry point restores its
  context before returning. Near-integer roundoff is snapped at1e-4 authored frame.
- The intermediate helper evaluates floor support, body/focus positions and
  sword/shield geometry. Down/up slash windows use the original first sampled
  active frames8/2 and exclusive ends11/8. Jumpslash retains its phase handoff
  at world opportunities and closes its active geometry at frame5.
- Current Player/enemy geometry is queried at each Player boundary through the
  existing collision shape dispatcher. Each attack/opportunity delivers once.
  Damage and tactical reactions retain their original world consumer. The pair
  is excluded from the old world AT query and deferred Player/world sweep path,
  preventing a second or time-misaligned query. Other actor pairs remain unchanged.
- Player's existing heavy-body constraint reads this actor's current cylinder
  rather than a stale world-position copy. Original world reciprocal OC remains.
- Blur vertices, dust, ice emissions and BodyBreak allocation bookkeeping stay
  at world draw opportunities. Intermediate CPU draws update pose geometry and
  prepared rendering commands, not those world effects.

## Rendering and lifetime

The world Actor_Draw slot reserves two display-list bindings. Each admitted
Player step prepares fresh opaque/translucent commands and limb matrices in the
current graphics arena. Those commands render the newly evaluated enemy pose;
ordinary actor-frame interpolation is suspended only around this actor's draw.
The outer lighting, object segment, color-filter and feet-shadow behavior remains.
The selected actor's CPU draw bypasses view culling so looking away cannot
suppress its authoritative pose or the remaining fine movement steps. GPU
clipping still handles out-of-view primitives; other actors keep normal culling.
Matrix/recording/segment state is restored after preparation. The bindings and
all arena pointers expire at the next world frame. Scene reset and actor deletion
invalidate the selected actor. No framebuffer queue or retained inter-frame pose
has been introduced.

## Validation

Focused results and final build identity are recorded in TESTING.md. The initial
engine contact stopped gracefully on the old validator's blanket prohibition of
high-rate contacts; the revised verifier allows only this explicitly owned pair.
It still rejects unrelated high-rate calls into legacy collision. An online RNG
check covers intermediate work, and compact receipts record world decisions,
motion/animation/pose opportunities, attack contacts and the first slash's frames
at common50ms boundaries. Human combat feel and motion acceptance remain pending.

The counterattack gate caught an incorrect legacy AT-list requirement. Player
high-rate swords bypass that list by design; the direct query now requires the
bridge's produced changed sweep instead. One counter causes one contact and
enemy health10 to8. The shield gate records one block with no Player health loss.
Guard blend and standing upper-body recoil deliberately hold the base animation;
the phase verifier checks those holds rather than demanding an idle advance.

## Human comparison

2026-10-31 follow-up: the radial part of sidestepping now advances at each fine
step, as do shape/head turns toward the world-selected heading. Spawn pose keeps
the action's home-Y anchor instead of alternating with gravity/floor snaps.
Rendering ownership no longer changes solely because a world hitstop skips the
enemy update. Body-break matrices refresh from the last evaluated limb pose;
allocation, RNG and part spawning remain world-owned. These address identified
discontinuities, not a claim that every intermittent visual artifact is eliminated.

Run `play_scenario.py stalfos` for the experiment, or add `--rate 20` for original
Player/enemy authority at rendering120. Lock on, circle, block, dodge the jumping
slash and counterattack; watch feet, sword, shield and shadow alignment. Do not
generalize one successful encounter to every enemy, damage type or Stalfos variant.
