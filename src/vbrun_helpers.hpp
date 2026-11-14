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

#include <string>
#include <vector>

namespace vbrun {

// A run-time error of the program (`Err.Number`): remembered for the thread
// that raised it. Returns the HRESULT a helper hands back for the error.
HRESULT raise(int vb_error);

// The error most recently raised on this thread (0 for none).
int last_error();
void clear_error();

// Erl (the line number of the failing statement) and Err.LastDllError (the
// system error code of the last Declare'd call), both per thread.
int erl();
void set_erl(int line);
DWORD last_dll_error();
void set_last_dll_error(DWORD code);

// The Visual Basic error number for an HRESULT (the reference maps a few
// well-known results and keeps the code of `0x800Axxxx` results).
int error_of_hresult(HRESULT result);

// Raises the error for a failed HRESULT; returns the HRESULT to hand back.
HRESULT raise_hresult(HRESULT result);

// The message of a Visual Basic error number (the generic one when there is
// none; null for 0).
const wchar_t* error_message(int number);

// One key press or release of SendKeys: a virtual key, or (unicode) the
// character itself.
struct KeyEvent {
    WORD key;
    bool up;
    bool unicode;
};

// The key events for a SendKeys string (+ shift, ^ control, % alt, ~ enter,
// ( ) groups, {NAME} and {NAME count} for named keys); false when the string
// is not valid (SendKeys then raises error 5).
bool parse_send_keys(const std::wstring& text, std::vector<KeyEvent>& events);

}  // namespace vbrun
