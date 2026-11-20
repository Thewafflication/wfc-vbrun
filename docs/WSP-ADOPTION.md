# WSP Adoption Record

**Content type:** Project adoption record

**Project:** wfc-vbrun (VBRUN compatibility library)

**WSP release:** 1.4.0

**WSP baseline:** Immutable commit `f009399dd1406219571ac978bfee85e85bbdfeac`

**Submodule path:** `wsp/`

**Pinned commit:** `f009399dd1406219571ac978bfee85e85bbdfeac`

**Status:** Style, identifiers, traceability and commit gate adopted; CI wiring
and the adoption-record dispositions open (see the gap analysis)

The library is one of the WFC libraries and follows the WFC adoption record
(`planning/wsp-adoption-record.md` in
[WFC](https://github.com/Thewafflication/wfc)); this record covers what the
library repository itself carries.

## Selected profiles

| Profile | Selected | Scope or rationale |
| --- | --- | --- |
| Security/DFS | Yes | Hardening flags (WSP-SEC-0015) from `wsp/tools/cmake/WspBuild.cmake` in the standalone build |
| C source style | No | The sources are C++; the C test programs follow the same layout |
| CMake style | Yes | `CMakeLists.txt` |
| Windows version resources | Yes | `version.rc.in`, one version resource per DLL |
| Windows code signing and Defender | Deferred | Until the first release |
| Documentation | Yes | `README.md`, `ROADMAP.md`, `COMPATIBILITY.md` |

## Gap analysis

Compared with WFC (the model of an adopted project) at WSP 1.4.0. The `wsp/`
submodule already pins 1.4.0, commit `f009399`, the same as WFC.

| Item | WSP source | Status | Action |
| --- | --- | --- | --- |
| WSP pin | `wsp/` submodule | Done: 1.4.0, same commit as WFC | Move with WFC |
| `.clang-format` | WSP-LANG-0002 | Added | Reformat C++ (below) |
| `.gersemirc` | WSP-LANG-0010 | Added; `CMakeLists.txt` formatted | None |
| `.editorconfig` | WSP-STYLE-0002 to 0004 | Added | None |
| Line endings | WSP-STYLE-0002 | Done (`.gitattributes`, LF) | None |
| 80-column physical style | WSP-STYLE-0001 | Done: C++ reformatted; `scripts/capture_web.py` and `render_terminal.py` (identical in every WFC library) and `vb6_harness_gen.py` (Visual Basic source in strings) are excluded from the check | Wrap the shared scripts when all libraries change together |
| PowerShell analysis | WSP-LANG-0007 | Added (`PSScriptAnalyzerSettings.psd1`); scripts pass | None |
| Commit gate (lint, build, test) | WSP-CHECK-0001 to 0008 | Added `.pre-commit-config.yaml` and `scripts/Invoke-{Lint,Build,Tests}.ps1`; the first hook is `wfc-gate-start` (machine-wide lock `wfc-commit-gate.lock`, shared with WFC, and the test progress window, which follows `out/test-progress.log`); lint and tests pass | `pre-commit install` |
| CI runs the same commands | WSP-CHECK-0002 | Open: `ci.yml` calls cmake directly, no lint job | Call the scripts; add a lint job |
| Requirements | WSP-REQM-0001 to 0005 | Done: `REQ-0001` to `REQ-0007` in the library's own namespace (old `REQ-0099` to `REQ-0105`, see `docs/id-map.md`), status Proposed | None |
| Test cases and traceability | WSP-REQM-0005, `Test-Traceability.ps1` | Done: 45 test cases `TC-0001` to `TC-0045` (CTest names `TC-NNNN-<short-name>`), each with `tests/tc-NNNN-*.tex` and `tests/run-tc-NNNN.ps1`; `scripts/Test-Traceability.ps1` passes (7 requirements, 45 specifications, 45 implementations) and runs in lint | Execution evidence and the test report (WSP report template) |
| Requirement history, baseline, dispositions | WSP-REQM-0006 to 0010 | Open: record above has no per-requirement disposition table | Fill the adoption-record template |
| `AGENTS.md` | WFC model | Added | None |
| Windows version resource | WSP-WINRES | Done (`version.rc.in`) | None |
| Hardening | WSP-SEC-0015 | Done (`WspBuild.cmake`) | None |
| Release and versioning | WSP release process | Partly: `VERSION`, tag, shared `release.yml`; README and ROADMAP still say 0.6.0 while `VERSION` is 0.7.0 | Correct the docs |
| Documentation structure | WSP documentation style | Partly: README, ROADMAP, COMPATIBILITY; no `architecture/`, work logs or release-readiness record | As needed |
| Signing | WSP release profile | Deferred to the first release | Planned |

## Open items

| Item | Disposition |
| --- | --- |
| 80-column style (WSP-STYLE-0001) | CI reports it; the library sources are not yet wrapped (the generated tables are exempt) |
| Requirements | Adopted: `requirements/` (moved from WFC, renumbered; `docs/id-map.md`) |
| Traceability check | Adopted: `scripts/Test-Traceability.ps1`, a lint stage. `TC-0001` is registered only in a WFC checkout (needs the MSVBVM60 inventory); its runner skips otherwise |
| Signing | Planned for the first release |

## Test progress window

`scripts/Start-CommitGate.ps1` and `scripts/Show-TestProgress.ps1` are
byte-identical copies of WFC's, so they can move to one shared place. The
window follows `out/test-progress.log`, which `Write-Stage` (lint and build
stages) and `scripts/Invoke-Tests.ps1` (every ctest line) write. Started with
`-Run`, the window needs `-BuildDir out/build` (WFC's default build folder
differs); the gate does not use `-Run`.

## Visuals

The library has no window. `docs/images/windows-x64.png` is a result card of
the real ctest session (`scripts/New-SessionImage.ps1`, `scripts/render_card.py`,
house style). The Linux images are made by CI with the shared
`scripts/render_terminal.py` (a WFC test requires the copies to be identical),
so they keep its look and still show the old test names until CI runs again.
