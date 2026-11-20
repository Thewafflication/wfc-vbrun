# wfc-vbrun Repository Instructions

This is the VBRUN compatibility library of WFC. It is checked out as a
submodule of WFC (`lib/wfc-vbrun`); the WSP practices it follows are pinned in
`wsp/` and recorded in `docs/WSP-ADOPTION.md`.

## Owner Authorization

- Ordinary file edits within this repository and the current task scope do not
  require asking the owner for approval each time.
- Running the local lint, configure, build and test tooling for x64 Debug
  (`cmake`, `ctest`, `scripts/Invoke-*.ps1`) is pre-authorized.
- Run only the x64 configure, build and test suite locally. Do not run local
  x86 builds/tests or ARM64 cross-builds; GitHub Actions validates those
  targets after a push.
- Commit with plain `git commit` of named paths. The pre-commit hook
  (`.pre-commit-config.yaml`: lint, x64 Debug build, full test suite) is the
  gate; do not run those stages by hand in place of it and never skip it with
  `--no-verify`.
- Pushing, tagging and releasing (`VERSION`, tag, `release.yml`) happen only
  when the owner asks. This repository is also a WFC submodule: changing the
  submodule pointer in WFC is a separate WFC commit.

## Style and Checks

- Controlled formatters: clang-format 22.1.3 (`.clang-format`), gersemi
  0.29.2 (`.gersemirc`), `.editorconfig`; install with
  `python -m pip install --requirement requirements-dev.txt`.
- Physical style is checked by `wsp/tools/Test-SourceStyle.ps1` (80 columns).
  Generated files (`src/vbrun_exports.inc`, `src/wfcvbrun.def`) are produced
  by `scripts/New-VbrunExports.ps1`; edit `src/implemented.txt`, not them.
- Do not modify the `wsp/` submodule; update its pin only when moving the
  whole project to a newer WSP release.

## Requirements and Tests

- Requirements are `requirements/req-NNNN-*.md` (`REQ-NNNN`); identifiers are
  stable and never reused (WSP `artifact-identifiers.md`).
- Test cases are `TC-NNNN`: each CTest test is named `TC-NNNN-<short-name>`
  and has a specification `tests/tc-NNNN-<short-name>.tex` and a runner
  `tests/run-tc-NNNN.ps1`. The library numbers its own requirements and test
  cases from 0001; `docs/id-map.md` maps the identifiers it had in WFC.
- When another project mounts the library (the WFC submodule `lib/wfc-vbrun`),
  CMake prefixes every CTest name with `wfc-vbrun-` (`WFC_VBRUN_TEST_PREFIX`,
  empty standalone), for example `wfc-vbrun-TC-0007-late`. The TC-NNNN
  identifiers never change; filter with `-R "TC-0007-"` or the per-case runner.
- `pwsh -NoProfile -File scripts/Test-Traceability.ps1` runs the WSP
  traceability check (requirement to test specification to runner).
- The reference call cases are in `tests/cases/` (see its README).
