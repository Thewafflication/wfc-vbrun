# Identifier map

**Content type:** Project record

**Status:** Complete for the renumbering of 2026-11

The library was part of [WFC](https://github.com/Thewafflection/wfc) and used
WFC's global identifier sequence. It is its own repository now, so its
requirements and test cases are numbered in this library's namespace, from
0001 (WSP `style/artifact-identifiers.md`). WSP says to preserve references to
historical identifiers: this file maps each old identifier to the new one.
Documents outside this repository (WFC planning notes, work logs, closeouts
and the generated requirement documents) may still use the old identifiers.

Identifiers are assigned sequentially and are never reused. Do not renumber
the new ones.

## Requirements

Old requirement numbers were `REQ-0099` to `REQ-0105` in WFC's sequence; new
requirements are `REQ-0001` to `REQ-0007` in the same order.

| Old ID | New ID | Old title | File |
| --- | --- | --- | --- |
| `REQ-0099` | `REQ-0001` | VB6 runtime binary identity and loader contract | `requirements/req-0001-runtime-binary-loader-contract.md` |
| `REQ-0100` | `REQ-0002` | VB6 execution-engine and hosting exports | `requirements/req-0002-engine-hosting-exports.md` |
| `REQ-0101` | `REQ-0003` | VB6 compiler-helper export ABI | `requirements/req-0003-compiler-helper-exports.md` |
| `REQ-0102` | `REQ-0004` | VB6 language-runtime service exports | `requirements/req-0004-language-runtime-exports.md` |
| `REQ-0103` | `REQ-0005` | VB6 member and memory helper exports | `requirements/req-0005-member-memory-exports.md` |
| `REQ-0104` | `REQ-0006` | VB6 native numeric support exports | `requirements/req-0006-numeric-support-exports.md` |
| `REQ-0105` | `REQ-0007` | VB6 runtime export-directory layout | `requirements/req-0007-export-directory-layout.md` |

## Test cases

The old identifiers were CTest names that carried the WFC milestone
(`TC-MP0007-`, `TC-MP0009-`, `TC-MP0010-`). The new CTest name is the new
identifier; the old name is the old title. Mounted in WFC (`lib/wfc-vbrun`),
the CTest name also carries the library name, `wfc-vbrun-TC-NNNN-<name>`
(`WFC_VBRUN_TEST_PREFIX` in `CMakeLists.txt`; empty when this repository is
built alone). Every test case now has a
specification (`tests/tc-NNNN-<short-title>.tex`) and a runner
(`tests/run-tc-NNNN.ps1`).

| Old ID (CTest name) | New ID (CTest name) | Specification |
| --- | --- | --- |
| `TC-MP0007-vbrun-library` | `TC-0001-library` | `tests/tc-0001-library.tex` |
| `TC-MP0009-vbrun-helpers-memory` | `TC-0002-helpers-memory` | `tests/tc-0002-helpers-memory.tex` |
| `TC-MP0009-vbrun-helpers-clock` | `TC-0003-helpers-clock` | `tests/tc-0003-helpers-clock.tex` |
| `TC-MP0009-vbrun-openfiles` | `TC-0004-openfiles` | `tests/tc-0004-openfiles.tex` |
| `TC-MP0009-vbrun-filedata` | `TC-0005-filedata` | `tests/tc-0005-filedata.tex` |
| `TC-MP0009-vbrun-systemmisc` | `TC-0006-systemmisc` | `tests/tc-0006-systemmisc.tex` |
| `TC-MP0009-vbrun-late` | `TC-0007-late` | `tests/tc-0007-late.tex` |
| `TC-MP0009-vbrun-foreach` | `TC-0008-foreach` | `tests/tc-0008-foreach.tex` |
| `TC-MP0009-vbrun-systemmore` | `TC-0009-systemmore` | `tests/tc-0009-systemmore.tex` |
| `TC-MP0009-vbrun-objects` | `TC-0010-objects` | `tests/tc-0010-objects.tex` |
| `TC-MP0009-vbrun-errobj` | `TC-0011-errobj` | `tests/tc-0011-errobj.tex` |
| `TC-MP0009-vbrun-regcall` | `TC-0012-regcall` | `tests/tc-0012-regcall.tex` |
| `TC-MP0009-vbrun-x87` | `TC-0013-x87` | `tests/tc-0013-x87.tex` |
| `TC-MP0009-vbrun-x87math` | `TC-0014-x87math` | `tests/tc-0014-x87math.tex` |
| `TC-MP0009-vbrun-members` | `TC-0015-members` | `tests/tc-0015-members.tex` |
| `TC-MP0009-vbrun-records` | `TC-0016-records` | `tests/tc-0016-records.tex` |
| `TC-MP0009-vbrun-frames` | `TC-0017-frames` | `tests/tc-0017-frames.tex` |
| `TC-MP0009-vbrun-owner` | `TC-0018-owner` | `tests/tc-0018-owner.tex` |
| `TC-MP0009-vbrun-engine` | `TC-0019-engine` | `tests/tc-0019-engine.tex` |
| `TC-MP0009-vbrun-settings` | `TC-0020-settings` | `tests/tc-0020-settings.tex` |
| `TC-MP0009-vbrun-midstmt` | `TC-0021-midstmt` | `tests/tc-0021-midstmt.tex` |
| `TC-MP0009-vbrun-filemisc` | `TC-0022-filemisc` | `tests/tc-0022-filemisc.tex` |
| `TC-MP0009-vbrun-cases-strings` | `TC-0023-cases-strings` | `tests/tc-0023-cases-strings.tex` |
| `TC-MP0009-vbrun-cases-variants` | `TC-0024-cases-variants` | `tests/tc-0024-cases-variants.tex` |
| `TC-MP0009-vbrun-cases-convert` | `TC-0025-cases-convert` | `tests/tc-0025-cases-convert.tex` |
| `TC-MP0009-vbrun-cases-varops` | `TC-0026-cases-varops` | `tests/tc-0026-cases-varops.tex` |
| `TC-MP0009-vbrun-cases-language` | `TC-0027-cases-language` | `tests/tc-0027-cases-language.tex` |
| `TC-MP0009-vbrun-cases-varfuncs` | `TC-0028-cases-varfuncs` | `tests/tc-0028-cases-varfuncs.tex` |
| `TC-MP0009-vbrun-cases-varstrings` | `TC-0029-cases-varstrings` | `tests/tc-0029-cases-varstrings.tex` |
| `TC-MP0009-vbrun-cases-scalars` | `TC-0030-cases-scalars` | `tests/tc-0030-cases-scalars.tex` |
| `TC-MP0009-vbrun-cases-scalars2` | `TC-0031-cases-scalars2` | `tests/tc-0031-cases-scalars2.tex` |
| `TC-MP0009-vbrun-cases-scalars3` | `TC-0032-cases-scalars3` | `tests/tc-0032-cases-scalars3.tex` |
| `TC-MP0009-vbrun-cases-random` | `TC-0033-cases-random` | `tests/tc-0033-cases-random.tex` |
| `TC-MP0009-vbrun-cases-random2` | `TC-0034-cases-random2` | `tests/tc-0034-cases-random2.tex` |
| `TC-MP0009-vbrun-cases-numeric` | `TC-0035-cases-numeric` | `tests/tc-0035-cases-numeric.tex` |
| `TC-MP0009-vbrun-cases-finance` | `TC-0036-cases-finance` | `tests/tc-0036-cases-finance.tex` |
| `TC-MP0009-vbrun-cases-dates` | `TC-0037-cases-dates` | `tests/tc-0037-cases-dates.tex` |
| `TC-MP0009-vbrun-cases-dates2` | `TC-0038-cases-dates2` | `tests/tc-0038-cases-dates2.tex` |
| `TC-MP0009-vbrun-cases-payments` | `TC-0039-cases-payments` | `tests/tc-0039-cases-payments.tex` |
| `TC-MP0009-vbrun-cases-arrays` | `TC-0040-cases-arrays` | `tests/tc-0040-cases-arrays.tex` |
| `TC-MP0009-vbrun-cases-lists` | `TC-0041-cases-lists` | `tests/tc-0041-cases-lists.tex` |
| `TC-MP0009-vbrun-cases-system` | `TC-0042-cases-system` | `tests/tc-0042-cases-system.tex` |
| `TC-MP0009-vbrun-cases-files` | `TC-0043-cases-files` | `tests/tc-0043-cases-files.tex` |
| `TC-MP0010-vbrun-static` | `TC-0044-static` | `tests/tc-0044-static.tex` |
| `TC-MP0010-vbrun-dll` | `TC-0045-dll` | `tests/tc-0045-dll.tex` |

## Other identifiers kept as references to WFC

These name WFC artifacts, not artifacts of this library, and are unchanged:

| Identifier | Meaning |
| --- | --- |
| `ADR-0007` | WFC architecture decision on native packaging and the VB runtime |
| `MP-0001`, `MP-0007`, `MP-0009`, `MP-0010` | WFC milestone plans |
| `evidence/reference/msvbvm60-6.0-runtime.json` | Reference inventory kept by WFC |

The CTest labels `mp-0007`, `mp-0009` and `mp-0010` (WFC milestone labels) were
removed; the labels `unit`, `integration` and `vbrun` remain.
