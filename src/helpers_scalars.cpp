// REQ-0101: the scalar conversions of the compiler (Variant to number, number
// to String, String to number), the Currency arithmetic, the String
// comparisons and the Variant concatenation.
//
// A compiled program pushes the operands left first, so the helper receives
// the right operand first: __vbaStrCat(right, left), __vbaStrCmp(right,
// left). The conversions are the Automation ones (VarI4FromStr, VarCyAdd, ...)
// with the user's locale; a failure is a Visual Basic error of the thread
// (vbrun::raise_hresult) and the helper returns 0.

#include <cstdint>
#include <cstring>
#include <cwctype>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

// Converts a Variant to the given type; false after raising the error (a Null
// is error 94, the rest are the Automation errors).
bool change(const VARIANT* const value, const VARTYPE type, VARIANT& out) {
    VariantInit(&out);
    if (value == nullptr) {
        static_cast<void>(vbrun::raise(94));
        return false;
    }
    if ((value->vt & VT_TYPEMASK) == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return false;
    }
    const HRESULT result = VariantChangeTypeEx(
        &out, const_cast<VARIANT*>(value), language_locale(), 0, type);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return false;
    }
    return true;
}

// Raises the error of a failed call; true when the call worked.
bool worked(const HRESULT result) {
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return false;
    }
    return true;
}

// The text of a conversion that was written into `text`.
BSTR text_result(const HRESULT result, const BSTR* const text) {
    if (!worked(result)) {
        return nullptr;
    }
    return *text;
}

// The ordering of two Strings by code unit.
int ordinal_order(const BSTR left, const BSTR right) {
    const UINT left_size = left == nullptr ? 0U : SysStringLen(left);
    const UINT right_size = right == nullptr ? 0U : SysStringLen(right);
    const UINT common = left_size < right_size ? left_size : right_size;
    for (UINT at = 0; at < common; ++at) {
        if (left[at] != right[at]) {
            return left[at] < right[at] ? -1 : 1;
        }
    }
    if (left_size == right_size) {
        return 0;
    }
    return left_size < right_size ? -1 : 1;
}

LONG text_order(const BSTR left, const BSTR right) {
    const HRESULT ordering =
        VarBstrCmp(left, right, language_locale(),
                   NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH);
    return static_cast<LONG>(ordering) - 1;
}

// A text that is no date or time reads as a number (`&H1F`, `-5`).
bool date_of_text(const BSTR text, DATE& value) {
    if (SUCCEEDED(VarDateFromStr(text, language_locale(), 0, &value))) {
        return true;
    }
    double number = 0.0;
    if (!worked(VarR8FromStr(text, language_locale(), 0, &number))) {
        return false;
    }
    value = number;
    return true;
}

// The Visual Basic error number of an error Variant (a missing argument is
// 448).
int error_number(const VARIANT* const value) {
    return vbrun::error_of_hresult(value->scode);
}

// Like change(), but an error Variant reads as its error number.
bool change_error_variant(const VARIANT* const value, const VARTYPE type,
                          VARIANT& out) {
    if (value != nullptr && value->vt == VT_ERROR) {
        VARIANT number;
        VariantInit(&number);
        number.vt = VT_I4;
        number.lVal = error_number(value);
        VariantInit(&out);
        const HRESULT result =
            VariantChangeTypeEx(&out, &number, language_locale(), 0, type);
        if (FAILED(result)) {
            static_cast<void>(vbrun::raise_hresult(result));
            return false;
        }
        return true;
    }
    return change(value, type, out);
}

// The text of a Variant for CStr, where an error Variant reads "Error n".
BSTR text_of_error_variant(const VARIANT* const value) {
    if (value != nullptr && value->vt == VT_ERROR) {
        const std::wstring text =
            L"Error " + std::to_wstring(error_number(value));
        return SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    }
    VARIANT out;
    VariantInit(&out);
    if (value == nullptr || (value->vt & VT_TYPEMASK) == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return nullptr;
    }
    // A Boolean reads True or False.
    const HRESULT result =
        VariantChangeTypeEx(&out, const_cast<VARIANT*>(value),
                            language_locale(), VARIANT_ALPHABOOL, VT_BSTR);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return nullptr;
    }
    return out.bstrVal;
}

// Pattern matching of Like.
wchar_t fold(const wchar_t letter, const bool text) {
    return text ? static_cast<wchar_t>(std::towlower(letter)) : letter;
}

bool like_here(const wchar_t* pattern, const wchar_t* const pattern_end,
               const wchar_t* text, const wchar_t* const text_end,
               const bool ignore_case, bool& bad) {
    while (pattern < pattern_end) {
        const wchar_t letter = *pattern;
        if (letter == L'?') {
            if (text == text_end) {
                return false;
            }
            ++pattern;
            ++text;
        } else if (letter == L'#') {
            if (text == text_end || *text < L'0' || *text > L'9') {
                return false;
            }
            ++pattern;
            ++text;
        } else if (letter == L'*') {
            while (pattern < pattern_end && *pattern == L'*') {
                ++pattern;
            }
            if (pattern == pattern_end) {
                return true;
            }
            for (const wchar_t* at = text;; ++at) {
                if (like_here(pattern, pattern_end, at, text_end, ignore_case,
                              bad)) {
                    return true;
                }
                if (bad || at == text_end) {
                    return false;
                }
            }
        } else if (letter == L'[') {
            const wchar_t* at = pattern + 1;
            bool negate = false;
            if (at < pattern_end && *at == L'!') {
                negate = true;
                ++at;
            }
            const wchar_t current =
                text == text_end ? L'\0' : fold(*text, ignore_case);
            bool matched = false;
            while (at < pattern_end && *at != L']') {
                const wchar_t low = fold(*at++, ignore_case);
                wchar_t high = low;
                if (at + 1 < pattern_end && *at == L'-' && at[1] != L']') {
                    high = fold(at[1], ignore_case);
                    at += 2;
                }
                if (current >= low && current <= high) {
                    matched = true;
                }
            }
            if (at == pattern_end) {
                bad = true;
                return false;
            }
            if (text == text_end || matched == negate) {
                return false;
            }
            pattern = at + 1;
            ++text;
        } else {
            if (text == text_end ||
                fold(*text, ignore_case) != fold(letter, ignore_case)) {
                return false;
            }
            ++pattern;
            ++text;
        }
    }
    return text == text_end;
}

LONG like(const BSTR pattern, const BSTR text, const bool ignore_case) {
    const UINT pattern_size = pattern == nullptr ? 0U : SysStringLen(pattern);
    const UINT text_size = text == nullptr ? 0U : SysStringLen(text);
    bool bad = false;
    const bool matched = like_here(pattern, pattern + pattern_size, text,
                                   text + text_size, ignore_case, bad);
    if (bad) {
        static_cast<void>(vbrun::raise(93));  // invalid pattern string
        return 0;
    }
    return matched ? -1 : 0;
}

LONG like_of_variants(const VARIANT* const pattern, const VARIANT* const text,
                      const bool ignore_case) {
    if (pattern == nullptr || text == nullptr ||
        (pattern->vt & VT_TYPEMASK) == VT_NULL ||
        (text->vt & VT_TYPEMASK) == VT_NULL) {
        return 0;
    }
    VARIANT pattern_text;
    VARIANT subject;
    if (!change(pattern, VT_BSTR, pattern_text)) {
        return 0;
    }
    if (!change(text, VT_BSTR, subject)) {
        VariantClear(&pattern_text);
        return 0;
    }
    const LONG answer =
        like(pattern_text.bstrVal, subject.bstrVal, ignore_case);
    VariantClear(&pattern_text);
    VariantClear(&subject);
    return answer;
}

// The generator of Rnd: a 24-bit linear congruence.
std::uint32_t g_seed = 0x50000U;

float next_random() {
    g_seed = (g_seed * 0x43FD43FDU + 0xC39EC3U) & 0xFFFFFFU;
    return static_cast<float>(g_seed) / 16777216.0F;
}

// Randomize: the seed takes bits of the Double's high word.
void reseed(const std::uint32_t high_word) {
    const std::uint32_t mixed =
        ((high_word & 0xFFFFU) << 8) ^ ((high_word >> 8) & 0xFFFF00U);
    g_seed = mixed | (g_seed & 0xFF0000FFU);
}

// Whether a Variant is the missing optional argument.
bool is_missing(const VARIANT* const value) {
    return value == nullptr ||
           (value->vt == VT_ERROR && value->scode == DISP_E_PARAMNOTFOUND);
}

float seconds_since_midnight() {
    SYSTEMTIME now;
    GetLocalTime(&now);
    return static_cast<float>(
        ((now.wHour * 60 + now.wMinute) * 60 + now.wSecond) +
        now.wMilliseconds * 0.001);
}

// InStr on Strings: the position of `needle` in `haystack` from `start` (1
// based); 0 when it is not found. An empty needle gives `start`.
LONG find_in_text(const LONG mode, const BSTR needle, const BSTR haystack,
                  const LONG start) {
    const UINT haystack_size =
        haystack == nullptr ? 0U : SysStringLen(haystack);
    const UINT needle_size = needle == nullptr ? 0U : SysStringLen(needle);
    if (haystack_size == 0U) {
        return 0;
    }
    if (needle_size == 0U) {
        return start;
    }
    if (start < 1) {
        static_cast<void>(vbrun::raise(5));
        return 0;
    }
    if (mode != 0 && mode != 1) {
        static_cast<void>(vbrun::raise(5));
        return 0;
    }
    if (static_cast<UINT>(start) > haystack_size ||
        needle_size > haystack_size) {
        return 0;
    }
    for (UINT at = static_cast<UINT>(start) - 1U;
         at + needle_size <= haystack_size; ++at) {
        if (mode == 0) {
            bool same = true;
            for (UINT offset = 0; offset < needle_size; ++offset) {
                if (haystack[at + offset] != needle[offset]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                return static_cast<LONG>(at) + 1;
            }
        } else if (CompareStringW(
                       language_locale(),
                       NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH,
                       haystack + at, static_cast<int>(needle_size), needle,
                       static_cast<int>(needle_size)) == CSTR_EQUAL) {
            return static_cast<LONG>(at) + 1;
        }
    }
    return 0;
}

// InStrB: the same on the bytes.
LONG find_in_bytes(const BSTR needle, const BSTR haystack, const LONG start) {
    const UINT haystack_size =
        haystack == nullptr ? 0U : SysStringByteLen(haystack);
    const UINT needle_size = needle == nullptr ? 0U : SysStringByteLen(needle);
    if (haystack_size == 0U) {
        return 0;
    }
    if (start >= 1 && static_cast<UINT>(start) > haystack_size) {
        return 0;
    }
    if (needle_size == 0U) {
        return start;
    }
    if (start < 1) {
        static_cast<void>(vbrun::raise(5));
        return 0;
    }
    if (needle_size > haystack_size) {
        return 0;
    }
    const auto* const bytes = reinterpret_cast<const unsigned char*>(haystack);
    const auto* const wanted = reinterpret_cast<const unsigned char*>(needle);
    for (UINT at = static_cast<UINT>(start) - 1U;
         at + needle_size <= haystack_size; ++at) {
        if (std::memcmp(bytes + at, wanted, needle_size) == 0) {
            return static_cast<LONG>(at) + 1;
        }
    }
    return 0;
}

// The days from 1970-01-01 of a date of the proleptic Gregorian calendar.
std::int64_t days_from_civil(std::int64_t year, const int month,
                             const int day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t year_of_era = year - era * 400;
    const std::int64_t day_of_year =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + day_of_era - 719468;
}

// The year of a two-digit year: the century that ends at the system's
// TwoDigitYearMax.
std::int64_t full_year(const std::int64_t year) {
    DWORD limit = 0;
    if (GetCalendarInfoW(LOCALE_USER_DEFAULT, CAL_GREGORIAN,
                         CAL_ITWODIGITYEARMAX | CAL_RETURN_NUMBER, nullptr, 0,
                         &limit) == 0 ||
        limit < 99) {
        limit = 2029;
    }
    const std::int64_t base = static_cast<std::int64_t>(limit) - 99;
    return base + ((year - base % 100 + 100) % 100);
}

BSTR formatted(const VARIANT* const value, const VARIANT* const format,
               const LONG first_day, const LONG first_week) {
    BSTR pattern = nullptr;
    if (format != nullptr && !is_missing(format)) {
        if (format->vt != VT_BSTR) {
            static_cast<void>(vbrun::raise(5));
            return nullptr;
        }
        pattern = format->bstrVal;
    }
    BSTR text = nullptr;
    const HRESULT status = VarFormat(const_cast<VARIANT*>(value), pattern,
                                     first_day, first_week, 0, &text);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return nullptr;
    }
    return text;
}
}  // namespace

extern "C" {

// CInt(v), CLng(v), CByte(v), CSng(v), CDbl(v), CBool(v), CDate(v), CCur(v).
SHORT WINAPI __vbaI2Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_I2, out)) {
        return 0;
    }
    return out.iVal;
}

LONG WINAPI __vbaI4Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_I4, out)) {
        return 0;
    }
    return out.lVal;
}

BYTE WINAPI __vbaUI1Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_UI1, out)) {
        return 0;
    }
    return out.bVal;
}

float WINAPI __vbaR4Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_R4, out)) {
        return 0.0F;
    }
    return out.fltVal;
}

double WINAPI __vbaR8Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_R8, out)) {
        return 0.0;
    }
    return out.dblVal;
}

SHORT WINAPI __vbaBoolVar(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_BOOL, out)) {
        return 0;
    }
    return out.boolVal;
}

double WINAPI __vbaDateVar(const VARIANT* const value) {
    if (value != nullptr && value->vt == VT_BSTR) {
        DATE date = 0.0;
        return date_of_text(value->bstrVal, date) ? date : 0.0;
    }
    VARIANT out;
    if (!change(value, VT_DATE, out)) {
        return 0.0;
    }
    return out.date;
}

LONGLONG WINAPI __vbaCyVar(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_CY, out)) {
        return 0;
    }
    return out.cyVal.int64;
}

// CStr(number).
BSTR WINAPI __vbaStrI2(const SHORT value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromI2(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrBool(const SHORT value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromBool(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrR4(const float value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromR4(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrR8(const double value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromR8(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrDate(const double value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromDate(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrCy(const LONGLONG value) {
    BSTR text = nullptr;
    CY money;
    money.int64 = value;
    const HRESULT status = VarBstrFromCy(money, language_locale(), 0, &text);
    return text_result(status, &text);
}

// CDbl(text), CDate(text), CCur(text).
double WINAPI __vbaR8Str(const BSTR text) {
    double value = 0.0;
    if (!worked(VarR8FromStr(text, language_locale(), 0, &value))) {
        return 0.0;
    }
    return value;
}

double WINAPI __vbaDateStr(const BSTR text) {
    DATE value = 0.0;
    return date_of_text(text, value) ? value : 0.0;
}

LONGLONG WINAPI __vbaCyStr(const BSTR text) {
    CY value;
    value.int64 = 0;
    if (!worked(VarCyFromStr(text, language_locale(), 0, &value))) {
        return 0;
    }
    return value.int64;
}

// Currency arithmetic and conversions.
LONGLONG WINAPI __vbaCyAdd(const LONGLONG right, const LONGLONG left) {
    CY a;
    CY b;
    CY sum;
    a.int64 = left;
    b.int64 = right;
    sum.int64 = 0;
    return worked(VarCyAdd(a, b, &sum)) ? sum.int64 : 0;
}

// The minuend comes first here (left - right), unlike the String helpers.
LONGLONG WINAPI __vbaCySub(const LONGLONG left, const LONGLONG right) {
    CY a;
    CY b;
    CY difference;
    a.int64 = left;
    b.int64 = right;
    difference.int64 = 0;
    return worked(VarCySub(a, b, &difference)) ? difference.int64 : 0;
}

LONGLONG WINAPI __vbaCyMul(const LONGLONG right, const LONGLONG left) {
    CY a;
    CY b;
    CY product;
    a.int64 = left;
    b.int64 = right;
    product.int64 = 0;
    return worked(VarCyMul(a, b, &product)) ? product.int64 : 0;
}

LONGLONG WINAPI __vbaCyAbs(const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyAbs(in, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyFix(const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyFix(in, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyInt(const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyInt(in, &out)) ? out.int64 : 0;
}

LONG WINAPI __vbaCySgn(const LONGLONG value) {
    if (value == 0) {
        return 0;
    }
    return value < 0 ? -1 : 1;
}

LONGLONG WINAPI __vbaCyI2(const SHORT value) {
    CY out;
    out.int64 = 0;
    return worked(VarCyFromI2(value, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyI4(const LONG value) {
    CY out;
    out.int64 = 0;
    return worked(VarCyFromI4(value, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyUI1(const BYTE value) {
    CY out;
    out.int64 = 0;
    return worked(VarCyFromUI1(value, &out)) ? out.int64 : 0;
}

SHORT WINAPI __vbaI2Cy(const LONGLONG value) {
    CY in;
    SHORT out = 0;
    in.int64 = value;
    return worked(VarI2FromCy(in, &out)) ? out : 0;
}

LONG WINAPI __vbaI4Cy(const LONGLONG value) {
    CY in;
    LONG out = 0;
    in.int64 = value;
    return worked(VarI4FromCy(in, &out)) ? out : 0;
}

BYTE WINAPI __vbaUI1Cy(const LONGLONG value) {
    CY in;
    BYTE out = 0;
    in.int64 = value;
    return worked(VarUI1FromCy(in, &out)) ? out : 0;
}

float WINAPI __vbaR4Cy(const LONGLONG value) {
    CY in;
    float out = 0.0F;
    in.int64 = value;
    return worked(VarR4FromCy(in, &out)) ? out : 0.0F;
}

double WINAPI __vbaR8Cy(const LONGLONG value) {
    CY in;
    double out = 0.0;
    in.int64 = value;
    return worked(VarR8FromCy(in, &out)) ? out : 0.0;
}

// left & right on Strings.
BSTR WINAPI __vbaStrCat(const BSTR right, const BSTR left) {
    BSTR joined = nullptr;
    const HRESULT status = VarBstrCat(left, right, &joined);
    return text_result(status, &joined);
}

// Comparison of Strings: -1, 0 or 1 for left against right. The mode is 0 for
// a binary comparison and 1 for a text comparison; the database mode (2) is
// an error.
LONG WINAPI __vbaStrComp(const LONG mode, const BSTR right, const BSTR left) {
    if (mode == 0) {
        return ordinal_order(left, right);
    }
    if (mode == 1) {
        return text_order(left, right);
    }
    static_cast<void>(vbrun::raise(5));
    return 0;
}

LONG WINAPI __vbaStrCmp(const BSTR right, const BSTR left) {
    return ordinal_order(left, right);
}

LONG WINAPI __vbaStrTextCmp(const BSTR right, const BSTR left) {
    return text_order(left, right);
}

// left & right on Variants (result, right, left).
VARIANT* WINAPI __vbaVarCat(VARIANT* const result, VARIANT* const right,
                            VARIANT* const left) {
    VariantInit(result);
    const HRESULT status = VarCat(left, right, result);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return result;
}

// The conversions that read an error Variant as its error number (an optional
// argument that was left out).
SHORT WINAPI __vbaI2ErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_I2, out) ? out.iVal : 0;
}

LONG WINAPI __vbaI4ErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_I4, out) ? out.lVal : 0;
}

BYTE WINAPI __vbaUI1ErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_UI1, out) ? out.bVal : 0;
}

float WINAPI __vbaR4ErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_R4, out) ? out.fltVal : 0.0F;
}

double WINAPI __vbaR8ErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_R8, out) ? out.dblVal : 0.0;
}

SHORT WINAPI __vbaBoolErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_BOOL, out) ? out.boolVal : 0;
}

LONGLONG WINAPI __vbaCyErrVar(const VARIANT* const value) {
    VARIANT out;
    return change_error_variant(value, VT_CY, out) ? out.cyVal.int64 : 0;
}

SHORT WINAPI rtI2FromErrVar(const VARIANT* const value) {
    return __vbaI2ErrVar(value);
}

LONG WINAPI rtI4FromErrVar(const VARIANT* const value) {
    return __vbaI4ErrVar(value);
}

BYTE WINAPI rtUI1FromErrVar(const VARIANT* const value) {
    return __vbaUI1ErrVar(value);
}

float WINAPI rtR4FromErrVar(const VARIANT* const value) {
    return __vbaR4ErrVar(value);
}

double WINAPI rtR8FromErrVar(const VARIANT* const value) {
    return __vbaR8ErrVar(value);
}

SHORT WINAPI rtBoolFromErrVar(const VARIANT* const value) {
    return __vbaBoolErrVar(value);
}

LONGLONG WINAPI rtCyFromErrVar(const VARIANT* const value) {
    return __vbaCyErrVar(value);
}

BSTR WINAPI rtBstrFromErrVar(const VARIANT* const value) {
    return text_of_error_variant(value);
}

BSTR WINAPI __vbaStrErrVarCopy(const VARIANT* const value) {
    return text_of_error_variant(value);
}

// CDate(v) as a Variant: a Null stays Null.
VARIANT* WINAPI rtcVarDateFromVar(VARIANT* const result,
                                  const VARIANT* const value) {
    VariantClear(result);
    if (value != nullptr && (value->vt & VT_TYPEMASK) == VT_NULL) {
        result->vt = VT_NULL;
        return result;
    }
    if (value != nullptr && value->vt == VT_BSTR) {
        DATE date = 0.0;
        if (date_of_text(value->bstrVal, date)) {
            result->vt = VT_DATE;
            result->date = date;
        }
        return result;
    }
    VARIANT converted;
    if (change(value, VT_DATE, converted)) {
        *result = converted;
    }
    return result;
}

VARIANT* WINAPI __vbaVarDateVar(VARIANT* const result,
                                const VARIANT* const value) {
    return rtcVarDateFromVar(result, value);
}

double WINAPI rtcDateFromVar(const VARIANT* const value) {
    return __vbaDateVar(value);
}

// CVar(v): a copy of the value.
VARIANT* WINAPI rtcVarFromVar(VARIANT* const result,
                              const VARIANT* const value) {
    VariantClear(result);
    const HRESULT status = VariantCopyInd(result, const_cast<VARIANT*>(value));
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return result;
}

// CVErr(number).
VARIANT* WINAPI rtcCVErrFromVar(VARIANT* const result,
                                const VARIANT* const value) {
    VariantClear(result);
    vbrun::clear_error();
    const LONG number = __vbaI4Var(value);
    if (vbrun::last_error() != 0) {
        return result;
    }
    if (number < 0 || number > 65535) {
        static_cast<void>(vbrun::raise(5));
        return result;
    }
    result->vt = VT_ERROR;
    result->scode =
        static_cast<SCODE>(0x800A0000U | static_cast<unsigned>(number));
    return result;
}

// The Like operator: the pattern comes first (right operand first).
LONG WINAPI __vbaStrLike(const BSTR pattern, const BSTR text) {
    return like(pattern, text, false);
}

LONG WINAPI __vbaStrTextLike(const BSTR pattern, const BSTR text) {
    return like(pattern, text, true);
}

LONG WINAPI __vbaVarLike(const VARIANT* const pattern,
                         const VARIANT* const text) {
    return like_of_variants(pattern, text, false);
}

LONG WINAPI __vbaVarTextLike(const VARIANT* const pattern,
                             const VARIANT* const text) {
    return like_of_variants(pattern, text, true);
}

// TimeSerial(hour, minute, second).
VARIANT* WINAPI rtcPackTime(VARIANT* const result, const LONG hours,
                            const LONG minutes, const LONG seconds) {
    VariantClear(result);
    result->vt = VT_DATE;
    result->date =
        static_cast<double>(hours * 3600 + minutes * 60 + seconds) / 86400.0;
    return result;
}

// Rnd(x) and Randomize(x).
float WINAPI rtcRandomNext(const VARIANT* const value) {
    if (is_missing(value)) {
        return next_random();
    }
    VARIANT converted;
    if (!change(value, VT_R4, converted)) {
        return 0.0F;
    }
    const float number = converted.fltVal;
    if (number == 0.0F) {
        return static_cast<float>(g_seed) / 16777216.0F;
    }
    if (number < 0.0F) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &number, sizeof(bits));
        g_seed = (bits + (bits >> 24)) & 0xFFFFFFU;
    }
    return next_random();
}

void WINAPI rtcRandomize(const VARIANT* const value) {
    if (is_missing(value)) {
        const float timer = seconds_since_midnight();
        std::uint32_t bits = 0;
        std::memcpy(&bits, &timer, sizeof(bits));
        reseed(bits);
        return;
    }
    VARIANT converted;
    if (!change(value, VT_R8, converted)) {
        return;
    }
    std::uint64_t bits = 0;
    std::memcpy(&bits, &converted.dblVal, sizeof(bits));
    reseed(static_cast<std::uint32_t>(bits >> 32));
}

// Timer: the seconds since midnight.
float WINAPI rtcGetTimer() {
    return seconds_since_midnight();
}

// InStr(start, text, find, compare): the operands arrive right first (the text
// to find comes before the text searched).
LONG WINAPI __vbaInStr(const LONG mode, const BSTR needle, const BSTR haystack,
                       const LONG start) {
    return find_in_text(mode, needle, haystack, start);
}

LONG WINAPI __vbaInStrB(const LONG mode, const BSTR needle, const BSTR haystack,
                        const LONG start) {
    static_cast<void>(mode);
    return find_in_bytes(needle, haystack, start);
}

// Format(expression, format, first day of week, first week of year).
BSTR WINAPI rtcBstrFromFormatVar(const VARIANT* const value,
                                 const VARIANT* const format,
                                 const LONG first_day, const LONG first_week) {
    return formatted(value, format, first_day, first_week);
}

VARIANT* WINAPI rtcVarFromFormatVar(VARIANT* const result,
                                    const VARIANT* const value,
                                    const VARIANT* const format,
                                    const LONG first_day,
                                    const LONG first_week) {
    VariantClear(result);
    BSTR text = formatted(value, format, first_day, first_week);
    if (text != nullptr) {
        result->vt = VT_BSTR;
        result->bstrVal = text;
    }
    return result;
}

// Error$(number): the message of a run-time error; the number is a Variant.
BSTR WINAPI rtcBstrFromError(const VARIANT* const number) {
    const LONG value = __vbaI4Var(number);
    const wchar_t* const message = vbrun::error_message(value);
    return message == nullptr ? nullptr : SysAllocString(message);
}

VARIANT* WINAPI rtcVarFromError(VARIANT* const result,
                                const VARIANT* const number) {
    VariantClear(result);
    const LONG value = __vbaI4Var(number);
    const wchar_t* const message = vbrun::error_message(value);
    result->vt = VT_BSTR;
    result->bstrVal = SysAllocString(message == nullptr ? L"" : message);
    return result;
}

// DateSerial(year, month, day).
VARIANT* WINAPI rtcPackDate(VARIANT* const result, const LONG year_value,
                            const LONG month, const LONG day) {
    VariantClear(result);
    std::int64_t year = year_value;
    if (year >= -99 && year < 0) {
        year += 100;
    }
    if (year >= 0 && year <= 99) {
        year = full_year(year);
    }
    const std::int64_t months = year * 12 + (month - 1);
    std::int64_t whole_years = months / 12;
    std::int64_t month_of_year = months % 12;
    if (month_of_year < 0) {
        month_of_year += 12;
        --whole_years;
    }
    const std::int64_t days =
        days_from_civil(whole_years, static_cast<int>(month_of_year) + 1, 1) +
        (static_cast<std::int64_t>(day) - 1);
    const std::int64_t serial = days - days_from_civil(1899, 12, 30);
    const std::int64_t earliest =
        days_from_civil(100, 1, 1) - days_from_civil(1899, 12, 30);
    const std::int64_t latest =
        days_from_civil(9999, 12, 31) - days_from_civil(1899, 12, 30);
    if (serial < earliest || serial > latest) {
        static_cast<void>(vbrun::raise(5));
        return result;
    }
    result->vt = VT_DATE;
    result->date = static_cast<double>(serial);
    return result;
}

// A Currency times an Integer.
LONGLONG WINAPI __vbaCyMulI2(const SHORT factor, const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyMulI4(in, factor, &out)) ? out.int64 : 0;
}

}  // extern "C"

// StrComp on Variants: each side reads as a String; a Null on either side
// gives a Null result. The result is an Integer Variant.
namespace {

// 1: a String (`text` allocated), 0: a Null, -1: raised.
int string_of_variant(const VARIANT* const value, BSTR& text) {
    VARIANT source = *value;
    if ((source.vt & VT_BYREF) != 0) {
        VariantInit(&source);
        if (FAILED(VariantCopyInd(&source, const_cast<VARIANT*>(value)))) {
            static_cast<void>(vbrun::raise(13));
            return -1;
        }
    }
    if (source.vt == VT_NULL) {
        return 0;
    }
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status = VariantChangeTypeEx(
        &converted, &source, language_locale(), VARIANT_ALPHABOOL, VT_BSTR);
    if (source.vt != value->vt) {
        VariantClear(&source);
    }
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return -1;
    }
    text = converted.bstrVal;
    return 1;
}

}  // namespace

extern "C" {

VARIANT* WINAPI __vbaStrCompVar(VARIANT* const result, const LONG mode,
                                VARIANT* const right, VARIANT* const left) {
    VariantInit(result);
    BSTR right_text = nullptr;
    BSTR left_text = nullptr;
    const int right_state = string_of_variant(right, right_text);
    const int left_state =
        right_state < 0 ? -1 : string_of_variant(left, left_text);
    if (right_state > 0 && left_state > 0) {
        const LONG order = __vbaStrComp(mode, right_text, left_text);
        result->vt = VT_I2;
        result->iVal = static_cast<SHORT>(order);
    } else if (right_state >= 0 && left_state >= 0) {
        result->vt = VT_NULL;
    }
    SysFreeString(right_text);
    SysFreeString(left_text);
    return result;
}

// StrComp(left, right, mode) as the compiler calls it for Variants.
VARIANT* WINAPI rtcCompareBstr(VARIANT* const result, VARIANT* const left,
                               VARIANT* const right, const LONG mode) {
    return __vbaStrCompVar(result, mode, right, left);
}

}  // extern "C"
