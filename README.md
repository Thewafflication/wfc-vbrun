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

## Screenshots

The runtime has no window of its own, so there is nothing to photograph. The
test program prints what it checks:

```
vbrun: exports, version 0.6.0 and registration are right
```

## Roadmap, compatibility and platforms

- [ROADMAP.md](ROADMAP.md): milestones from 0.6.x to Microsoft parity,
  including the platform and toolchain tracks.
- [COMPATIBILITY.md](COMPATIBILITY.md): how far the library matches the
  Microsoft component.
- [docs/WSP-ADOPTION.md](docs/WSP-ADOPTION.md): the WSP baseline the library
  follows (`wsp/` submodule).

| Platform | Status |
| --- | --- |
| Windows x86, x64, ARM64 (MSVC) | CI workflow in place; first run pending |
| Linux x86, x64, ARM through Winelib | experimental CI job; `cmake/winelib.cmake` |
| WebAssembly (Emscripten) | experimental CI job; not possible for this library yet |
| WFC's own C++ compiler and library | planned; neither exists yet |

## Building on its own

```
git clone --recurse-submodules https://github.com/Thewafflication/wfc-vbrun
cmake -S wfc-vbrun -B out/build
cmake --build out/build --config Debug
ctest --test-dir out/build -C Debug
```

Under Winelib: `-DCMAKE_TOOLCHAIN_FILE=cmake/winelib.cmake
-DWFC_WARNINGS_AS_ERRORS=OFF`. For WebAssembly use `emcmake cmake`.

## Version and target

The version is in `VERSION` and the Microsoft component it stands in for in
`TARGET`: **vbrun** targets MSVBVM60.DLL 6.0.98 (Visual Basic 6.0 SP6). The version stays
`0.<target major>.<target minor>` until the library matches the Microsoft one
functionally; then it takes the Microsoft version. Both are stamped into the
DLL's version resource (`FileVersion`, `Comments`).

## Registration with Windows

`regsvr32 wfcvbrun.dll` (`/u` to remove) writes `Software\WFC\Libraries\vbrun`
(version, target, path). `WFC_REGISTER_SCOPE=user|machine` picks the registry
(default: the machine, the user when that is not writable). The runtime does not create objects through COM yet.

Each library is also a WPM package of its own (`wfc-vbrun`), whose install script
copies the DLL to `lib\wfcvbrun\<version>\<arch>` of the WFC installation and
registers it, so dynamically linked programs get an update by installing that
package alone.

Part of [WFC](https://github.com/Thewafflication/wfc), where it is mounted at
`lib/wfc-vbrun`. Licence: GPL-3.0-or-later (see the WFC `LICENSE`).
