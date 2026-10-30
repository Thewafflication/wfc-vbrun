# Visual Basic 6.0 Runtime Requirements

**Content type:** Project requirement index

**Status:** Proposed

**Reference baseline:** Installed `MSVBVM60.DLL` 6.00.9848 (x86)

| Requirement | Contract | Export count |
| --- | --- | ---: |
| `REQ-0099` | [Binary identity and loader](req-0099-runtime-binary-loader-contract.md) | — |
| `REQ-0100` | [Execution engine and hosting](req-0100-engine-hosting-exports.md) | 58 |
| `REQ-0101` | [Compiler helpers](req-0101-compiler-helper-exports.md) | 334 |
| `REQ-0102` | [Language-runtime services](req-0102-language-runtime-exports.md) | 198 |
| `REQ-0103` | [Member and memory helpers](req-0103-member-memory-exports.md) | 24 |
| `REQ-0104` | [Native numeric support](req-0104-numeric-support-exports.md) | 21 |
| `REQ-0105` | [Export-directory layout](req-0105-export-directory-layout.md) | 635 named |

The retained export directory contains 1,911 slots beginning at ordinal 100;
635 slots are populated and named. Function signatures and calling conventions
are not encoded by PE exports and require compiled reference probes and call-site
inspection before implementation.

> The `evidence/` paths and the `TC-*` test case names in these requirements
> refer to [WFC](https://github.com/Thewafflication/wfc), which keeps the
> reference evidence and runs the test cases (the corpus programs are in
> `tests/corpus` of this repository).
