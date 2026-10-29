// REQ-0101: conversions between Strings and numbers, and the length of a
// String (Len, LenB).
//
// The conversions are the Automation ones (VarI4FromStr, VarBstrFromI4, ...)
// with the user's locale, as the reference calls them; a failure is a Visual
// Basic error of the thread (vbrun::raise_hresult) and the helper returns 0.

#include <cstdint>

#include "vbrun_helpers.hpp"

namespace {

// The locale the reference uses: the user's, or US English when there is none.
LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

}  // namespace

extern "C" {

// Len(text) and LenB(text).
LONG WINAPI __vbaLenBstr(const BSTR text) {
    return text == nullptr ? 0 : static_cast<LONG>(SysStringLen(text));
}

LONG WINAPI __vbaLenBstrB(const BSTR text) {
    return text == nullptr ? 0 : static_cast<LONG>(SysStringByteLen(text));
}

// CLng(text), CInt(text), CByte(text).
LONG WINAPI __vbaI4Str(const BSTR text) {
    LONG value = 0;
    const HRESULT result = VarI4FromStr(text, language_locale(), 0, &value);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return 0;
    }
    return value;
}

SHORT WINAPI __vbaI2Str(const BSTR text) {
    SHORT value = 0;
    const HRESULT result = VarI2FromStr(text, language_locale(), 0, &value);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return 0;
    }
    return value;
}

// A value outside 0 to 255 is an overflow (error 6), whatever the text.
BYTE WINAPI __vbaUI1Str(const BSTR text) {
    const auto value = static_cast<std::uint16_t>(__vbaI2Str(text));
    if (value > 0xFFU) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    return static_cast<BYTE>(value);
}

// CStr(number).
BSTR WINAPI __vbaStrI4(const LONG value) {
    BSTR text = nullptr;
    const HRESULT result = VarBstrFromI4(value, language_locale(), 0, &text);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return nullptr;
    }
    return text;
}

BSTR WINAPI __vbaStrUI1(const BYTE value) {
    BSTR text = nullptr;
    const HRESULT result = VarBstrFromUI1(value, language_locale(), 0, &text);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return nullptr;
    }
    return text;
}

}  // extern "C"
