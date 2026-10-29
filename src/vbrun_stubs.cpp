// The VBRUN compatibility library (ADR-0007, REQ-0099, REQ-0105).
//
// The export directory is that of MSVBVM60 6.00.9848 (ordinal base 100, 635
// named exports in 1,911 slots; see wfcvbrun.def). The signatures of the
// exports are not recorded in the reference inventory, so each one is a stub
// that names itself and stops the process: a native program that reaches a
// helper WFC does not implement fails with a diagnostic instead of running on
// with a corrupt stack.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>

namespace {

constexpr unsigned kUnimplementedExitCode = 0x5642U;  // 'VB'

[[noreturn]] void unimplemented(const unsigned ordinal,
                                const char* const name) {
    char message[160];
    const int length = std::snprintf(
        message, sizeof(message),
        "wfcvbrun: runtime export %s (ordinal %u) is not implemented\n", name,
        ordinal);
    if (length > 0) {
        DWORD written = 0;
        const HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
        if (error != nullptr && error != INVALID_HANDLE_VALUE) {
            static_cast<void>(WriteFile(
                error, message, static_cast<DWORD>(length), &written, nullptr));
        }
        OutputDebugStringA(message);
    }
    ExitProcess(kUnimplementedExitCode);
}

}  // namespace

#define VBRUN_EXPORT(ordinal, name)         \
    extern "C" int vbrun_stub_##ordinal() { \
        unimplemented(ordinal, #name);      \
    }
#include "vbrun_exports.inc"
#undef VBRUN_EXPORT

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}
