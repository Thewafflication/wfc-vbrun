# wfc-vbrun roadmap

Library: **VBRUN compatibility library** (`wfcvbrun.dll`), the WFC replacement for `MSVBVM60.DLL`.
Current version **0.6.0**, targeting **MSVBVM60.DLL 6.0.98 (Visual Basic 6.0 SP6)**.

The version stays `0.<target major>.<target minor>` until the library matches
the Microsoft one functionally; then it takes the Microsoft version. A checked
item is present, tested and passing in CI; an unchecked one is planned.

## Tracks

| Track | Goal |
| --- | --- |
| Microsoft compatibility | the exported entry points and the behaviour match the Microsoft component; measured in [COMPATIBILITY.md](COMPATIBILITY.md) |
| Packaging | a WPM package of its own, registered with Windows |
| Platforms | Windows x86, x64 and ARM64; Linux (x86, x64, ARM) through Winelib; WebAssembly |
| Toolchain | built with WFC's own C++ compiler and C++ library |
| WSP | the WSP baseline: requirements, style, hardening, traceability |

## 0.6.x: where it is

- [x] Version and target stamped into the DLL resource (0.6.0).
- [x] `DllRegisterServer` and `DllUnregisterServer` (WFC ProgIDs and class ids;
      the Microsoft ids on request).
- [x] A C program tests the DLL (exports, version, registration round trip).
- [x] Standalone build (tested locally on x64).
- [ ] CI on Windows x86, x64 and ARM64 (the workflow is in place; its first
      run is pending).
- [x] WSP baseline as a submodule, WSP hardening flags in the build.
- [ ] Screenshots and an example program in the README (done for the
      controls; see below).

## 0.7: the library creates its own controls

- [ ] `DllGetClassObject` returns a class factory; the control is created
      without the WFC evaluator.
- [ ] The behaviour moves out of the evaluator (`include/wfc/vbrun/vbrun.inc`)
      into the library.
- [ ] A C test creates the control through COM and calls a few members.

## 0.8: Microsoft ids verified

- [ ] Every class id and ProgID checked against a machine with the Microsoft
      component installed.
- [ ] Registration over the Microsoft ids restores what was there (tested for
      `vbrun` against the real component).
- [ ] A program built against the Microsoft type library loads this library.

## 0.9: behaviour parity

- [ ] Every member of [COMPATIBILITY.md](COMPATIBILITY.md) marked *done* or
      *not applicable*, each with a test.
- [ ] Differences from the Microsoft behaviour documented.

## 1.0: Microsoft parity

- [ ] Version takes the Microsoft version number.
- [ ] Signed release, WPM package and release notes.

## Platform track

- [ ] Windows x86, x64, ARM64 (MSVC) passing in CI.
- [ ] Linux x86, x64 and ARM through Winelib (`cmake/winelib.cmake`): x64 and x86
      build and pass in CI; arm64 builds, but Wine 9 hangs when it runs the
      tests, so they are not run there yet.
- [ ] WebAssembly with Emscripten: not possible for this library (it is made of Windows API calls); the static helpers move to a portable core first.
- [ ] A browser demo of the example program.

## Toolchain track

- [ ] Build with WFC's own C++ compiler and C++ library. Blocked: neither
      exists yet. Until then the library builds with MSVC, and with GCC or
      Clang through Winelib and Emscripten.

## WSP track

- [x] WSP 1.4.0 as a submodule (`wsp/`), adoption record in
      [docs/WSP-ADOPTION.md](docs/WSP-ADOPTION.md).
- [x] WSP hardening flags (WSP-SEC-0015) in the standalone build.
- [ ] 80-column source style (WSP-STYLE-0001) for the library sources; the
      generated tables are exempt. CI reports the findings meanwhile.
- [ ] Requirements and traceability (`requirements/`, test case identifiers).
- [ ] Windows code signing and the version resource review.
