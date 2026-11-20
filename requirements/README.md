# Visual Basic 6.0 Runtime Requirements

**Content type:** Project requirement index

**Status:** Proposed

**Reference baseline:** Installed `MSVBVM60.DLL` 6.00.9848 (x86)

| Requirement | Contract | Export count |
| --- | --- | ---: |
| `REQ-0001` | [Binary identity and loader](req-0001-runtime-binary-loader-contract.md) | — |
| `REQ-0002` | [Execution engine and hosting](req-0002-engine-hosting-exports.md) | 58 |
| `REQ-0003` | [Compiler helpers](req-0003-compiler-helper-exports.md) | 334 |
| `REQ-0004` | [Language-runtime services](req-0004-language-runtime-exports.md) | 198 |
| `REQ-0005` | [Member and memory helpers](req-0005-member-memory-exports.md) | 24 |
| `REQ-0006` | [Native numeric support](req-0006-numeric-support-exports.md) | 21 |
| `REQ-0007` | [Export-directory layout](req-0007-export-directory-layout.md) | 635 named |

The retained export directory contains 1,911 slots beginning at ordinal 100;
635 slots are populated and named. Function signatures and calling conventions
are not encoded by PE exports and require compiled reference probes and call-site
inspection before implementation.

> The `evidence/` paths in these requirements refer to
> [WFC](https://github.com/Thewafflection/wfc), which keeps the reference
> evidence. The requirements and test cases are numbered in this library's own
> namespace (`REQ-NNNN`, `TC-NNNN`, from 0001); [the identifier
> map](../docs/id-map.md) lists the identifiers they had in WFC. The test
> specifications are `tests/tc-NNNN-*.tex`; `scripts/Test-Traceability.ps1`
> checks the links.
