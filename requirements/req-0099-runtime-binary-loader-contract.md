# REQ-0099 — VB6 runtime binary identity and loader contract

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

The WFC VB6 runtime compatibility component shall provide an x86 PE DLL that
can satisfy references to `MSVBVM60.DLL`, load in the Windows GUI subsystem,
and provide the export ABI required by native VB6 executables. Cross-architecture
WFC shall not run x86 code on x64 or ARM64 through a bridge, helper process or
emulation (owner decision, 2026-10-12, `ADR-0007`); programs for those
architectures are generated natively.

## Reference Binary

| Item | Observed value |
| --- | --- |
| SHA-256 | `898288BD3B21D0E7D5F406DF2E0B69A5BBFA4F241BAF29A2CDF8A3CF4D4619F2` |
| File/product version | `6.00.9848` / `6.00.9848` |
| Size | 1436032 bytes |
| PE machine | `0x014C` (x86) |
| Image base | `0x66000000` |
| Subsystem | 2 (Windows GUI) |
| Authenticode status | Valid |

The observed imports are: `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `ADVAPI32.dll`, `ole32.dll`, `OLEAUT32.dll`.

## Rationale

Native-code VB6 programs import runtime entry points directly. Their
names, ordinals, calling conventions, data layouts, and behavior therefore
form a binary compatibility boundary independent of the VBA object model.

## Verification

**Method:** Inspection and test

**References:** `evidence/reference/msvbvm60-6.0-runtime.json`; test case TBD

Build and load the compatibility DLL from a minimal x86 native executable, inspect its PE headers, and verify that supported x64 and ARM64 hosts use the selected compatibility bridge.

## Relationships

- **Derived from:** MP-0001 compatibility discovery objective
- **Depends on:** WCRT integration and VB execution-engine architecture
- **Conflicts with:** None known

## Tailoring

An export or behavior may be deferred only through an approved compatibility
profile that identifies the affected compiled VB6 programs.

## Implementation Record

`wfcvbrun.dll` (target `wfc_vbrun`, `lib/wfc-vbrun/`) is built for the architecture
of the build and loads in a GUI-subsystem process. It does not carry the
reference identity (hash, version, image base); it is a compatible export
surface, not a copy. Every export is a stub (`ADR-0007`): it writes
`wfcvbrun: runtime export <name> (ordinal N) is not implemented` to stderr and
the debugger and ends the process with exit code `0x5642`, because the
signatures are not known. There is no x86 helper process for x64 and ARM64 hosts, by decision. Verified by `TC-MP0007-vbrun-library`.
