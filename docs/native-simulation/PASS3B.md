# Pass 3B: bounded HUD and simple-message extraction — accepted

Date: 2026-10-03. Starting Pass 3A checkpoint:
`02075db9d7edf6b3b65abe8cfd0b1e12237b0a4e`.
Validated runtime commit: `ae9c2bfc2dfa778712da05b118bbceb3727316ef` on `mod/native-simulation-rates`.
This pass completes only the bounded late countdown and English 0x1043 extraction.
Canonical simulation remains 20 Hz. No actor or high-rate gameplay was converted.

## Runtime identity

The Release executable at `x64/Release/soh.exe` is 27,017,216 bytes, SHA-256
`93973762f119adf169b8567b620f310f0ecc0d9f88313d3be91cb4dba3019f03`. Its preserved copy and matching PDB are under
`build/pass3b-evidence/reviewed-runtime/`; launch the established x64/Release path.
PDB SHA-256: `86c91528a1c830a6a5273355b5996dea6214d3816cea98cd80e1ecb80e2831b9`.

The build used `e092381d0f828a75bf119485c061dabeca50d646` plus the four reviewed
runtime-file changes. `review-build-01/source-inputs.json` binds raw build inputs;
`runtime-binding.json` proves their correspondence to the committed runtime,
with explicit CRLF/LF comparison. The embedded Git stamp alone is insufficient.
The baseline build wrapper exited 0 and retained its full invocation/log in
`review-build-01/`. Windows x64 MSVC 19.44.35228, SDK 10.0.26100.0, CMake 4.4.3,
and Python 3.12.5 remain the recorded toolchain.

Dependencies are unchanged: libultraship `c6bbb8c328938c115f4a1cbeaca3d00a4502269d`,
Torch `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`, vcpkg
`3aea538b2bb21a586502c67b00eb474fdd2e3098`. Assets remain local and ignored:
oot.o2r SHA-256 `a058767a2f4f8f415b099a5c5189a9bf974a068f331b88131afea8df24e6a997`,
soh.o2r `51c8b166b913fdc745901fc48a2ca6261e480e3e8c0715ae0f2d946adb982712`,
controller database `e606134678e6b3fdbdec9081289a1f0ba3e25d53b59b2aa3cdfe69bf491ad18f`.

## Implemented ownership and admission

`soh/src/code/z_parameter.c` adds `Interface_IsCountdownProfileAdmitted`,
`Interface_AdvanceCountdownLegacy`, and `Interface_DrawCountdownPresentation`.
Admission requires normal game mode, Link's House or Kokiri Forest, no subtimer
or hazard, no minigame/gallery, frame advance, freeze, NoUI, pause/debug pause,
game over, message, ocarina attempt, transition or cutscene, and default timer
placement. Seconds are 0..3599; states are DOWN_INIT/PREVIEW/MOVE/TICK/STOP/OFF;
MOVE requires a positive divisor. Existing outer HUD gates still apply.
Only admitted main state/seconds, hidden eligible-call divisors, integer X/Y
slide, retained digits, prior-drawn-ones warning selection and ordered SFX ingress
move into explicit late authority. MOVE fallthrough and the complete STOP
transaction before OFF are preserved. Counter units and reset/lifetime are unchanged.

`soh/src/code/z_message_PAL.c` adds `Message_IsPlainTextProfileAdmitted`,
`Message_AdvancePlainTextLegacy`, and `Message_DrawPlainTextPresentation`.
Admission requires normal mode in the same two scenes, English ID 0x1043,
null talker, its full 64-byte resource fingerprint and ASCII/NEWLINE/BOX_BREAK/END
grammar, black/default-end textbox, no choices/ocarina/credits/fade, supported
modes, default text speed/skip/spacing/color configuration, and no pause, freeze,
frame advance, NoUI, cutscene, transition, game over or debug display. Displaying,
await-next and done require the entire expected live decoded page; opening and
closing do not inspect stale decoded storage. Other profiles, including 0x305F,
take the original fallback before mutation. There is no partial new-path fallback.

The exact predicates are the two named functions above. Message admission uses
fingerprint `0xf8afb1cd8bd57335`, BOX_BREAK at byte 27, NEWLINE at 42 and END at 63;
all other bytes must be ASCII. Admitted modes are TEXT_START, TEXT_BOX_GROWING,
TEXT_STARTING, TEXT_NEXT_MSG, TEXT_CLOSING, TEXT_DISPLAYING, TEXT_AWAIT_NEXT and
TEXT_DONE. The latter three require page 1/2, decoded length 27/35, draw position
at most length + 1, matching decoded bytes, positive icon-flash timer and color
index 0/1. Character scale is positive; TextSpeed/SlowTextSpeed are 1, SkipText is
0, TextSpacing is 6, and the inspected button/text/color cosmetic settings are
default. These are whole-profile conditions, not per-character fallbacks.

Late message authority owns character/delay progression, page/DONE transitions,
logical SFX, END icon/DoAction, once-per-icon flash evolution and shared stateTimer
increment. Message_Update retains opening, growth, decode, input skip, page/close
handling and cleanup. Shared icon statics retain their original process lifetime.

The transaction remains input -> world/actor/message/interface update using
prior-draw contacts -> actor draw/pose/RNG -> HUD preamble -> countdown authority
and packet -> HUD remainder -> message authority and entry-state packet ->
message presentation -> audio control/mixing -> display-list replay -> snapshot.
The HUD consumes old DoAction before END changes it. No canonical phase labels
or event ordering were changed.

Packets are zero-initialized stack objects used synchronously within one draw.
Countdown packets carry visibility, position/size/scale, digits and color;
message packets carry segments/resources, textbox geometry/color, entry-mode
glyph prefix and positions, scale/shadow/alpha, and once-latched icon state.
View allocation happens once in preparation. Canonical live fields remain the
authority; packets are never reused across transactions.

Actor pose, Player limbs/weapons/contact registration, Keese draw RNG, general
actor mutations, nonadmitted HUD/message branches, ocarina, audio scheduling,
physics, movement, camera and animation remain at their existing seams.
This is not global draw purity or permission to run an unconverted world faster.

## Purity and negative coverage

`NativeSimulationPresentation.h`, `NativeSimulationTest.cpp` and the shared
`NativeSimulationPresentationCoverage.hpp` provide the opt-in direct-helper gate.
Each packet is emitted normally, then twice into independent Gfx/paint storage.
Every call compares complete process-local packet bytes and the declared live
closure: PlayState/message/font/view/input, SaveContext, registers, segment and
DoAction aliases, PadMgr, hidden HUD fields, message statics, semantic actor/RNG/
audio state, draw observations and event sequence. No live context is swapped or
restored. Extra commands never enter rendering/interpolation recording. Ordered
actual Gfx words and paint bytes must match; pointers/padding never enter portable
semantic hashes. Purity receipts attest packet checking and are SHA-bound by
invocation receipts; separate analysis validates them.

The canonical HUD/message suite covers 2,616 countdown packets (1,080 visible,
1,536 invisible) and 564 message packets (546 visible, 18 invisible): 3,180 ordinary
calls plus 6,360 extra calls. Copied-state rejection checks pass seven countdown
cases in 24 processes and thirteen message cases in nine processes, totaling
168 + 117 checks. These are bounded cases, not runtime proof of every CVar or
unsupported profile. The 0x305F suite reports zero admitted message packets.
All presentation-matrix processes also enable direct helper verification.

The graceful negative control changes one live message timer after a scratch call;
the verifier detects it and exits normally with code 2. No native fault is induced.
Invisible countdown packets emit nothing. Source review corrected the earlier
message wording: invisible START/CLOSING still emits legacy segment/setup commands,
but no textbox/glyph/icon paint. Those exact commands must match on every call.

## Complete acceptance

| Gate | Evidence under `build/` | Result |
|---|---|---|
| Original canonical, purity off/on | `pass3b-original-off-03/`, `pass3b-original-on-03/` | 12 fixtures x 3 repetitions per mode; 72 processes, 6,840 measured steps, 6,912 snapshots; full reference traces exact |
| HUD/message canonical, purity off/on | `pass3b-draw-off-03/`, `pass3b-draw-on-03/` | 8 fixtures x 3 repetitions per mode; 48 processes, 4,848 steps, 4,896 snapshots; full reference traces exact |
| Strict draw-state and coupling analysis | Both HUD/message corpora and all four `measured_couplings.json` receipts | All expected runs pass; no phase/state/input/audio/paint relaxation |
| Explicit purity-toggle comparison | `pass3b-evidence/flag-toggle-original-03.json`, `flag-toggle-draw-03.json`, `final-acceptance.json` | 60 off/on pairs have byte-identical complete snapshots and traces |
| Direct CPU-helper purity | `pass3b-draw-on-03/purity-analysis.json`; original and matrix receipts | One normal plus two scratch calls per admitted packet; live state, packet, event/RNG/input and ordered Gfx/paint comparisons pass |
| Original presentation/trace matrix | `pass3b-original-presentation-03/matrix_result.json` | 7 cases x 3 processes = 21; 1,941 exact snapshots; purity enabled |
| HUD/message presentation/trace matrix | `pass3b-draw-presentation-03/matrix_result.json` | 24 cases x 3 processes = 72; 7,344 exact snapshots; purity enabled |
| CLI rejection | `pass3b-cli-03/native-validation.json` | 40/40; no game initialization, fail-fast controller |
| Graceful helper-detector control | `pass3b-purity-control-03/control.json` | Test-only state write detected, normal exit 2; no native fault |
| Copied-output comparator control | `pass3b-negative-03/negative-test.json` | One-bit mismatch at tick 17 detected; restored output exact |
| Ordinary startup/graceful close | `pass3b-default-03/smoke.json` | Window/scene evidence and exit 0; no forced termination or human gameplay claim |
| Startup characterization | `pass3b-evidence/startup-completion-04.json`, referenced campaigns and `pass3b-startup-gravity-03/` | 180 successful short starts (15 per cohort) plus 12 complete gravity replays; one retained prelaunch disk-full failure; zero native exceptions/timeouts |
| Native TLUT regression | `pass3b-tlut-02/tlut-bounds-result.json` | 31 checks; unchanged production helper/dependency |
| Native coverage-counter regression | `pass3b-crash-sol/coverage-regression-01/purity-coverage-result.json` | 20 checks; retained source-identical production-header evidence |
| Python tooling/math/evidence suite | `pass3b-evidence/python-tests-06.log` | 121 tests pass, including seven storage-lifetime checks |
| Source inventory | `pass3b-inventory-final-check/` | Four files byte-identical to committed inventory; no actor conversion claims |


`build/pass3b-evidence/final-acceptance.json` audits the complete receipt set,
source/executable/assets, invocation and purity identities, exact off/on file
pairs, matrix comparisons and inventory. The final local checkpoint receipt
separately binds the pushed documentation commit and clean working tree.
All 2,432 inventoried source-file hashes were rechecked against the current
checkout; the runner-only change does not alter that runtime-source inventory.
The focused review is `focused-source-review.md`; original Message_DrawMain and
Message_DrawText preservation is checked by `legacy-message-source-check.json`.
Review was performed by the primary agent, not a newly delegated independent agent.

## Retained failures and design corrections

The original Pass 3B compiler failure (missing enum include) and the first native
purity exception remain retained. The failed executable was 27,015,680 bytes,
SHA-256 `854b9bd06d49fc5ce09ed2133197b33e05245866f470b80a7e3071eba5ad321b`;
the first attempt in `build/pass3b-draw-on-01/hud-message-gate/run-001` stopped
with 0xE06D7363. Work stopped under the user's crash policy. Authorized Sol repair
located JSON type_error 306: the first helper record was null when value() read
counters. Initializing that record to an object fixed the diagnostic bookkeeping.
Its 20 native checks and six focused repaired replays remain under
`build/pass3b-crash-sol/`; no historical failure was rerun or discarded here.

Focused review then added the missing explicit packet comparison, receipt hashes
and fail-fast CLI dispatch. A completed reviewed-build corpus exposed a tooling
integration failure: strict analysis still expected the old CLI argument list.
The failed report and stopped campaign are retained in `pass3b-evidence/`.
The analyzer now requires the exact flag only for a boolean purity declaration;
no semantic/paint check was relaxed, no golden replaced, and no engine rerun was
needed for that correction. All final gates above pass. Earlier pre-review
successes in `pass3b-original-off-02` and `pass3b-draw-off-01` remain historical,
not substitutes for final-runtime acceptance. No new unexplained native crash
occurred during the resumed pass.

Startup attempt 96 then stopped before process creation with disk-full errno 28.
The first 95 starts remain successful, the failed attempt and 84 unattempted
schedule entries remain in the original incomplete campaign, and no failure
record was replaced. Staged asset hard links had reached 1,024 per original;
fallback copies accumulated 5.07 GiB in successful runs. Those byte-verified
duplicates were removed with a separate retained cleanup manifest. Original
assets and all diagnostic outputs remain.

The runner-only storage correction (`11c6a2f73`) reserves 1 GiB before each launch,
budgets copy fallback and releases only verified staged inputs after engine and
fixture validation. It preserves inputs when that validation fails. A later
cross-run comparison failure still retains outputs and the original assets.
The unchanged executable then completed only the outstanding 85 short starts
in fresh continuation campaigns (8 for the last cohort, 7 for each other cohort),
plus the 12 not-yet-run gravity fixtures. All 85 continuation results also match
their prior-campaign references. The accepted denominator is 180 successful
short starts across the retained campaign and continuations, not a claim that
the interrupted campaign passed. One prelaunch infrastructure failure remains.
No completed gameplay corpus or matrix was repeated for this tooling change.

## Limits, next pass and stop

No human gameplay, pixel equality, GPU completion or audible-output acceptance
was performed. Alternate languages/settings, broad pause/transition/NoUI states,
NPC dialogue, general actor action serializers and full ocarina/combat coverage
remain outside the admitted proof. No 30/60/120-Hz authoritative simulation exists.

Recommended next pass: **Pass 3C, Ultra reasoning: bounded Player pose/weapon-contact
ownership design and reference fixtures**. Identify the complete smallest pilot
dependency closure, preserve prior-draw collision latency, specify admission,
packet/lifetime boundaries and exact oracle coverage before a later High-effort
extraction. Do not start timing primitives, general actor conversion or higher-rate
gameplay until the Phase 2 prerequisites in EXECPLAN are satisfied. Pass 3B stops
at its clean pushed checkpoint.

## Changed-file manifest

Exact tracked file set relative to the starting Pass 3A checkpoint;
paths below are repository-relative. Runtime functions are described above.
The test seam adds PresentationLiveBytes, WritePurity,
CheckAdmissionNegatives, NativeSimTest_Present and
NativeSimRecordPresentationCoverage, with CLI/init/end-frame integration.
Message_CopyPresentationStatics exposes only diagnostic state; shared
Message_AdvanceIconFlashLegacy preserves the original flash arithmetic.
Python changes add strict purity analysis and controls, receipt binding,
matrix forwarding, fail-fast CLI dispatch and regression tests.

- `docs/native-simulation/ARCHITECTURE.md`
- `docs/native-simulation/CONVERSION_LEDGER.md`
- `docs/native-simulation/EXECPLAN.md`
- `docs/native-simulation/KNOWN_DIVERGENCES.md`
- `docs/native-simulation/PASS3B.md`
- `docs/native-simulation/README.md`
- `docs/native-simulation/TESTING.md`
- `docs/native-simulation/inventory/summary.json`
- `docs/native-simulation/inventory/timing-candidates.csv`
- `scripts/native-simulation/analyze_draw_state.py`
- `scripts/native-simulation/analyze_purity.py`
- `scripts/native-simulation/native/purity_coverage.cpp`
- `scripts/native-simulation/presentation_matrix.py`
- `scripts/native-simulation/run_corpus.py`
- `scripts/native-simulation/test_draw_state_analysis.py`
- `scripts/native-simulation/test_native_cli_control.py`
- `scripts/native-simulation/test_purity_analysis.py`
- `scripts/native-simulation/test_replay_storage.py`
- `scripts/native-simulation/validate_native_cli.py`
- `scripts/native-simulation/validate_purity_control.py`
- `scripts/native-simulation/validate_purity_coverage.py`
- `soh/soh/NativeSimulationPresentation.h`
- `soh/soh/NativeSimulationPresentationCoverage.hpp`
- `soh/soh/NativeSimulationTest.cpp`
- `soh/src/code/z_message_PAL.c`
- `soh/src/code/z_parameter.c`
