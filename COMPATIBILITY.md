# wfc-vbrun: compatibility with MSVBVM60.DLL

Target: **MSVBVM60.DLL 6.0.98 (Visual Basic 6.0 SP6)**. Library version: **0.6.0**.

`wfcvbrun.dll` exports the helper functions of the Visual Basic 6.0 runtime
(`MSVBVM60.DLL`) under their original names. The list of what the reference
runtime exports is kept in WFC's evidence (`evidence/reference`); the library
implements all 635 of them.

| Item | Count |
| --- | ---: |
| Exports implemented | 635 |
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

- The object engine (instances of a compiled project's classes, forms,
  predeclared instances) and the p-code engine are not reproduced:
  `VBDllGetClassObject` and the `TipCreateInstance*` calls answer
  `CLASS_E_CLASSNOTAVAILABLE`, `ThunRTMain` runs a program that starts with
  Sub Main only, and `ProcCallEngine`/`MethCallEngine` stop the program
  with a diagnostic.
- `__vbaExceptHandler` and `__vbaResume` jump into the frames of compiled
  code on x86 only; the runtime's helpers report an error as the
  thread's error, not as an exception, so only hardware exceptions
  reach the handler.
- The error numbers of some conversions differ from Microsoft's (see the WFC
  planning notes, MP-0010).
