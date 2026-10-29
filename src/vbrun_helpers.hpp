// Shared parts of the VBRUN helpers (REQ-0100 to REQ-0104).
//
// The helpers are written from the disassembly of the installed MSVBVM60
// 6.00.9848 (see requirements/vbrun/implementation-status.md): each one has
// the argument list and the result the reference code has, and the same
// observable effect on its arguments. They are exported under the names and
// ordinals of the reference (wfcvbrun.def); the calling convention is
// `__stdcall` on x86, the only architecture whose binaries import them.

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

namespace vbrun {

// A run-time error of the program (`Err.Number`): remembered for the thread
// that raised it. Returns the HRESULT a helper hands back for the error.
HRESULT raise(int vb_error);

// The error most recently raised on this thread (0 for none).
int last_error();
void clear_error();

// The Visual Basic error number for an HRESULT (the reference maps a few
// well-known results and keeps the code of `0x800Axxxx` results).
int error_of_hresult(HRESULT result);

// Raises the error for a failed HRESULT; returns the HRESULT to hand back.
HRESULT raise_hresult(HRESULT result);

}  // namespace vbrun
