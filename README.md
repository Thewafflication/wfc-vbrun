# wfc-vbrun

The VBRUN compatibility library of WFC: an `MSVBVM60.DLL`-compatible export
directory (`wfcvbrun.dll`, ordinal base 100, 635 named exports in 1,911 slots)
with real implementations for the helpers WFC has confirmed against the
installed reference runtime, and stubs that stop with a diagnostic for the rest.

- `src/` library sources; `implemented.txt` lists the exports that have an
  implementation, `wfcvbrun.def` and `vbrun_exports.inc` are generated from it.
- `tests/` the export-directory test, the helper tests and the reference call
  cases (`tests/cases/<family>.cases` with the reference results in
  `.expected`).
- `scripts/` `New-VbrunExports.ps1` regenerates the export files,
  `Probe-Vbrun.ps1` and `Probe-VbrunCases.ps1` probe a 32-bit reference runtime.
- `VERSION` the library version, stamped into the DLL's version resource.

Targets: `wfc_vbrun` (DLL, `wfcvbrun.dll`) and `wfc_vbrun_static` (static
library `wfcvbrun_static.lib`), so a program can link the helpers either way.

Part of [WFC](https://github.com/Thewafflication/wfc), where it is mounted at
`lib/wfc-vbrun`. Licence: GPL-3.0-or-later (see the WFC `LICENSE`).
