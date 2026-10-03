# Pass 3B interrupted checkpoint — NOT ACCEPTED

Date: 2026-10-03. Base: `02075db9d7edf6b3b65abe8cfd0b1e12237b0a4e`.

The bounded countdown/message extraction and direct helper verifier are implemented
but incomplete and unaccepted. Work stopped at the first native exception under
the user's crash-stop policy. Do not treat this checkpoint as a playable release
or a completed authority-extraction pass. No higher-rate simulation was added.

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

Wait for explicit user direction on the crash. After authorized resolution:
complete purity on/off original and HUD/message corpora, strict draw/coupling
analysis, both presentation matrices, CLI checks, graceful mutation control,
ordinary startup/stress and TLUT regression. Complete the focused source review,
inventory refresh and final documentation. The current implementation and test
utilities must not be described as accepted until all required gates pass.

The existing late ordering, packet/admission design and remaining presentation
authority are described in ARCHITECTURE.md. Those implementation descriptions
are not substitutes for the outstanding validation.
