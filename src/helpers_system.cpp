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
        const std::wstring name(
            copy.bstrVal == nullptr ? L"" : copy.bstrVal,
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
    const HRESULT status = VariantChangeType(
        &converted, const_cast<VARIANT*>(argument), 0, VT_I4);
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
VARIANT* WINAPI rtcEnvironVar(VARIANT* const result,
                              VARIANT* const argument) {
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

}  // extern "C"
