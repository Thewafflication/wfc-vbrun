// REQ-0102: Left, Right, Mid and String on Variants, Str, Hex and Oct on
// Variants, StrConv, Chr, and the parts of a date (Year, Month, Day, Hour,
// Minute, Second, Weekday).
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/varstrings.cases): the result Variant comes first. A Null gives
// a Null, the other operands are read as text or as a date the way
// Automation reads them, and a failure is an error of the thread.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kTypeMismatch = 13;
constexpr int kInvalidNull = 94;

wchar_t lower_of(wchar_t unit);

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

bool is_null(const VARIANT* const value) {
    return (value->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL;
}

VARIANT* give_null(VARIANT* const result) {
    VariantClear(result);
    result->vt = VT_NULL;
    return result;
}

VARIANT* give_text(VARIANT* const result, const std::wstring& text) {
    BSTR made = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal = made;
    return result;
}

VARIANT* give_bytes(VARIANT* const result, const char* const data,
                    const UINT count) {
    BSTR made = SysAllocStringByteLen(data, count);
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal = made;
    return result;
}

VARIANT* give_short(VARIANT* const result, const SHORT number) {
    VariantClear(result);
    result->vt = VT_I2;
    result->iVal = number;
    return result;
}

bool text_of(const VARIANT* const operand, std::wstring& text) {
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status =
        VariantChangeTypeEx(&converted, const_cast<VARIANT*>(operand),
                            language_locale(), VARIANT_ALPHABOOL, VT_BSTR);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    text.assign(converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                SysStringLen(converted.bstrVal));
    VariantClear(&converted);
    return true;
}

// The length argument of Mid: a missing one (an error Variant) takes the rest
// of the String; a whole number is the length.
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

// A piece of the text of an operand: characters or bytes.
enum class Side { left, right };

VARIANT* edge(VARIANT* const result, const VARIANT* const operand,
              const LONG count, const Side side, const bool bytes) {
    if (is_null(operand)) {
        return give_null(result);
    }
    std::wstring text;
    if (!text_of(operand, text)) {
        return result;
    }
    if (count < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    if (bytes) {
        const std::size_t have = text.size() * 2U;
        const std::size_t take = std::min<std::size_t>(count, have);
        const char* const data = reinterpret_cast<const char*>(text.data());
        return give_bytes(result,
                          side == Side::left ? data : data + (have - take),
                          static_cast<UINT>(take));
    }
    const std::size_t take = std::min<std::size_t>(count, text.size());
    return give_text(result, side == Side::left
                                 ? text.substr(0, take)
                                 : text.substr(text.size() - take));
}

VARIANT* middle(VARIANT* const result, const VARIANT* const operand,
                const LONG start, const VARIANT* const length,
                const bool bytes) {
    if (is_null(operand)) {
        return give_null(result);
    }
    std::wstring text;
    if (!text_of(operand, text)) {
        return result;
    }
    std::int64_t count = 0;
    if (start < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    if (!read_length(length, count)) {
        return result;
    }
    const std::int64_t have =
        static_cast<std::int64_t>(text.size()) * (bytes ? 2 : 1);
    const std::int64_t first = static_cast<std::int64_t>(start) - 1;
    if (first >= have) {
        return give_text(result, std::wstring());
    }
    const std::int64_t take =
        count < 0 ? have - first : std::min(count, have - first);
    if (bytes) {
        return give_bytes(result,
                          reinterpret_cast<const char*>(text.data()) + first,
                          static_cast<UINT>(take));
    }
    return give_text(result, text.substr(static_cast<std::size_t>(first),
                                         static_cast<std::size_t>(take)));
}

// The operand as a Date; false (after raising) when it is none.
bool date_of(const VARIANT* const operand, DATE& date) {
    if ((operand->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_EMPTY) {
        date = 0.0;
        return true;
    }
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status =
        VariantChangeTypeEx(&converted, const_cast<VARIANT*>(operand),
                            language_locale(), 0, VT_DATE);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    date = converted.date;
    return true;
}

enum class Part { year, month, day, hour, minute, second };

VARIANT* part_of(VARIANT* const result, const VARIANT* const operand,
                 const Part part) {
    if (is_null(operand)) {
        return give_null(result);
    }
    DATE date = 0.0;
    if (!date_of(operand, date)) {
        return result;
    }
    SYSTEMTIME time{};
    if (!VariantTimeToSystemTime(date, &time)) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return result;
    }
    switch (part) {
        case Part::year:
            return give_short(result, static_cast<SHORT>(time.wYear));
        case Part::month:
            return give_short(result, static_cast<SHORT>(time.wMonth));
        case Part::day:
            return give_short(result, static_cast<SHORT>(time.wDay));
        case Part::hour:
            return give_short(result, static_cast<SHORT>(time.wHour));
        case Part::minute:
            return give_short(result, static_cast<SHORT>(time.wMinute));
        default:
            return give_short(result, static_cast<SHORT>(time.wSecond));
    }
}

// Hex or Oct: a 16-bit value (an Integer, a Boolean, a Byte) shows its 16
// bits, everything else is rounded to a Long.
VARIANT* radix(VARIANT* const result, const VARIANT* const operand,
               const wchar_t* const format) {
    if (is_null(operand)) {
        return give_null(result);
    }
    const unsigned vt = operand->vt & ~static_cast<unsigned>(VT_BYREF);
    const bool by_reference = (operand->vt & VT_BYREF) != 0;
    std::uint32_t bits = 0;
    if (vt == VT_I2 || vt == VT_BOOL) {
        bits = static_cast<std::uint16_t>(by_reference ? *operand->piVal
                                                       : operand->iVal);
    } else if (vt == VT_UI1) {
        bits = by_reference ? *operand->pbVal : operand->bVal;
    } else if (vt == VT_I4) {
        bits = static_cast<std::uint32_t>(by_reference ? *operand->plVal
                                                       : operand->lVal);
    } else {
        VARIANT number;
        VariantInit(&number);
        const HRESULT status =
            VariantChangeTypeEx(&number, const_cast<VARIANT*>(operand),
                                language_locale(), 0, VT_I4);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return result;
        }
        bits = static_cast<std::uint32_t>(number.lVal);
    }
    wchar_t buffer[16];
    std::swprintf(buffer, 16, format, bits);
    return give_text(result, buffer);
}

VARIANT* give_long(VARIANT* const result, const LONG number) {
    VariantClear(result);
    result->vt = VT_I4;
    result->lVal = number;
    return result;
}

// A whole number argument (an Integer or a Long); false after raising.
bool whole_of(const VARIANT* const value, LONG& number) {
    VARIANT converted;
    VariantInit(&converted);
    const unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    if (vt == VT_BSTR || vt == VT_NULL || vt == VT_DATE) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return false;
    }
    const HRESULT status = VariantChangeTypeEx(
        &converted, const_cast<VARIANT*>(value), language_locale(), 0, VT_I4);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    number = converted.lVal;
    return true;
}

VARIANT* search(VARIANT* const result, const VARIANT* const start,
                const VARIANT* const text, const VARIANT* const find,
                const VARIANT* const compare, const bool bytes) {
    LONG first = 0;
    if (!whole_of(start, first)) {
        return result;
    }
    if (first < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    if (is_null(text) || is_null(find)) {
        return give_null(result);
    }
    LONG mode = 0;
    if ((compare->vt & ~static_cast<unsigned>(VT_BYREF)) != VT_ERROR &&
        !whole_of(compare, mode)) {
        return result;
    }
    std::wstring haystack;
    std::wstring needle;
    if (!text_of(text, haystack) || !text_of(find, needle)) {
        return result;
    }
    if (haystack.empty()) {
        return give_long(result, 0);
    }
    if (needle.empty()) {
        return give_long(result, first);
    }
    if (bytes) {
        // InStrB looks for the bytes themselves (case is never ignored).
        const std::string where(reinterpret_cast<const char*>(haystack.data()),
                                haystack.size() * 2U);
        const std::string what(reinterpret_cast<const char*>(needle.data()),
                               needle.size() * 2U);
        if (static_cast<std::size_t>(first - 1) > where.size()) {
            return give_long(result, 0);
        }
        const std::size_t at =
            where.find(what, static_cast<std::size_t>(first - 1));
        return give_long(
            result, at == std::string::npos ? 0 : static_cast<LONG>(at + 1U));
    }
    if (mode == 1) {
        for (wchar_t& unit : haystack) {
            unit = lower_of(unit);
        }
        for (wchar_t& unit : needle) {
            unit = lower_of(unit);
        }
    }
    const std::size_t unit_start = static_cast<std::size_t>(first - 1);
    if (unit_start > haystack.size()) {
        return give_long(result, 0);
    }
    const std::size_t found = haystack.find(needle, unit_start);
    return give_long(result, found == std::wstring::npos
                                 ? 0
                                 : static_cast<LONG>(found + 1U));
}

wchar_t lower_of(const wchar_t unit) {
    return static_cast<wchar_t>(reinterpret_cast<std::uintptr_t>(CharLowerW(
        reinterpret_cast<LPWSTR>(static_cast<std::uintptr_t>(unit)))));
}

wchar_t upper_of(const wchar_t unit) {
    return static_cast<wchar_t>(reinterpret_cast<std::uintptr_t>(CharUpperW(
        reinterpret_cast<LPWSTR>(static_cast<std::uintptr_t>(unit)))));
}

}  // namespace

extern "C" {

VARIANT* WINAPI rtcLeftVar(VARIANT* const result, VARIANT* const operand,
                           const LONG count) {
    return edge(result, operand, count, Side::left, true);
}

VARIANT* WINAPI rtcRightVar(VARIANT* const result, VARIANT* const operand,
                            const LONG count) {
    return edge(result, operand, count, Side::right, true);
}

VARIANT* WINAPI rtcLeftCharVar(VARIANT* const result, VARIANT* const operand,
                               const LONG count) {
    return edge(result, operand, count, Side::left, false);
}

VARIANT* WINAPI rtcRightCharVar(VARIANT* const result, VARIANT* const operand,
                                const LONG count) {
    return edge(result, operand, count, Side::right, false);
}

VARIANT* WINAPI rtcMidVar(VARIANT* const result, VARIANT* const operand,
                          const LONG start, VARIANT* const length) {
    return middle(result, operand, start, length, true);
}

VARIANT* WINAPI rtcMidCharVar(VARIANT* const result, VARIANT* const operand,
                              const LONG start, VARIANT* const length) {
    return middle(result, operand, start, length, false);
}

// String(count, character): the character is a text (its first character) or
// a character code.
VARIANT* WINAPI rtcStringVar(VARIANT* const result, const LONG count,
                             VARIANT* const character) {
    if (is_null(character)) {
        static_cast<void>(vbrun::raise(kInvalidNull));
        return result;
    }
    if (count < 0 || count > 0x3FFFFFFF) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    wchar_t unit = L' ';
    if ((character->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_BSTR) {
        std::wstring text;
        if (!text_of(character, text)) {
            return result;
        }
        if (text.empty()) {
            static_cast<void>(vbrun::raise(kInvalidArgument));
            return result;
        }
        unit = text[0];
    } else {
        VARIANT number;
        VariantInit(&number);
        const HRESULT status = VariantChangeTypeEx(&number, character,
                                                   language_locale(), 0, VT_I4);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return result;
        }
        if (number.lVal < 0 || number.lVal > 0xFFFF) {
            static_cast<void>(vbrun::raise(kInvalidArgument));
            return result;
        }
        unit = static_cast<wchar_t>(number.lVal);
    }
    return give_text(result,
                     std::wstring(static_cast<std::size_t>(count), unit));
}

// StrConv(text, conversion): upper (1), lower (2), proper (3) case, and
// Unicode (64), which widens every byte of the text to a character.
VARIANT* WINAPI rtcStrConvVar(VARIANT* const result, VARIANT* const operand,
                              const LONG conversion) {
    if (is_null(operand)) {
        return give_null(result);
    }
    std::wstring text;
    if (!text_of(operand, text)) {
        return result;
    }
    if ((conversion & 3) == 1) {
        for (wchar_t& unit : text) {
            unit = upper_of(unit);
        }
    } else if ((conversion & 3) == 2) {
        for (wchar_t& unit : text) {
            unit = lower_of(unit);
        }
    } else if ((conversion & 3) == 3) {
        bool start = true;
        for (wchar_t& unit : text) {
            unit = start ? upper_of(unit) : lower_of(unit);
            start = !(IsCharAlphaW(unit) != 0 ||
                      (unit >= L'0' && unit <= L'9') || unit == L'\'');
        }
    }
    if ((conversion & 64) != 0) {
        const char* const bytes = reinterpret_cast<const char*>(text.data());
        const int count = static_cast<int>(text.size() * 2U);
        std::wstring widened(static_cast<std::size_t>(count), L'\0');
        const int made = count == 0
                             ? 0
                             : MultiByteToWideChar(CP_ACP, 0, bytes, count,
                                                   widened.data(), count);
        widened.resize(static_cast<std::size_t>(made));
        text = widened;
    }
    return give_text(result, text);
}

// Chr(code): a character of the ANSI code page.
VARIANT* WINAPI rtcVarBstrFromChar(VARIANT* const result, const LONG code) {
    if (code < 0 || code > 255) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    const char byte = static_cast<char>(code);
    wchar_t unit = static_cast<wchar_t>(code);
    if (code >= 0x80) {
        MultiByteToWideChar(CP_ACP, 0, &byte, 1, &unit, 1);
    }
    return give_text(result, std::wstring(1, unit));
}

VARIANT* WINAPI rtcHexVarFromVar(VARIANT* const result,
                                 VARIANT* const operand) {
    return radix(result, operand, L"%X");
}

VARIANT* WINAPI rtcOctVarFromVar(VARIANT* const result,
                                 VARIANT* const operand) {
    return radix(result, operand, L"%o");
}

VARIANT* WINAPI rtcGetYear(VARIANT* const result, VARIANT* const operand) {
    return part_of(result, operand, Part::year);
}

VARIANT* WINAPI rtcGetMonthOfYear(VARIANT* const result,
                                  VARIANT* const operand) {
    return part_of(result, operand, Part::month);
}

VARIANT* WINAPI rtcGetDayOfMonth(VARIANT* const result,
                                 VARIANT* const operand) {
    return part_of(result, operand, Part::day);
}

VARIANT* WINAPI rtcGetHourOfDay(VARIANT* const result, VARIANT* const operand) {
    return part_of(result, operand, Part::hour);
}

VARIANT* WINAPI rtcGetMinuteOfHour(VARIANT* const result,
                                   VARIANT* const operand) {
    return part_of(result, operand, Part::minute);
}

VARIANT* WINAPI rtcGetSecondOfMinute(VARIANT* const result,
                                     VARIANT* const operand) {
    return part_of(result, operand, Part::second);
}

// InStrB and InStr(start, text, find, compare): all Variants. The start must
// be a number of at least 1, compare 1 ignores case, and the answer is a
// Long (the position in bytes for InStrB).
VARIANT* WINAPI rtcInStr(VARIANT* const result, VARIANT* const start,
                         VARIANT* const text, VARIANT* const find,
                         VARIANT* const compare) {
    return search(result, start, text, find, compare, true);
}

VARIANT* WINAPI rtcInStrChar(VARIANT* const result, VARIANT* const start,
                             VARIANT* const text, VARIANT* const find,
                             VARIANT* const compare) {
    return search(result, start, text, find, compare, false);
}

// Weekday(date, first day): 1 to 7, counted from `first` (0 and 1 are
// Sunday).
VARIANT* WINAPI rtcGetDayOfWeek(VARIANT* const result, VARIANT* const operand,
                                const LONG first) {
    if (is_null(operand)) {
        return give_null(result);
    }
    DATE date = 0.0;
    if (!date_of(operand, date)) {
        return result;
    }
    SYSTEMTIME time{};
    if (!VariantTimeToSystemTime(date, &time)) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return result;
    }
    const int sunday_based = time.wDayOfWeek + 1;  // 1 is Sunday
    const int start = first <= 1 ? 1 : static_cast<int>(first);
    return give_short(result,
                      static_cast<SHORT>((sunday_based - start + 7) % 7 + 1));
}

// Str(number): a leading space for a positive number; a Boolean and a Date
// read as text.
BSTR WINAPI rtcStrFromVar(VARIANT* const operand) {
    const unsigned vt = operand->vt & ~static_cast<unsigned>(VT_BYREF);
    if (vt == VT_NULL) {
        static_cast<void>(vbrun::raise(kInvalidNull));
        return nullptr;
    }
    std::wstring text;
    if (vt == VT_BOOL || vt == VT_DATE) {
        if (!text_of(operand, text)) {
            return nullptr;
        }
    } else {
        VARIANT number;
        VariantInit(&number);
        HRESULT status = S_OK;
        if (vt == VT_EMPTY) {
            number.vt = VT_I2;
            number.iVal = 0;
        } else if (vt == VT_BSTR) {
            status = VariantChangeTypeEx(&number, operand, language_locale(), 0,
                                         VT_R8);
        } else {
            status = VariantCopyInd(&number, operand);
        }
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return nullptr;
        }
        if (!text_of(&number, text)) {
            return nullptr;
        }
        if (text.empty() || text[0] != L'-') {
            text.insert(text.begin(), L' ');
        }
    }
    return SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
}

}  // extern "C"
