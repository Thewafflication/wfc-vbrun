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

Every export is called by at least one test (see
`requirements/implementation-status.md`).

- `vbrun_tests` (`TC-0001-library`) compares the export directory
  with the reference inventory. It needs the WFC evidence, so it runs in WFC
  only.
- The call cases (`TC-0023` to `TC-0043`, `TC-NNNN-cases-<family>`) call the exports with the
  arguments in `tests/cases/<family>.cases` and compare the results with
  `.expected`, which holds the installed runtime's answers.
- 22 unit test programs cover the families that need a running program, x87
  operands or registers, or compiled-code frames:
  - `TC-0002-helpers-memory`, `TC-0003-helpers-clock`, `-systemmisc`,
    `-systemmore`, `-settings`, `-errobj`, `-objects`, `-late`, `-foreach`,
    `-midstmt`;
  - the file statements: `-openfiles`, `-filedata`, `-filemisc`, `-owner`;
  - `-regcall`, `-x87`, `-x87math`, `-members`, `-records`, `-frames`,
    `-engine`.

  Their expected values come from the reference: the probe scripts, Visual
  Basic 6 programs built with `VB6.EXE /make`, or its disassembly.
- `vbrun_static_tests` (`TC-0044-static`) links the static library.
- `vbrun_dll_test` (`TC-0045-dll`, in C) loads the DLL and checks its
  version and registration.

## Known gaps

- The object engine (instances of a compiled project's classes, forms,
  predeclared instances) and the p-code engine are not reproduced:
  `VBDllGetClassObject` and the `TipCreateInstance*` calls answer
  `CLASS_E_CLASSNOTAVAILABLE`, `ThunRTMain` runs a program that starts with
  Sub Main only, and `ProcCallEngine`/`MethCallEngine` stop the program
  with a diagnostic. The owner approved this deferral as the compatibility
  profile of `REQ-0002` (2026-11-18). It affects programs compiled to
  p-code and programs that start with a form or create the project's own
  classes.
- `__vbaExceptHandler` and `__vbaResume` jump into the frames of compiled
  code on x86 only; the runtime's helpers report an error as the
  thread's error, not as an exception, so only hardware exceptions
  reach the handler.
- The error numbers of some conversions differ from Microsoft's (see the WFC
  planning notes for the distribution milestone).
