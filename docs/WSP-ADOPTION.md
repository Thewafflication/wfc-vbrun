# WSP Adoption Record

**Content type:** Project adoption record

**Project:** wfc-vbrun (VBRUN compatibility library)

**WSP release:** 1.4.0

**WSP baseline:** Immutable commit `f009399dd1406219571ac978bfee85e85bbdfeac`

**Submodule path:** `wsp/`

**Pinned commit:** `f009399dd1406219571ac978bfee85e85bbdfeac`

**Status:** Adoption in progress; configuration and gate added, reformat and
traceability open (see the gap analysis)

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
| 80-column physical style | WSP-STYLE-0001 | Open: 21 C++ files, 419 clang-format findings; 100 over-long lines (90 in `scripts/vb6_harness_gen.py`, 4 in `ci.yml`, 2 in a test) | Reformat in a follow-up |
| PowerShell analysis | WSP-LANG-0007 | Added (`PSScriptAnalyzerSettings.psd1`); scripts pass | None |
| Commit gate (lint, build, test) | WSP-CHECK-0001 to 0008 | Added `.pre-commit-config.yaml` and `scripts/Invoke-{Lint,Build,Tests}.ps1`; lint is red until the reformat | Reformat, then `pre-commit install` |
| CI runs the same commands | WSP-CHECK-0002 | Open: `ci.yml` calls cmake directly, no lint job | Call the scripts; add a lint job |
| Requirements | WSP-REQM-0001 to 0005 | Partly: `REQ-0099` to `REQ-0105`, moved from WFC, status Proposed | Number policy (decision) |
| Test cases and traceability | WSP-REQM-0005, `Test-Traceability.ps1` | Open: tests are CTest names `TC-MP00NN-*`, no `tc-NNNN` specifications; the tool finds none | Define `TC-` specs and links (decision) |
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
| Requirements | Adopted: `requirements/` (moved from WFC) |
| Traceability check | Planned (roadmap, WSP track); see the gap table |
| Signing | Planned for the first release |
