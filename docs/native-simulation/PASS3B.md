# Pass 3B interrupted checkpoint — NOT ACCEPTED

Date: 2026-10-03. Base: `02075db9d7edf6b3b65abe8cfd0b1e12237b0a4e`.

The bounded countdown/message extraction and direct helper verifier are implemented
but incomplete and unaccepted. Work initially stopped at the first native exception
under the user's crash-stop policy. The subsequent authorized Sol repair below is
validated; full Pass 3B acceptance remains pending. No higher-rate simulation was added.

## Evidence obtained before the stop

- Python suite: 110 tests passed (`build/pass3b-evidence/python-tests-01.log`).
- First build: missing enum include in test code; compiler failure preserved in
  `build/pass3b-evidence/build-01/`. The include was corrected.
- Subsequent Release build: exit 0; receipt/log in
  `build/native-simulation-evidence/build-invocation.json` and `build.log`.
- Executable: `x64/Release/soh.exe`, 27,015,680 bytes, SHA-256
  `854b9bd06d49fc5ce09ed2133197b33e05245866f470b80a7e3071eba5ad321b`.
- Purity disabled: all eight HUD/message fixtures, three fresh runs each,
  passed and matched preserved Pass 3A snapshots and full traces exactly.
  Receipt: `build/pass3b-draw-off-01/corpus_result.json`; reference:
  `build/native-simulation-draw-state-02`.
- No human gameplay acceptance occurred.

## Mandatory stop

The first purity-enabled fixture (`hud-message-gate`, run-001) exited with
native exception code `3765269347` (`0xE06D7363`). The user also reported the
game's crash dialog. The runner stopped after this first failed attempt.
No second attempt, deliberate crash, crash debugging or repair was performed
after the report. The exception code alone does not establish a root cause.

Preserve `build/pass3b-draw-on-01/` in full, including `process.log`, invocation,
runtime logs and partial output. The mutation-control option was **not** enabled
for this run. Direct helper purity is not proven.

## Pending work

The crash-only repair is complete. Stop here for the requested model switch;
after the user resumes the implementation pass, complete purity on/off original
and HUD/message corpora, strict draw/coupling
analysis, both presentation matrices, CLI checks, graceful mutation control,
ordinary startup/stress and TLUT regression. Complete the focused source review,
inventory refresh and final documentation. The current implementation and test
utilities must not be described as accepted until all required gates pass.

The existing late ordering, packet/admission design and remaining presentation
authority are described in ARCHITECTURE.md. Those implementation descriptions
are not substitutes for the outstanding validation.

## Authorized Sol crash repair — validated, 2026-10-03

Starting source checkpoint: `4f03d7e8a079b69fa0edcb466344bef1d6e1b621`.
The preserved runtime stack locates the exception in `json::value`, called by
NativeSimTest_Present while recording its first helper's coverage. A missing
helper key yields JSON null; value() requires an object even when given a fallback.
The installed library throws type_error 306 for this case. The root cause is
diagnostic coverage bookkeeping.

`NativeSimRecordPresentationCoverage` now initializes each new null record to an
object before reading counters. Existing counters and verification checks remain
intact. Its production header is shared with a standalone native regression.
No HUD/message authority, renderer, audio, RNG, input, TLUT or submodule code
changed in the repair. The historical failing executable was not rerun.

Validation:

- Native production-counter regression: 20 checks passed, covering both helper
  records, first invisible/zero-command call, repeated accumulation, separate
  setup/measured and visible/invisible totals, and serialization.
- Python tooling suite: 110 tests passed.
- Repaired Release build: exit 0, unchanged dependency revision.
- Affected hud-message-gate fixture: three fresh traced processes and three
  trace-disabled processes, all with purity verification enabled, all exit 0.
  All 966 snapshots match the preserved passing reference exactly; traced runs
  also match the complete 11,861-record reference trace. Both helpers have
  measured visible/invisible coverage, two extra calls per packet, and 7 countdown
  plus 13 message admission-negative checks in each process. No new exception.

Executable: `x64/Release/soh.exe`, 27,016,704 bytes, SHA-256
`28fc7f76bc87db9f193c1b06a21be85d3d8c97dd97ca78548ea0a29f84234404`.
The build used the starting checkpoint plus the source changes bound by file
hashes in `build/pass3b-crash-sol/crash-resolution.json`; its embedded Git stamp
alone does not identify those changes. That receipt, the build-01 logs, source
review, regression receipt, replay-trace and replay-no-trace corpora remain local
under the ignored evidence directory. The executable/PDB are preserved there.
Original failed evidence remains under `build/pass3b-draw-on-01/`.

This validates the identified crash repair and the affected fixture. It does not
replace the full outstanding Pass 3B acceptance set. No human gameplay was tested.
