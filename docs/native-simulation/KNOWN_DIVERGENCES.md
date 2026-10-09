# Divergence and ambiguity register

Pass 4B is in progress. The fixed clock and animation ownership checkpoints do
not close the movement, camera, mixed combo-counter or contact ambiguities below.
High-rate animation adapter branches are prepared but not production-admitted;
do not interpret canonical queue tests as high-rate gameplay evidence. The first
asset-free queue run exited normally but lost redirected stdout to the Windows
game console. Its failed receipt remains in `build/pass4b-02/animation-queue`;
the stream-preservation correction passes all 23 checks in `build/pass4b-03`.

The prepared 4B constant-gravity adapter selects the affine continuation in
TIMING_SEMANTICS.md, including its half-step displacement term. An earlier local
scaled semi-implicit candidate was replaced before high-rate engine admission;
its primitive receipts remain historical evidence. Clamp crossings remain outside
this primitive's supported domain. Coupled action acceleration, contact resolution
and camera smoothing still require engine qualification. No high-rate gameplay
result is inferred from the 170 passing native primitive checks.

Pass 4A adds observational temporal state and isolated primitives only. It does
not resolve the existing high-rate smoothing/contact/input ambiguities by silently
choosing a gameplay model. New ownership scope invalidation clears only sidecar
metadata; canonical sword history remains at original reset sites. Input diagnostic
counts are scoped to generation, and acquisition metadata is distinct from actual
menu-filtered gameplay input. Live pause/death/scene transitions have source and
native lifecycle-contract coverage, not new end-to-end transition fixtures.
PASS4A.md retains implementation/build findings and exact runtime acceptance.
No new measured canonical divergence or native crash occurred. The QA driver's
Windows atomic-replace lock race was test infrastructure: the failed partial run
is retained, identical-command retry is bounded by its existing timeout, and the
fresh six-run stepping campaign passes. Source review also separated Player
lifetime reset from open world-context ownership; nine final-build fixture runs
and startup pass. Broader results are explicitly source-reused, not rerun.

Pass 3D admission/compile failures are tracked in PASS3D.md as implementation
findings, not accepted gameplay divergences. All ten canonical extraction fixtures
match the unchanged Pass 3C references across three repetitions; no new measured
divergence or native crash occurred. Unsupported profiles remain wholly on
legacy draw; this does not resolve any high-rate timing/contact ambiguity below.

The first pass has **no implemented higher-rate mode and no measured gameplay
divergence**. Entries below are source-derived risks and semantic questions to
test, not observations from 60/120-Hz Player play; runtime 30 Hz is deferred. No entry authorizes changing the
canonical 20-Hz behavior. The exact baseline is
`9eafd15fe1382c5a41e881f1b6ea87345c797d18`.

## Pass 2 oracle envelope and implementation findings

There is still no higher-rate gameplay mode. Replay deliberately controls external
inputs: a debug-save boot recipe, fixture scene seed, normalized controller states,
audio counter clock and fixed 528-sample sink. It preserves the original gameplay
arithmetic and full CPU draw. Repeating this controlled executable does not prove
that an untouched upstream binary has deterministic host audio or RNG behavior.

The first automated launch exposed an integration failure: `RunExtract` treats
additional arguments as ROM filenames, so the native test flag opened a ROM-error
modal and the runner timed out. Test arguments are now consumed before extraction;
ordinary extraction arguments retain their original route. This is a harness
startup defect, not a ROM defect or a measured gameplay-timing divergence.

The initial corpus also exposed startup log text at the beginning of two JSONL
outputs. The strict parser rejected those runs. Test mode now preserves redirected
Windows standard handles and opens its diagnostic files after logger setup,
buffering any earlier trace records. The six repaired message/Keese probe runs
passed without filtering log lines; final full-corpus acceptance is in TESTING.md.
This is test infrastructure failure, not a timing-semantic exception.

The second full corpus completed 35 of 36 runs. `gravity-fall/run-003`
failed during the first setup presentation with a Windows access violation in
libultraship's `gfx_load_tlut_handler_rdp`, before any measured snapshot or fixture
Player placement. Its entire 78,223-byte trace matches the successful run's prefix;
all 399 complete records agree. This does not establish a gameplay divergence or
prove that the fault predates this harness. Its cause was unresolved at the
Pass 2 checkpoint; the later reproduced source-extent finding is recorded below.
Evidence is retained in `build/native-simulation-corpus-02`, including the crash
log under that run's `work/logs/Ship of Harkinian.log`.

Replay-only crash reporting now records the active graphics command and retained
texture metadata in the existing crash log. Addresses never enter semantic
snapshots or replay hashes. At that Pass 2 checkpoint, no rendering operation was
skipped, clamped or otherwise changed. Subsequent review added a capacity bound to these extra log
writes; it fixes diagnostic reporting only and does not fix the startup fault.
Forty fresh one-step startup probes passed with the first diagnostic
build; they are not forty complete gravity fixtures and do not prove a fix.
Two separate debugger-assisted probes stalled during graphics initialization and
captured no access violation; those are debugger-infrastructure failures. Final
corpus and presentation results are recorded separately in `PASS2.md`.

Open oracle coverage limits: per-family actor actions/timers beyond base Actor,
all caller-owned RNG stream identities, complete ocarina play/scoring, paused and
scene-transition fixtures, GPU-free execution, and cross-platform float identity.
The Ice Keese fixture now directly observes its existing limb callbacks: six
gameplay RNG draws per measured CPU draw, 360 over 60 steps, with stable actor
attribution. Gohma combat and general actor coverage remain untested. The
authoritative event fingerprint excludes diagnostic presentation
counts while retaining RNG draws and gameplay audio ingress in order.

## Pass 3A startup and extraction-design status

The preserved final Pass 2 executable completed a bounded 180-start matrix and
12 full gravity replays without a TLUT access violation. These 192 successes
remain a separate reference campaign, conditional on its recorded executable
path, assets and configuration; they are not a general reliability guarantee.

The later observation candidate reproduced a TLUT access violation in corpus 05,
`gravity-fall/run-002`, before measurement. The aggregate remains failed: 34 runs
completed out of 35 attempted, with gravity run 003 unattempted. Captured metadata
identifies an owned `gLinkChildSwordTLUT` resource with a 216-byte image, but a
512-byte transfer request. The image starts inside its retained 296-byte file
buffer and the request extends 296 bytes past that buffer. This establishes a
renderer source over-read for this reproduction; the old corpus 02 log cannot
retrospectively establish the same resource identity.

All 38,415 archive members were scanned. Exactly eight display lists reference
this palette; their audited CI8 jewel texture uses only indices 0–106, never the
omitted entries. Original ROM comparison confirms the extracted bytes, while the
original bulk transfer's tail contains adjacent texture data. A reviewed bounded
staging fix in `ChocoChopin/libultraship`, branch `codex/tlut-source-bounds`, copies
the available resource bytes and zeroes only the requested unavailable tail.
This is a defined fallback, not byte-identical N64 transfer emulation. CI4 partial
write preservation and raw-source fallback remain intact; archives are unchanged.
The dependency fix is committed as `c6bbb8c328938c115f4a1cbeaca3d00a4502269d`;
rebuilt-engine validation passed the original canonical/matrix, CLI, ordinary
startup and 192 fresh startup trials on the fixed bytes. The native helper test passed
31 checks; no guard-page or intentionally crashing test was
performed. [PASS3A.md](PASS3A.md) owns the captured ranges and evidence paths.

A separate startup condition was observed when the same executable bytes were
launched from copied directories under `build/`: five retained 45-second timeouts
ended at the D3D adapter log without durable scene/measurement evidence. The
original `x64/Release/soh.exe` path completed paired probes and the campaigns.
The path association is evidence, not a mechanism. The intentionally stopped
first campaign additionally retains one interrupted attempt and 177 unattempted
schedule entries. Neither the failures nor the interrupted attempt is pooled away.
There is no captured stack proving the wait location, and a visible-launch probe
also timed out. [PASS3A.md](PASS3A.md) owns exact denominators and identities.

These incomplete starts remain reliability/infrastructure records, separate from
a completed canonical semantic mismatch. Later extraction passes must retain
each failure and stop for user direction before debugging or reproduction.
They may not automatically attribute every crash to the old fault, or
declare a completed state/event mismatch exempt because startup has a known risk.

ND-006/010/013 have a bounded next implementation design for the main countdown
and English 0x1043 message, including old-digit warnings, STOP latency, input/page
edges and entry-state paint packets. Eight pre-extraction fixtures observe those
legacy paths. No authority has moved and those general risks remain open. In
particular 60/120 display-list replay equality does not prove that repeated C
presentation helpers are pure; that is a separate required High-pass test.
Other HUD/message branches, actor pose/collision and draw RNG remain draw-owned.

## Pass 3C resource-backed I4 crash repair

The retained `pass3c-draw-on-01/hud-zero-input/run-001` access violation is in
`ImportTextureI4`, distinct from the older TLUT fault. Its magic-meter resource
contains 64 image bytes at the end of a 144-byte owned file, while the legacy HUD
load requests 128 bytes (16x16 I4). The importer decoded the unavailable tail;
the allocator layout determined whether that read crossed an inaccessible page.
This is renderer application source failure, not a retiming error or a gameplay
semantic exception. The modal crash handler also explains the later timeout.

After explicit user authorization, the separate dependency repair
`9280b17ddc504da6630892a46440e86be41ac571` on `codex/i4-resource-bounds` adds bounded
resource-row staging. Available bytes, requested dimensions, UV convention and
valid-resource decoding remain intact; missing bytes become zero intensity/alpha.
The existing TLUT routine shares the same resource-extent check and still passes
all 31 checks. Raw/replacement pointers and other texture formats are not admitted
by this fix. Missing-tail zero-fill is a defined fallback, not adjacent-ROM emulation.

The 24 new native copy checks, 129 existing Python tests and 18 focused repaired
engine processes pass. Canonical HUD/message/gravity state and events match prior
references; presentation/tracing variants and helper purity also pass. The old
failure's 8,835 complete trace records match the repaired prefix. Historical failed
files remain hash-identical. PASS3C.md and ignored
`build/pass3c-crashfix-01/crash-resolution.json` own the exact identities and limits.
The subsequent Pass 3C acceptance completed under the user's reduced scope:
90 canonical and 135 matrix replays, focused controls and ordinary startup.
No new native crash occurred. The 48 omitted Player variants and 39 omitted
startup-stress runs remain explicit coverage limits in PASS3C.md.
The repaired continuation passed 90 canonical replays and their analyses, then
retained a presentation runtime-budget overrun: 75 exact completed snapshots,
continued trace writes through the cutoff, and no logged native exception. This
is not classified as a timing-semantic divergence or evidence of another repaired
renderer fault. The bounded host-time allowance is recorded in PASS3C.md.

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
| ND-005 | Class 3; authored frame/event phase | `z_demo.c:231/:432/:1647`; `z_en_bom.c:269/:318/:323`; bomb timer is fuse duration, equality event and bit-pattern blink. Supported Player modes share legacy 50-ms boundaries. | Keep authored event labels; consume crossings once in deterministic order. Specify ceiling-to-next-boundary event timing and phase/reset behavior. Never use a repeatedly floored frame label as permission to trigger again; do not replay a world event on each Player substep. |
| ND-006 | Class 3; Draw-to-simulation extraction ordering | `z_player_lib.c:1789` body/weapon pose, Gohma `:2104/:2134` focus/spheres; `z_actor.c:2681` update culling. Previous Draw-produced state may be consumed on next update. | Keep the existing authoritative command-generation/Draw stage initially. A later CPU pose pass must match original visibility/culling, transformation and previous-step timing at 20; advancing pose before collision may silently change canonical behavior. |
| ND-007 | Class 3; input resolution and short-pulse representation | Player `z_player.c:11776` consumes common input, numerous `press.button` branches; held input and edge masks have distinct contracts. | Choose timestamp-to-boundary assignment once in input architecture. Test 10-ms pulses, multiple transitions within one canonical step, simultaneous release/press and pause. Finer-rate responsiveness is intended; changed edge multiplicity is a bug. There is no unique analogue of a pulse never observed at 20 Hz. |
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

- A 70-count bomb fuse becoming 70/120 seconds, or duplicated duration expiry across six Player substeps. The s=2/3 math
  oracle separately detects nonintegral drift without requiring runtime 30 Hz.
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

## Pass 3C boundary decisions (design, no retiming yet)

- ND-016: a target such as EnKanban reads live Player melee animation during
  its update, after Player update. A high-rate queued hit must carry its attack
  identity and animation. Preserve exact live-read timing in canonical mode;
  use producing-attack semantics in the future high-rate bridge and classify any
  changed cut selection at a shared boundary as an explicit Class 3 decision.
- ND-017: world reaction remains 20 Hz, Player contact feedback uses Player
  cadence. Detection, reservation and damage commitment are distinct events.
  Repeated substep overlap is not permission for repeated damage. Legacy multi-hit
  opportunities cannot be replaced by an unconditional one-hit-per-B rule.
- ND-018: static-world admission excludes DynaPoly carry and reciprocal pushes.
  Holding a moving-platform transform while substepping its rider is not proved
  correct. Static collision resolution differences are candidates for Class 2
  only after integration/geometry/order contracts pass.
- ND-019: Player update contains blink RNG, HUD, sequence and environmental
  work. Whole-function repetition would alter world clocks and random order.
  PASS3C.md requires explicit splits; the machine-readable graph records them.

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

## Pass 3B accepted extraction and evidence corrections

Both original and bounded HUD/message corpora remain exactly equal to the
preserved 20-Hz references with helper verification off/on. Direct off/on
snapshot and trace bytes agree, and presentation at 60/120 FPS plus trace-off
cases retains canonical snapshots. This does not measure high-rate gameplay.

The initial diagnostic JSON-null coverage exception was repaired and validated
under the authorized Sol work; the failed run remains retained. Focused review
added explicit packet immutability checks and hash-bound purity receipts. Strict
draw analysis initially rejected the new CLI flag, a test-infrastructure error
corrected by binding that flag to the declared mode without relaxing state or
paint checks. CLI validation now stops after its first unexpected result.

An earlier specification required invisible packets to emit no commands. Actual
legacy message START/CLOSING still emits segment/setup commands with no visible
paint. The implemented contract preserves and compares those commands; countdown
invisibility still emits none. This is a source-derived specification correction,
not a canonical divergence. PASS3B.md retains all failure paths and exact results.
Global draw purity, general unsupported-profile coverage and human gameplay
acceptance remain unproved; no higher-rate mode has been added.
