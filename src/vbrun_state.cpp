// The run-time error state of the VBRUN helpers.

#include "vbrun_helpers.hpp"

namespace vbrun {

namespace {

thread_local int g_error = 0;
thread_local int g_erl = 0;
thread_local DWORD g_dll_error = 0;

}  // namespace

HRESULT raise(const int vb_error) {
    g_error = vb_error;
    // The reference turns a Visual Basic error into 0x800A0000 | number (zero
    // stays zero).
    return vb_error == 0 ? S_OK
                         : static_cast<HRESULT>(
                               0x800A0000U | static_cast<unsigned>(vb_error));
}

int last_error() {
    return g_error;
}

void clear_error() {
    g_error = 0;
}

int erl() {
    return g_erl;
}

void set_erl(const int line) {
    g_erl = line;
}

void set_last_dll_error(const DWORD code) {
    g_dll_error = code;
}

DWORD last_dll_error() {
    return g_dll_error;
}

int error_of_hresult(const HRESULT result) {
    if (result >= 0) {
        return 0;
    }
    const auto bits = static_cast<unsigned>(result);
    const auto facility = bits & 0x1FFF0000U;
    if (facility == 0x000A0000U) {
        return static_cast<int>(bits & 0xFFFFU);
    }
    if (facility == 0x00010000U) {
        return 440;  // an Automation error
    }
    switch (result) {
        case E_NOTIMPL:
            return 445;  // object doesn't support this action
        case STG_E_FILENOTFOUND:
            return 432;  // file name or class name not found
        case E_OUTOFMEMORY:
            return 7;
        case E_INVALIDARG:
            return 5;
        case DISP_E_TYPEMISMATCH:
            return 13;
        case DISP_E_OVERFLOW:
            return 6;
        case DISP_E_BADINDEX:
            return 9;
        case DISP_E_PARAMNOTFOUND:
            return 448;  // named argument not found (a missing argument)
        case DISP_E_DIVBYZERO:
            return 11;
        default:
            return 440;
    }
}

HRESULT raise_hresult(const HRESULT result) {
    const int error = error_of_hresult(result);
    if (error == 440) {
        // An Automation error keeps its own HRESULT; the error information
        // of the failed call is dropped (the reference calls SetErrorInfo
        // with nothing).
        static_cast<void>(SetErrorInfo(0, nullptr));
        return result;
    }
    return raise(error);
}

}  // namespace vbrun

extern "C" {

// Erl: the line number of the statement that raised the error most recently.
LONG WINAPI rtcGetErl() {
    return vbrun::erl();
}

// Takes the error code of the last system call of the thread (what a
// Declare'd function with the error flag leaves in Err.LastDllError).
void WINAPI __vbaSetSystemError() {
    vbrun::set_last_dll_error(GetLastError());
}

}  // extern "C"
