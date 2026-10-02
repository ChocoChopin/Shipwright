# Native simulation experiment

This is a history-preserving Shipwright fork. Read `docs/native-simulation/README.md`,
`EXECPLAN.md`, `ARCHITECTURE.md`, `TIMING_SEMANTICS.md`, and `TESTING.md` there before
timing changes. Use `CONVERSION_LEDGER.md` to claim scope and record evidence.

## Scope and correctness

- Canonical 20-Hz behavior stays available and unchanged. Current higher rendering
  FPS is not higher authoritative simulation. 30/60/120 support is NOT implemented.
- Use one fixed-rate architecture for 20, 30, 60, and 120. No variable gameplay dt,
  scattered rate-specific multipliers, or four separate implementations.
- Do not equate `R_UPDATE_RATE` with the proposed simulation setting. Preserve
  existing menus, slowdowns, audio timing, ordering, and unit conventions explicitly.
- Every timing edit must identify its units and semantic class, its owning layer,
  canonical path, reset/lifetime behavior, and validation fixture. Do not double-scale.
- Keep mathematical bugs, expected resolution consequences, and design ambiguities
  distinct. Record the latter two; do not force all high-rate behavior back to 20 Hz.
- Do not enable an unconverted world at higher rates. Pilot scenarios must be
  explicitly constrained by a capability gate until their dependency closure passes.
- This first pass permits documentation/diagnostics/build infrastructure, not bulk
  actor conversion or gameplay changes. Future work follows the execution plan.

## Repository and asset boundaries

- Work on `mod/native-simulation-rates` or a deliberately named successor; inspect
  branch, HEAD, remotes, status, and submodules before changes. Keep upstream history.
- Never stage/commit/push ROMs, extracted Nintendo content, `.otr`/`.o2r` packages,
  runtime saves, or machine-local build products. Verify ignore rules before copying.
- Local ROMs go in ignored `roms/`; generated files and evidence in ignored `build/`.
  Do not modify or delete the original ROM. Do not upload proprietary test fixtures.
- Use explicit staging paths and inspect staged names/content before committing.
  Keep setup, infrastructure, and architecture commits distinct. Do not modify
  submodule pointers or vendored source without a scoped reason and separate review.
- Fork setup is authorized. No upstream PR, release, or unrelated remote write is
  implied by local implementation work.

## Permanent Windows workspace rules

- Prefer repository-local `.tmp/`, `.test-tmp/`, `.pytest-tmp/`, `build/`, `dist/`
  for temporary/generated/test/cache/disposable development files; keep them ignored.
- Avoid global Windows TEMP/TMP or protected user folders when local paths suffice.
  Set process-local TEMP/TMP to an existing `.tmp/` for build/test subprocesses.
- Invoke the exact Python interpreter (`.venv/Scripts/python.exe` if used); do not
  depend on PowerShell activation. Install dependencies only into the intended env.
- Run pytest with `--basetemp=.pytest-tmp`; independent Python tempfile users need
  local TEMP/TMP too. Do not change normal application temp behavior for tests.
- If PowerShell blocks a `.ps1`, use the underlying executable, `.cmd`, `.bat`, or
  direct `python -m ...`. Never change execution policy or system security to build.
- Before a fix, classify failure as application source, dependency/environment,
  filesystem permission/sandbox, build tooling, or test infrastructure. An access
  failure outside the repo is not evidence of an application defect. Do not repeat
  known failed environment experiments or patch gameplay to accommodate them.
- Make proven workarounds reusable in project-local scripts. Use native Windows,
  bounded commands, and preserve local evidence. No system-wide configuration edits.

## Validation and handoff

- Use `scripts/native-simulation/baseline.py` for repeatable local build phases.
  Read `BASELINE.md` for the exact original build receipt and limitations.
- Run the semantic oracle tests and inventory freshness checks documented in
  `TESTING.md`. These prove tooling/math properties, not gameplay support.
- Record exact source/submodule/toolchain/asset identities, commands, outcomes,
  failure classification, and evidence paths. Compiled, process-started, rendered,
  and human gameplay-accepted are separate claims.
- Keep timing documentation and ledger current in each implementation commit.
  No test harness is claimed until it actually executes the engine and emits traces.
