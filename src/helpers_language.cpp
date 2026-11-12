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

// Mid$(target, start, length) = replacement: overwrites characters of the
// target in place. `fixed` is the length (in characters) of a fixed-length
// String whose buffer is `target`; it is 0 for a String variable, where
// `target` points at the BSTR. Offsets and lengths are bytes here; the
// character forms double them first. Nothing is added to the target: at most
// the shortest of the replacement, the length and the rest of the target is
// copied.
namespace {

constexpr LONG kNoLength = 0x3FFFFFFF;

void mid_statement(const LONG fixed, const BSTR replacement,
                   const LONG length_bytes, const LONG start_bytes,
                   void* const target) {
    LONG target_bytes = 0;
    char* buffer = nullptr;
    if (fixed != 0) {
        const LONG doubled = static_cast<LONG>(static_cast<ULONG>(fixed) * 2U);
        if (doubled < 0 || doubled > kNoLength) {
            static_cast<void>(vbrun::raise(5));
            return;
        }
        target_bytes = doubled;
        buffer = static_cast<char*>(target);
    } else {
        const BSTR text = *static_cast<BSTR*>(target);
        buffer = reinterpret_cast<char*>(text);
        target_bytes =
            text == nullptr ? 0 : static_cast<LONG>(SysStringByteLen(text));
    }
    const ULONG replacement_bytes =
        replacement == nullptr ? 0 : SysStringByteLen(replacement);
    const LONG offset = start_bytes - 1;
    if (offset < 0 || offset > kNoLength || length_bytes < 0 ||
        length_bytes > kNoLength ||
        static_cast<ULONG>(offset) >= static_cast<ULONG>(target_bytes)) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    ULONG count = replacement_bytes;
    count = std::min(count, static_cast<ULONG>(length_bytes));
    count = std::min(count, static_cast<ULONG>(target_bytes - offset));
    if (count != 0) {
        std::memmove(buffer + offset, replacement, count);
    }
}

LONG chars_to_bytes(const LONG count) {
    return count == kNoLength
               ? count
               : static_cast<LONG>(static_cast<ULONG>(count) * 2U);
}

LONG char_start_to_bytes(const LONG start) {
    return static_cast<LONG>(static_cast<ULONG>(start) * 2U - 1U);
}

// The target Variant of Mid$ on a Variant: a String, or one that converts to
// it (the Variant itself is changed). Returns the BSTR slot or null.
BSTR* variant_string_slot(VARIANT* const target) {
    if ((target->vt & VT_BYREF) != 0) {
        if (target->vt != (VT_BYREF | VT_BSTR)) {
            static_cast<void>(vbrun::raise(kTypeMismatch));
            return nullptr;
        }
        return target->pbstrVal;
    }
    if (target->vt == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return nullptr;
    }
    if (target->vt != VT_BSTR) {
        const HRESULT status = VariantChangeTypeEx(
            target, target, GetUserDefaultLCID(), VARIANT_ALPHABOOL, VT_BSTR);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return nullptr;
        }
    }
    return &target->bstrVal;
}

void mid_variant_statement(VARIANT* const replacement, const LONG length_bytes,
                           const LONG start_bytes, VARIANT* const target) {
    BSTR* const slot = variant_string_slot(target);
    if (slot == nullptr) {
        return;
    }
    VARIANT converted;
    VariantInit(&converted);
    const VARIANT* source = replacement;
    if (replacement->vt == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return;
    }
    if (replacement->vt != VT_BSTR) {
        const HRESULT status =
            VariantChangeTypeEx(&converted, replacement, GetUserDefaultLCID(),
                                VARIANT_ALPHABOOL, VT_BSTR);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return;
        }
        source = &converted;
    }
    mid_statement(0, source->bstrVal, length_bytes, start_bytes, slot);
    VariantClear(&converted);
}

}  // namespace

extern "C" {

void WINAPI __vbaMidStmtBstr(const LONG fixed, const BSTR replacement,
                             const LONG length, const LONG start,
                             void* const target) {
    mid_statement(fixed, replacement, chars_to_bytes(length),
                  char_start_to_bytes(start), target);
}

void WINAPI __vbaMidStmtBstrB(const LONG fixed, const BSTR replacement,
                              const LONG length, const LONG start,
                              void* const target) {
    mid_statement(fixed, replacement, length, start, target);
}

void WINAPI __vbaMidStmtVar(VARIANT* const replacement, const LONG length,
                            const LONG start, VARIANT* const target) {
    mid_variant_statement(replacement, chars_to_bytes(length),
                          char_start_to_bytes(start), target);
}

void WINAPI __vbaMidStmtVarB(VARIANT* const replacement, const LONG length,
                             const LONG start, VARIANT* const target) {
    mid_variant_statement(replacement, length, start, target);
}

}  // extern "C"
