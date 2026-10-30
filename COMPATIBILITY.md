# wfc-vbrun: compatibility with MSVBVM60.DLL

Target: **MSVBVM60.DLL 6.0.98 (Visual Basic 6.0 SP6)**. Library version: **0.6.0**.

`wfcvbrun.dll` exports the helper functions of the Visual Basic 6.0 runtime
(`MSVBVM60.DLL`) under their original names. The list of what the reference
runtime exports is kept in WFC's evidence (`evidence/reference`); the library
implements 90 of them so far; the others are not implemented yet.

| Item | Count |
| --- | ---: |
| Exports implemented | 90 |
| Exports of the reference runtime | see `src/vbrun_exports.inc` |

## Registration

| Item | Value |
| --- | --- |
| Library key | `Software\WFC\Libraries\vbrun` (version, target, path) |
| COM classes | none (the runtime registers no classes) |

## How it is checked

- `vbrun_tests` compares the exports with the reference inventory (needs the
  WFC evidence, so it runs in WFC only).
- `vbrun_helper_tests` runs the helper families (strings, memory, variants,
  conversions, operators) and the `.cases`/`.expected` pairs in `tests/cases`.
- `vbrun_static_tests` links the static library.
- `vbrun_dll_test` (C) loads the DLL, checks its version and registration.

## Known gaps

- The exports beyond the 90 implemented ones are not implemented yet.
- The error numbers of some conversions differ from Microsoft's (see the WFC
  planning notes, MP-0010).
