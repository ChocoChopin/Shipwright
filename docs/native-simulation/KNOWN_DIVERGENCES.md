# Divergence and ambiguity register

The first pass has **no implemented higher-rate mode and no measured gameplay
divergence**. Entries below are source-derived risks and semantic questions to
test, not observations from 30/60/120-Hz play. No entry authorizes changing the
canonical 20-Hz behavior. The exact baseline is
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`.

## Classification contract

**Class 1: conversion bug.** The implementation violates the selected mathematical
or event contract: changed duration/speed, repeated input edge, missing event,
double integration/scaling, RNG consumption from presentation, wrong reset or
20-Hz regression. Fix it and retain a failing-then-passing fixture.

**Class 2: expected resolution consequence.** Correct finer steps encounter a
threshold, collision or new intermediate state sooner/differently. Admit this
only after checking rates, units, event order, initial conditions and integration
contract. Record bounded evidence; do not force all actor decisions back onto
20-Hz updates to remove the difference. A threshold crossing may change later
discrete outcomes substantially; compare the causal trace, not just a late
position tolerance.

**Class 3: semantic/design ambiguity.** More than one defensible generalization
exists. Record alternatives and the gameplay consequence. Implement unrelated
settled conversions independently. Preserve 20 Hz; do not silently rebalance an
AI, damage window, collision trick or speedrun-relevant interaction.

Use `unclassified` while investigation is incomplete; a discrepancy is not
Class 2 simply because the rate is higher. A Class 3 decision can yield Class 1
tests once its contract is fixed. A regression at 20 Hz is not excused by Class 2.

## Source-derived register

| ID | Current classification/status | Source and issue | Required evidence / decision boundary |
| --- | --- | --- | --- |
| ND-001 | Class 3; integration policy needs per-system application | `z_actor.c:1287/:1295`, `z_effect_soft_sprite.c:254`: gravity/acceleration then position; actor position already uses `R_UPDATE_RATE/2`, particles do not. A continuous ballistic trajectory and exact fractional extension of a discrete integrator need not coincide. | Follow TIMING_SEMANTICS' selected integrator contract, retain exact 20 path. Test constant acceleration, cap crossings, impulses, landing and clamp cases separately. Do not call systematic unapproved gravity/range change Class 2. |
| ND-002 | Class 3; AI opportunity policy unresolved per decision site | Stalfos `ovl_En_Test/z_en_test.c:394/:406`; Gohma `ovl_Boss_Goma/z_boss_goma.c:1100/:1747`: parity/modulus gates combined with random branches. | Distinguish response sensing every sim step from authored think/choice cadence. A historical cadence may gate a named decision without freezing actor simulation. Need attack-count distribution, first-eligible observation and RNG-call trace before selecting hazard/cadence semantics. |
| ND-003 | Class 3; stochastic-model choice | `z_kankyo.c:1816` lightning accumulates a 10%-chance 50-unit jump plus a random fractional increment; `code_800FD970.c:4` LCG is globally shared. | Hazard conversion `p(s)=1-(1-p)^s, s=20/f` preserves independent no-event probability, not arbitrary shared-stream realizations, correlated random walks or compound accumulation. Trace original draws and define intended process. Do not apply probability scaling to every Rand call or reseed per tick. |
| ND-004 | Class 3; integer/piecewise smoother edge policy | `z_lib.c:24/:386/:514/:554`: integer truncation, minimum steps, caps, angular wrap and boolean/remaining-distance returns. | The exponential fractional map applies only within its stated assumptions. Verify small positive/negative errors, wrap boundaries, min-step/cap crossings and return timing. Preserve fractional residue rather than truncating a sub-unit step to zero; retain exact legacy behavior at 20. |
| ND-005 | Class 3; authored frame/event phase | `z_demo.c:231/:432/:1647`; `z_en_bom.c:269/:318/:323`; bomb timer is fuse duration, equality event and bit-pattern blink. At 30 Hz many 20-Hz event times lie between simulation boundaries. | Keep authored event labels; consume crossings once in deterministic order. Specify ceiling-to-next-boundary event timing and phase/reset behavior. Never use a repeatedly floored frame label as permission to trigger again; do not round each 20-Hz frame into an integer 30-Hz tick. |
| ND-006 | Class 3; Draw-to-simulation extraction ordering | `z_player_lib.c:1789` body/weapon pose, Gohma `:2104/:2134` focus/spheres; `z_actor.c:2681` update culling. Previous Draw-produced state may be consumed on next update. | Keep the existing authoritative command-generation/Draw stage initially. A later CPU pose pass must match original visibility/culling, transformation and previous-step timing at 20; advancing pose before collision may silently change canonical behavior. |
| ND-007 | Class 3; input resolution and short-pulse representation | Player `z_player.c:11776` consumes common input, numerous `press.button` branches; held input and edge masks have distinct contracts. | Choose timestamp-to-boundary assignment once in input architecture. Test 10-ms pulses, multiple transitions within one 30-Hz step, simultaneous release/press and pause. Finer-rate responsiveness is intended; changed edge multiplicity is a bug. There is no unique analogue of a pulse never observed at 20 Hz. |
| ND-008 | Class 2 candidate; not yet observed | Hookshot `z_arms_hook.c:259`, arrow collision, ledge and dynamic-platform contact. Smaller movement steps can encounter contacts and narrow states absent at 20. | Admit only after duration/speed/integration and sweep endpoints are correct. Record first differing hit/normal/surface, penetration, state order, input phase and time bound. Keep the richer simulation; do not discard substep hits merely to match the 20-Hz path. |
| ND-009 | Class 3; combat opportunity versus duration | `z_player.c:11803` signed invincibility; `:11665` frame-masked burn damage; sword collision geometry from `z_player_lib.c:1527`. | Preserve authored active/invulnerability durations and damage pulse counts. More spatial observations can cause new valid hits (possible Class 2), but repeated damage on one active interval is Class 1. Tests must separate contact, hit registration, damage consumption and cooldown. |
| ND-010 | Class 3; clock domain and freeze semantics | `z_play.c:1116/:1129/:1286`; message Draw and Interface_Draw; menus still progress when gameplay is paused, environment has its own gate. | Write the clock-domain/gating table before converting timers. Test pause entry/exit, dialogue, ocarina, death, transition and single-step. A shared wall-clock deadline cannot replace every local countdown. |
| ND-011 | Class 3; animation/rendered pose sampling | Link `z_skelanime.c:1215` uses `/2`, ordinary `:1648` uses `/3`; event crossing `:1413`, root motion `z_actor.c:1336`. | Preserve authored animation units, event order, reverse/wrap and root displacement. Fractional pose interpolation affects collision geometry; diagnose separately from event-time preservation. No extra scaling of a delta already integrated by animation. |
| ND-012 | Class 3; reset/persistence semantics | SaveManager saves gameplay progress; `z_play.c:371` and `Enhancements/ResetGameplayFrames.cpp` reset counters; fishing has file-static lure/rod/weather state. | A scene reload is not proved to reset all state. Use process-isolated fixture replay initially. Rate/config and scenario identity belong in trace metadata; decide transient phase reset policy before changing rate mid-session or persisting new fields. |
| ND-013 | Class 1 risk; not an observed bug | Gohma Draw `z_boss_goma.c:1987` draws random RGB from shared gameplay stream; messages/HUD timers also mutate on Draw. | Presenting or skipping rendering must not change authoritative random calls, timer events or colliders. A high-FPS display-list replay may be safe while repeated C Draw is not. Test identical sim trace at several render rates including slow presentation. |
| ND-014 | Class 1 risk; not an observed bug | Bomb impact `z_en_bom.c:176` multiplies by `-0.3`; actor OC displacement `z_actor.c:1290` is added directly. | Negative restitution/reflection and contact correction are event/geometric operations, not time decay. Applying `pow(-0.3,h)` or scaling correction by dt is a conversion bug. Verify one restitution per contact event and no double platform/OC displacement. |
| ND-015 | Class 3; audio and gameplay event clock | `z_player.c:1819` animation sound list; Epona `z_en_horse.c:611` sound cursor; `audio_heap.c:829` audio update/sample parameters; message ocarina state | Separate one-shot timestamp, sustain refresh and sample/mixer clock. Do not increase pitch/tempo by increasing gameplay call count. Ocarina score/reaction windows require dedicated deterministic audio-event fixtures. |

Source paths shortened to code files refer to `soh/src/code`; actor filenames
refer to their `soh/src/overlays/actors/ovl_*` directories, fully listed in
CONVERSION_LEDGER and the machine inventory. ND-008 is only a candidate Class 2;
ND-013/014 are preventative assertions, not detected failures.

## Counterexamples future tests must reject (Class 1)

- A 70-count bomb fuse becoming 70/120 seconds, or repeated ceil-rounding of
  1/20-second intervals causing cumulative drift at 30 Hz.
- Applying a linear fraction to multiplicative retention rather than the
  exponential contraction required by the chosen continuous extension.
- Advancing actor velocity through a shared dt helper and scaling its caller's
  already-integrated displacement again.
- Missing an equality-keyed script event, or firing it on every substep whose
  floor is the same authored frame.
- Suppressing all new input/state/collision decisions until the next 20-Hz tick
  and presenting that as native 120-Hz gameplay.
- Making identical-rate repeats nondeterministic, changing 20-Hz event/RNG order,
  or increasing AI choices/damage opportunities without an explicit contract.

## Measured entry template

Create `ND-NNN` with the following fields when an actual run first differs:

```text
status: unclassified | investigating | decision-required | accepted | fixed
class: unknown | 1 | 2 | 3
baseline_commit / implementation_commit / source hashes:
fixture_id / ROM version-hash / asset manifest hash / config digest:
input_trace_id / seed / actor IDs and parameters / initial state digest:
simulation_rates / presentation_rates / comparison boundary phase:
first_different_time / field or event / expected and actual values:
minimal reproducer command / local evidence paths:
mathematical contract / causal explanation / quantization or error bound:
20-Hz reference result / same-rate repeat result:
alternatives and gameplay consequences (Class 3):
accepted decision and who/what approved it:
fix_commit or acceptance_scope / regression fixture IDs:
owner / related actor claims and subsystem claims:
```

Never close an entry with only a screenshot or a broad final-state tolerance.
Track first cause and stable event identity. A Class 2 acceptance applies to its
stated contract and scenario, not every later symptom. Keep unresolved Class 3
rows visible; they need not block independent actors or shared-tooling work.
