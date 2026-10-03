# Pass 3A: startup characterization and bounded draw-authority design

2026-10-03, native Windows, `mod/native-simulation-rates` on
[ChocoChopin/Shipwright](https://github.com/ChocoChopin/Shipwright).
This pass adds observation and acceptance coverage while leaving authoritative
work at its legacy call sites. Native 30/60/120-Hz gameplay is not implemented.
Interactive gameplay and visual/audio acceptance remain unverified.

## Preserved reference and source identity

Pass 2 runtime source is `68cd6a6520dcc9e3c4147fbc9dec62cd2f702417`.
Its immutable local reference is
`build/native-simulation-reference/soh-pass2-final.exe`, 26,969,088 bytes,
SHA-256 `e64ec79a23627f7d288ee0a6e96fbbc701ddefee1a8d403b7f1aa856459dadbf`.
Its matching PDB, earlier crash-era executable, full canonical corpus 04,
failed corpus 02, interrupted corpus 03 and Pass 2 final receipt are retained.
The original successful build log/invocation were copied to
`build/pass3a-evidence/pass2-build.log` and `pass2-build-invocation.json` before
the ordinary build-output paths were reused. [PASS2.md](PASS2.md) remains historical.

The branch started clean at `b0b79271eb4fe9649d020ea86f68ba37a8513b3b`.
The first validated, pushed checkpoints were:

- `11b06ca3d84e05c71db18766d9df85cb43ba2e42`: bounded startup-stress tooling.
- `4e5ec595717dd1008c7d09b6aaab0144bc6bb4c7`: explicit draw-state matrix selection.

The accepted observation runtime/design checkpoint is
`b14ea69cea954685803aa8d334a5424f8bdde190`, pushed to the same fork branch.
Runtime and final gate identities are recorded in the completion receipt below.
The Pass 2 embedded `9c3cd0d` build string and observation candidate's `b0b7927`
string are configure-time stamps, not proof of the complete source compiled into
an executable. Build invocation, runtime source file hashes, executable hash and
committed source must agree independently.
The reference and first observation candidate use libultraship
`62e973aeb4a53ad4d22bb91e2d9373ecdfcd246c`. A subsequently reproduced palette
over-read now has a reviewed, narrowly scoped renderer fix in
[ChocoChopin/libultraship](https://github.com/ChocoChopin/libultraship), branch
`codex/tlut-source-bounds`, commit
`c6bbb8c328938c115f4a1cbeaca3d00a4502269d`, with the root submodule URL/pin updated.
The root repair checkpoint is `2530a3cc4dd1f2222d7c1ddd097ba1b8b55c45ab`.
Rebuilt-game validation passed; exact gates are recorded below and in PASS2's
later repair addendum.
Archive identities remain those recorded in PASS2; no archive was regenerated.

An earlier local build linked the preliminary candidate but was interrupted
during post-build asset copying. Its invocation lacks a successful exit receipt
and is not counted as a passing build. Logs and the explicit interruption record
remain under `build/pass3a-evidence/build-interrupted-01.*` and
`build-interruption-01.json`. The finalized observation build was rerun to full
exit 0, including copying, before its validation processes began.

## Startup reliability: resource-backed palette over-read fixed and validated

The historical corpus 02 gravity failure records access violation `0xc0000005`
in `gfx_load_tlut_handler_rdp`, during the first setup presentation. Its complete
78,223-byte trace is a successful run's prefix, with 399 complete JSON records
and a truncated next record. It contains no measured snapshot. The crash stack
locates the operation after the buffered trace tail; the last written JSON row
does not identify the faulting instruction. The old log lacks the later bounded
palette/source metadata, so its selected pointer and resource lifetime cannot
be reconstructed reliably. Its failed aggregate remains failed.

The later observation candidate reproduced an access violation in the same TLUT
operation during the first setup presentation: `native-simulation-corpus-05`,
`gravity-fall/run-002`, exit `0xc0000005`. That failed corpus has **34 completed
runs of 35 attempted**; gravity run 003 was not attempted. Its executable SHA-256
is `db84991ad1644cea2e91b837feec50b5c1829d865a254439ca8a7655a1d955c1`.
The failed aggregate and all partial output remain retained. This is a source
memory-safety failure before measurement, not a completed timing-semantic mismatch.

The existing bounded crash diagnostics captured a concrete source-extent mismatch:

| Captured item | Value |
|---|---|
| Resource | `objects/object_link_child/gLinkChildSwordTLUT`, width 27, height 4 |
| Command | `w0=0xF0000000`, `w1=0x073FC000`; tile 7, TMEM 256, high index 255 |
| Requested source range | 512 bytes, `[0x0000025956AFDE50, 0x0000025956AFE050)` |
| Declared image range | 216 bytes, `[0x0000025956AFDE50, 0x0000025956AFDF28)`; `requested_fits=0` |
| Retained owned buffer | 296 bytes, `[0x0000025956AFDE00, 0x0000025956AFDF28)` |

The binary factory retains this file buffer through the texture resource. The
request starts at its valid image address but extends 296 bytes past its owned
end. `GfxDpLoadTlut` copies two 256-byte halves without checking that source extent.
This proves the over-read for the reproduced failure; the older corpus 02 log
lacks the metadata needed to prove its precise resource identity retrospectively.
No stale-pointer, resource-cache race, unfinished preload or GPU-driver cause was
established, and none of those hypotheses was patched.

Read-only analysis scanned all 38,415 archive members and found exactly eight
texture-image references: the near/far pairs of child Deku-shield sword/sheath,
Hylian-shield sword/sheath, left-fist/Kokiri-sword, and sword/sheath display lists.
All eight original ROM lists request 256 palette entries. Their audited CI8
consumer is `gLinkChildSwordJewelTex`: all 512 texels use indices 0–106, with no
index at or above 108. The extracted 216-byte palette and jewel texels exactly
match their original ROM spans. The original 512-byte transfer also includes
nonzero adjacent hand/sheath texture bytes; those extra entries are unobserved
by these consumers. Metadata-only receipts are
`build/pass3a-design/tlut-all-archive-references.json`, `tlut-original-span.json`
and `tlut-analysis.json`; no asset payload is committed.

The reviewed fix bounds resource-backed TLUT staging copies by both the declared
image span and the retained buffer span. It preserves every available byte,
zeroes only the unavailable part of the requested write, preserves untouched CI4
palette slots, and leaves the raw-source fallback unchanged. Cache identity uses
checked integer address arithmetic without dereferencing an out-of-object token.
Zero tail is an explicit fallback for bytes absent from the resource, **not**
byte-identical emulation of the original N64 bulk transfer. The audited sword
consumers cannot observe that tail. No gameplay, RNG, audio or archive change is
part of this renderer correction.

Independent source review found no blocker. The ordinary native helper regression
passed 31 checks in `build/native-simulation-tlut-bounds-02/tlut-bounds-result.json`,
superseding the initial 26-check receipt with additional coverage.
No guard-page or intentionally crashing test was performed. Helper success does
not establish renderer/GPU acceptance. Separate fixed-executable validation passed
the original canonical corpus, presentation matrix, native CLI, negative control,
coupling analysis, ordinary startup and the following campaigns:

| Fixed executable receipt under `build/` | Scope | Outcome |
|---|---|---|
| `native-simulation-startup-stress-fixed-01/stress_result.json` | Two scenes x 20/60/120 presentation FPS x trace on/off x 15, one setup + one measured transaction | 180/180 complete; zero native exceptions/timeouts |
| `native-simulation-startup-gravity-fixed-01/stress_result.json` | Original full gravity fixture, 60 setup + 80 measured transactions | 12/12 complete; zero native exceptions/timeouts; exact canonical reference |

Every fixed-build completion positively confirms scene initialization and measured
replay. These 192 runs use the fixed `e3da61b9...` executable and remain separate
from the preserved-build campaigns below. The causal storage bound plus unchanged
canonical results supports the scoped repair; it is not a guarantee against all
renderer/startup failures. Independent source and runtime audits are retained as
`build/pass3a-evidence/crash-source-review-sol.md` and
`crash-runtime-audit-sol.json`. The latter reports no pending checks or issues.

### Completed preserved-build campaigns

| Receipt under `build/` | Scope | Outcome |
|---|---|---|
| `native-simulation-startup-stress-02/stress_result.json` | Kokiri Forest and Link's House x 20/60/120 presentation FPS x trace on/off x 15, one setup + one measured transaction each | 180/180 completed; zero timeouts, native exceptions or TLUT faults |
| `native-simulation-startup-stress-gravity-01/stress_result.json` | Unchanged full gravity fixture, 60 setup + 80 measured transactions, trace on, 20 FPS | 12/12 completed; zero timeouts, native exceptions or TLUT faults |

Both campaigns used exactly the preserved e64 executable bytes at the original
`x64/Release/soh.exe` location. Scene initialization and measurement were positively
confirmed for every completion. There are 168 exact within-cohort comparisons in
the startup matrix and 11 in gravity. The gravity cohort reference also matches
corpus 04 exactly: 81 snapshots and 50,465 full trace records. This is 192 fresh
starts in these two defined campaigns, not a general failure-rate estimate or a
claim that the original fault is gone.

### Separate executable-location-associated stalls

| Retained evidence under `build/` | Outcome |
|---|---|
| `native-simulation-startup-stress-01` | Two 45-second timeouts from the preserved executable's copied `build/` location; third attempt intentionally interrupted; 177 scheduled but not launched |
| `native-simulation-startup-location-probe-01` | Preliminary observation candidate at `x64/Release/soh.exe`: completed |
| `native-simulation-startup-location-probe-02` | e64 copied to `build/pass3a-reference-bin/soh.exe`: 45-second timeout |
| `native-simulation-startup-location-probe-03` | The byte-identical preliminary candidate copied to `build/pass3a-candidate-bin/soh.exe`: 45-second timeout |
| `native-simulation-visible-probe-01` | e64 from the copied path with the show-window launch setting changed: 45-second timeout |
| `native-simulation-startup-location-probe-04` | e64 restored to the original `x64/Release/soh.exe` location: completed |

All five timeout records end at the D3D adapter log without durable scene or
measurement confirmation, and contain no TLUT/exception signature. The visible
probe retains its process-log tail in the invocation receipt rather than a
separate process.log. The first campaign's unchanged aggregate remains `started`;
its hash-bound `interruption.json` records 2 failed, 1 interrupted and 177 untried.
These failures/interruption are not removed from accounting or pooled into a
passing denominator. The preliminary candidate was not the final diagnostics
build; its complete identity and dirty-source provenance are retained in the probes.

The old and preliminary-candidate byte-identical path pairs establish an
association with executable launch location/context. They do not identify the
mechanism or prove location is the only factor. The visible launch also stalled;
no visibility workaround was adopted. An infinite frame-latency wait exists in
the examined DXGI initialization path, but no captured stack locates these
timeouts there. No desktop-lock, driver-configuration or asset-race cause was
established. Future runs must explicitly record and verify the executable path
as well as bytes, using the known working original path for comparisons.

Independent read-only campaign accounting and strict reference comparison are
retained in `build/pass3a-design/startup-evidence-audit.json`; source review is
`startup-review.md` and quantitative review is `startup-results.md` beside it.

## Selected extraction and reviewed order

[ARCHITECTURE.md](ARCHITECTURE.md) contains the field-by-field dependency closure,
aliases, gate/reset behavior, functions, admission predicates and exact High-pass
implementation specification. [TESTING.md](TESTING.md) contains fixture and command
contracts. The selected scope is deliberately narrow:

- **Main countdown:** states DOWN_INIT/PREVIEW/MOVE/TICK/STOP/OFF, hidden second
  and state divisors, main timer XY slide, seconds decrement, retained digits,
  warning selection and logical sound ingress. Preserve previous drawn ones
  digit for warnings and the complete STOP transaction before OFF. Subtimer,
  environmental hazard/countup and trade/minigame special branches remain legacy.
- **Simple message:** English 0x1043, default text behavior, null talk actor,
  NEWLINE/BOX_BREAK/END control profile. Extract the draw-owned character/delay
  walk, page/DONE transitions, logical SFX requests, END's DoAction/icon work and
  the end icon's shared stateTimer increment. Opening, growth, decode, input
  skip/page/close and close cleanup retain their update ownership. Quicktext/fade
  0x305F is a recorded fallback fixture, not an expanded admitted profile.

The canonical order is input -> world update with prior-draw collision state ->
actors/message/interface update -> actor draw/pose/RNG -> HUD preamble -> **late
countdown authority and its packet** -> timer/HUD presentation remainder ->
**late simple-message authority and its entry-state paint packet** -> message
presentation -> audio control/mixing -> display-list replay -> snapshot. The two
bold slots replace the existing operations in place. They are not permission to
move all authority before CPU drawing. HUD must already have consumed the old
DoAction state before END changes it; audio must see the same ordered requests.
The representation remains compatible with one canonical transaction advancing
six quanta on the future 120-unit/second clock. No timer scaling occurs here.

Independent review found and corrected two design gaps: icon flash evolution
must be latched once during packet preparation rather than repeated by a pure
helper, and state-only timer tests would miss omitted timer geometry. Actual
glyph/icon and timer-emission observations now cover those risks. Another review
bounded decoded-buffer observation to a proven live decoded page, avoiding stale
length reads during opening/closing. Getter/source review found no authority move.
The reviewed spec also requires a direct repeated CPU-helper purity test in High;
the current presentation-FPS matrix repeats display lists, not C helpers.

The `hud-zero-input` recipe was corrected to use Kokiri Forest because Link's
House disables the B item action being tested. Three revised probe runs and their
strict observer checks passed in `build/native-simulation-hud-input-probe-01` and
`build/pass3a-design/hud-input-observe-review.json`. They use the pre-fix observation
candidate and do not replace full draw-corpus or corrected-renderer acceptance.

## Completion receipt

The fixed observation executable at `x64/Release/soh.exe` is 26,991,616 bytes,
SHA-256 `e3da61b9397dee5d2e1ef3e168927d9525742bb6626142291853e879ff6864f4`.
Its immutable local copy is `build/native-simulation-reference/soh-pass3a-tlut-fixed.exe`
with the matching `.pdb`. The PDB SHA-256 is
`9e180eb2190e269701f7f86b3c54a6827f482a206f73f1b01ef711307f45cb02`.
The build exited 0; its complete log and invocation are under
`build/pass3a-evidence/fixed-validation-01/`. Native Windows x64 toolchain and
archive identities remain those in PASS2/BASELINE. Torch remains
`2ab12fe9660aec04e02ee89fe81baed304a1a1d6`, vcpkg remains
`3aea538b2bb21a586502c67b00eb474fdd2e3098`, and libultraship is the reviewed
`c6bbb8c328938c115f4a1cbeaca3d00a4502269d` repair.

The build used base `4e5ec595717dd1008c7d09b6aaab0144bc6bb4c7` plus the saved
observation sources and repaired dependency. The six changed/added engine files
and two dependency files are bound by size/SHA-256 in
`build/pass3a-evidence/crash-checkpoint-sol.json`; they were rechecked before
every remaining gate. A later Git checkpoint or the embedded `b0b7927` string
must not be substituted for that recorded build provenance. No runtime source
changed during the remaining validation.
The committed runtime at `b14ea69cea954685803aa8d334a5424f8bdde190` contains
those same validated sources. The final local handoff binding is
`build/pass3a-evidence/pass3a-final-acceptance.json`: final pushed HEAD, runtime
commit, committed blob identities, preserved executable, audited gate receipts,
regenerated inventory and clean working-tree checks are distinct fields.

| Gate | Evidence under `build/` | Result |
|---|---|---|
| Original canonical corpus against preserved corpus 04 | `native-simulation-corpus-06/corpus_result.json` | PASS, 12 x 3 runs, 3,420 measured steps, 3,456 snapshots, exact full traces |
| Original presentation/trace matrix | `native-simulation-presentation-02/matrix_result.json` | PASS, 7 x 3 runs, exact snapshots |
| Native CLI validation | `native-simulation-cli-05/native-validation.json` | PASS, 37/37 |
| Native palette helper | `native-simulation-tlut-bounds-02/tlut-bounds-result.json` | PASS, 31 checks; source-identical helper evidence |
| Original coupling analysis | `native-simulation-corpus-06/measured_couplings.json` | PASS, 36 runs, 10,260 blocks / 5,417,280 samples |
| Copied-output comparator control | `native-simulation-negative-05/negative-test.json` | PASS, one-bit mismatch at tick 17 detected and restoration exact |
| Ordinary startup/graceful close | `native-simulation-default-smoke-03/smoke.json` | PASS, scene/window and exit 0; no human gameplay claim |
| Fixed startup campaigns | `native-simulation-startup-stress-fixed-01/`, `native-simulation-startup-gravity-fixed-01/` | PASS, 180 short starts + 12 full gravity replays; zero native exceptions/timeouts |
| Tooling/math/evidence tests | `pass3a-evidence/draw-validation-01/tests-final.log` | PASS, 105 tests, including 27 draw-evidence checks and two mocked fail-fast controller checks |
| New canonical draw-state corpus | `native-simulation-draw-state-02/corpus_result.json` | PASS, 8 x 3 runs, 2,424 measured steps, 2,448 snapshots |
| Strict phase/state/input/paint analysis | `native-simulation-draw-state-02/draw_state_result.json` | PASS, all 24 complete runs |
| New draw-state coupling analysis | `native-simulation-draw-state-02/measured_couplings.json` | PASS, all 24 complete runs |
| New presentation/trace matrix | `native-simulation-draw-presentation-01/matrix_result.json` | PASS, 24 cases / 72 processes; 7,344 exact snapshot comparisons |
| Inventory regeneration | `pass3a-inventory-final-check/` | PASS, all four files byte-identical; 2,430 source files, all 430 actor claims still unclaimed |

`build/pass3a-evidence/draw-validation-01/acceptance-audit.json` rechecks the
source/executable/assets, retained original gate identities, every new invocation,
fixture, comparison and output. It records 306,945 canonical trace rows and
21,816 requested display-list replays in the traced presentation cases. The
matrix snapshots also match the canonical serialized bytes. This is a local
receipt audit, not an external build attestation. Its initial raw-versus-parsed
hash bookkeeping error and correction are retained beside it; no engine rerun,
fixture change or acceptance relaxation resulted.

The new matrix uses `--fail-fast`; the separately committed tooling checkpoint
`71660df7d` also adds this option to startup campaigns and records the user's
crash-stop policy in AGENTS.md. Its controller tests use mocked process results,
never a deliberately failing engine. Completed startup campaigns were retained
without launching another campaign in this continuation.

The old failed corpus 05, first draw-state acceptance failure and all interrupted
or stalled attempts remain retained. Corrected `hud-zero-input` fixture coverage
is accepted only through the new complete corpus above. The earlier narrow
three-run probe is not substituted for it. No goldens were replaced.

Independent source/regression review and the handoff review in
`build/pass3a-design/handoff-document-review-sol.md` found no remaining design
blocker. They require the future CPU-helper purity gate before accepting the
bounded extraction. Current matrices establish semantic equality across
display-list replay rates, not repeated C-helper purity, GPU completion, pixels,
audible output or human gameplay acceptance.

## Next High pass and stop criteria

Implement only `Interface_IsCountdownProfileAdmitted`,
`Interface_AdvanceCountdownLegacy`, `Interface_DrawCountdownPresentation`,
`Message_IsPlainTextProfileAdmitted`, `Message_AdvancePlainTextLegacy` and
`Message_DrawPlainTextPresentation`, or equivalent names preserving the complete
contracts in ARCHITECTURE.md. Live canonical fields/statics remain authoritative;
immutable paint packets are per transaction. Whole-profile unsupported cases
take the unchanged legacy path from entry. Do not partially mutate then fall back.

Preserve complete old and new canonical traces, state bits, input edges, RNG and
audio-event order. Add the direct helper-repeat/no-live-mutation test and negative
admission tests before accepting extraction. Keep same-executable 20/60/120
presentation and trace-off matrices, original corpus, CLI, negative control,
ordinary startup and startup stress. A changed canonical step/phase, omitted
paint, new SFX/RNG event, wrong STOP lifetime, stale packet, incomplete admission
closure or failed purity gate requires a fix or rollback of the bounded extraction.
Never normalize away fields or bless replacement goldens to obtain equality.

The repaired over-read and separate copied-path stalls retain their startup
receipts and classifications; none grants an exemption for measured semantic
differences. Stop on any new native crash, preserve its logs/partial outputs, and
report it. Wait for user direction before debugging or reproduction, and never
deliberately induce a crash. Use the fail-fast matrix/stress commands in TESTING.md.
Broad paused/transition,
world-freeze, NoUI and alternate-language/enhancement coverage remains unproved;
keep those profiles outside admission until their direct gates are established.

Actor pose/weapon/collision registration, actor draw RNG (including Keese), other
actor/HUD/message mutations, ocarina, audio scheduling and all high-rate physics,
timers and animation remain at their existing seams. No gameplay subsystem or
actor has been converted by Pass 3A. Do not start that High implementation during
this diagnosis/design pass.
