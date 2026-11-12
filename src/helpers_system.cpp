// REQ-0103: Environ.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/system.cases). A name that is not set gives an empty string; a
// number selects the entry "NAME=value" of that position (a number below 1 is
// error 5).

#include <string>
#include <vector>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kInvalidUseOfNull = 94;

// The text of Environ(name or number); false after raising an error.
bool environment_text(const VARIANT* const argument, std::wstring& text) {
    const unsigned vt = argument->vt & ~static_cast<unsigned>(VT_BYREF);
    if (vt == VT_NULL) {
        static_cast<void>(vbrun::raise(kInvalidUseOfNull));
        return false;
    }
    VARIANT converted;
    VariantInit(&converted);
    if (vt == VT_BSTR) {
        VARIANT copy;
        VariantInit(&copy);
        VariantCopyInd(&copy, const_cast<VARIANT*>(argument));
        const std::wstring name(copy.bstrVal == nullptr ? L"" : copy.bstrVal,
                                SysStringLen(copy.bstrVal));
        VariantClear(&copy);
        text.clear();
        if (name.empty()) {
            return true;
        }
        const DWORD needed = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
        if (needed > 1U) {
            std::vector<wchar_t> buffer(needed);
            const DWORD length =
                GetEnvironmentVariableW(name.c_str(), buffer.data(), needed);
            text.assign(buffer.data(), length);
        }
        return true;
    }
    const HRESULT status =
        VariantChangeType(&converted, const_cast<VARIANT*>(argument), 0, VT_I4);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    if (converted.lVal < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    text.clear();
    wchar_t* const block = GetEnvironmentStringsW();
    LONG position = 1;
    for (const wchar_t* at = block; *at != L'\0'; at += wcslen(at) + 1U) {
        if (*at == L'=') {
            continue;  // the entries of the drives' current directories
        }
        if (position++ == converted.lVal) {
            text = at;
            break;
        }
    }
    FreeEnvironmentStringsW(block);
    return true;
}

}  // namespace

extern "C" {

// Environ$(name or number).
BSTR WINAPI rtcEnvironBstr(VARIANT* const argument) {
    std::wstring text;
    if (!environment_text(argument, text)) {
        return nullptr;
    }
    return SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
}

// Environ(name or number) as a Variant.
VARIANT* WINAPI rtcEnvironVar(VARIANT* const result, VARIANT* const argument) {
    std::wstring text;
    if (!environment_text(argument, text)) {
        return result;
    }
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal =
        SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    return result;
}

// Beep.
void WINAPI rtcBeep() {
    MessageBeep(0);
}

// DoEvents: lets the program's windows handle their messages; the result is
// the number of loaded forms, which a library does not have.
SHORT WINAPI rtcDoEvents() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}

// The locale of the host (the user's).
LONG WINAPI rtcGetHostLCID() {
    return static_cast<LONG>(GetUserDefaultLCID());
}

// The calendar: 0 for the Gregorian one.
SHORT WINAPI rtcGetCurrentCalendar() {
    return 0;
}

// A String of one character from an ANSI code (0 to 255), and its Variant
// form.
BSTR WINAPI rtcBstrFromAnsi(const LONG code) {
    if ((code & ~0xFF) != 0) {
        static_cast<void>(vbrun::raise(5));
        return nullptr;
    }
    const char byte = static_cast<char>(code);
    wchar_t wide = 0;
    if (MultiByteToWideChar(CP_ACP, 0, &byte, 1, &wide, 1) != 1) {
        wide = static_cast<wchar_t>(static_cast<unsigned char>(byte));
    }
    return SysAllocStringLen(&wide, 1);
}

VARIANT* WINAPI rtcVarBstrFromAnsi(VARIANT* const result, const LONG code) {
    VariantInit(result);
    BSTR text = rtcBstrFromAnsi(code);
    if (text != nullptr) {
        result->vt = VT_BSTR;
        result->bstrVal = text;
    }
    return result;
}

// A String of one byte (a length of one byte, not one character).
VARIANT* WINAPI rtcVarBstrFromByte(VARIANT* const result, const LONG code) {
    VariantInit(result);
    const char byte = static_cast<char>(code);
    result->bstrVal = SysAllocStringByteLen(&byte, 1);
    if (result->bstrVal == nullptr) {
        static_cast<void>(vbrun::raise(14));
        return result;
    }
    result->vt = VT_BSTR;
    return result;
}

// Command$: the command line after the program name, without the spaces
// that follow the name and otherwise as typed.
BSTR WINAPI rtcCommandBstr() {
    const wchar_t* at = GetCommandLineW();
    if (at == nullptr) {
        return SysAllocStringLen(L"", 0);
    }
    if (*at == L'"') {
        ++at;
        while (*at != 0 && *at != L'"') {
            ++at;
        }
        if (*at == L'"') {
            ++at;
        }
    } else {
        while (*at != 0 && *at != L' ' && *at != L'	') {
            ++at;
        }
    }
    while (*at == L' ' || *at == L'	') {
        ++at;
    }
    return SysAllocString(at);
}

VARIANT* WINAPI rtcCommandVar(VARIANT* const result) {
    VariantInit(result);
    result->bstrVal = rtcCommandBstr();
    result->vt = VT_BSTR;
    return result;
}

}  // extern "C"
