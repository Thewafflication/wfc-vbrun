# WSP Adoption Record

**Content type:** Project adoption record

**Project:** wfc-vbrun (VBRUN compatibility library)

**WSP release:** 1.4.0

**WSP baseline:** Immutable commit `f009399dd1406219571ac978bfee85e85bbdfeac`

**Submodule path:** `wsp/`

**Pinned commit:** `f009399dd1406219571ac978bfee85e85bbdfeac`

**Status:** Adopted; the common practices are in place and the open items are
listed below

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

## Open items

| Item | Disposition |
| --- | --- |
| 80-column style (WSP-STYLE-0001) | CI reports it; the library sources are not yet wrapped (the generated tables are exempt) |
| Requirements and traceability | Planned (roadmap, WSP track) |
| Signing | Planned for the first release |
