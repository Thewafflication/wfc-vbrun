// REQ-0102: Mid and MidB, Space on a Variant, IsDate, RGB and QBColor.
//
// Written from what the installed MSVBVM60 6.00.9848 answers (the call cases
// of tests/cases/language.cases): the error paths raise a Visual Basic error
// for the thread (vbrun::raise) and the helper returns an empty result.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kTypeMismatch = 13;
constexpr int kOutOfStringSpace = 14;

BSTR empty_string() {
    BSTR text = SysAllocStringByteLen(nullptr, 0);
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
    }
    return text;
}

// The length argument of Mid: a missing one (an error Variant) takes the rest
// of the String; a whole number is the length; anything else is refused.
// Returns false after raising.
bool read_length(const VARIANT* const value, std::int64_t& length) {
    const unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    const bool by_reference = (value->vt & VT_BYREF) != 0;
    switch (vt) {
        case VT_ERROR:
            length = -1;
            return true;
        case VT_I2:
            length = by_reference ? *value->piVal : value->iVal;
            break;
        case VT_I4:
            length = by_reference ? *value->plVal : value->lVal;
            break;
        default:
            static_cast<void>(vbrun::raise(kTypeMismatch));
            return false;
    }
    if (length < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    return true;
}

}  // namespace

extern "C" {

// Mid(text, start, length): characters. `length` is a Variant that may be
// missing.
BSTR WINAPI rtcMidCharBstr(const BSTR text, const LONG start,
                           const VARIANT* length) {
    std::int64_t count = 0;
    if (start < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    if (!read_length(length, count)) {
        return nullptr;
    }
    const std::int64_t have = text == nullptr ? 0 : SysStringLen(text);
    const std::int64_t first = static_cast<std::int64_t>(start) - 1;
    if (first >= have) {
        return empty_string();
    }
    const std::int64_t take =
        count < 0 ? have - first : std::min(count, have - first);
    return take == 0 ? empty_string()
                     : SysAllocStringLen(text + first, static_cast<UINT>(take));
}

// MidB(text, start, length): bytes (an odd count leaves a last byte alone).
BSTR WINAPI rtcMidBstr(const BSTR text, const LONG start,
                       const VARIANT* length) {
    std::int64_t count = 0;
    if (start < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    if (!read_length(length, count)) {
        return nullptr;
    }
    const std::int64_t have =
        text == nullptr ? 0 : static_cast<std::int64_t>(SysStringByteLen(text));
    const std::int64_t first = static_cast<std::int64_t>(start) - 1;
    if (first >= have) {
        return empty_string();
    }
    const std::int64_t take =
        count < 0 ? have - first : std::min(count, have - first);
    return take == 0 ? empty_string()
                     : SysAllocStringByteLen(
                           reinterpret_cast<const char*>(text) + first,
                           static_cast<UINT>(take));
}

// Space(count) with a Variant result.
VARIANT* WINAPI rtcSpaceVar(VARIANT* const result, const LONG count) {
    if (count < 0 || count > 0x3FFFFFFF) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    BSTR text = SysAllocStringLen(nullptr, static_cast<UINT>(count));
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
        return result;
    }
    std::wmemset(text, L' ', static_cast<std::size_t>(count));
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal = text;
    return result;
}

// IsDate: a Date, or a String that reads as a date or a time (an Empty, a
// Null, a number and an empty String do not).
SHORT WINAPI rtcIsDate(const VARIANT* value) {
    const unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    const bool by_reference = (value->vt & VT_BYREF) != 0;
    if (vt == VT_DATE) {
        return -1;
    }
    if (vt != VT_BSTR) {
        return 0;
    }
    const BSTR text =
        by_reference ? (value->pbstrVal != nullptr ? *value->pbstrVal : nullptr)
                     : value->bstrVal;
    if (text == nullptr || SysStringLen(text) == 0U) {
        return 0;
    }
    DATE parsed = 0.0;
    const LCID locale =
        GetUserDefaultLCID() != 0 ? GetUserDefaultLCID() : 0x409;
    return SUCCEEDED(VarDateFromStr(text, locale, 0, &parsed)) ? -1 : 0;
}

// RGB(red, green, blue): each part is limited to 255.
LONG WINAPI rtcRgb(const LONG red, const LONG green, const LONG blue) {
    if (red < 0 || green < 0 || blue < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0;
    }
    const LONG r = std::min(red, 255L);
    const LONG g = std::min(green, 255L);
    const LONG b = std::min(blue, 255L);
    return r | (g << 8) | (b << 16);
}

// QBColor(number): the sixteen colours of QuickBASIC.
LONG WINAPI rtcQBColor(const LONG number) {
    static const LONG colors[16] = {0x000000, 0x800000, 0x008000, 0x808000,
                                    0x000080, 0x800080, 0x008080, 0xC0C0C0,
                                    0x808080, 0xFF0000, 0x00FF00, 0xFFFF00,
                                    0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF};
    if (number < 0 || number > 15) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0;
    }
    return colors[number];
}

}  // extern "C"
