// REQ-0103: the numeric functions of the language (Sin, Cos, Tan, Atn, Exp,
// Log, Sqr, Val, Round), MonthName, IIf and Partition.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/numeric.cases). A failure is an error of the thread (5 outside
// the domain of a function), the result is then 0.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kTypeMismatch = 13;

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

// Val: the number at the start of a text. Spaces are ignored, &H and &O read
// a hexadecimal and an octal number, and the reading stops at the first
// character that cannot belong to the number.
double read_value(const wchar_t* const text) {
    std::string digits;
    const wchar_t* at = text;
    while (*at == L' ' || *at == L'\t' || *at == L'\n' || *at == L'\r') {
        ++at;
    }
    if (*at == L'&') {
        const wchar_t kind = static_cast<wchar_t>(std::towupper(at[1]));
        const int base = kind == L'H' ? 16 : (kind == L'O' ? 8 : 0);
        if (base != 0) {
            at += 2;
            std::uint64_t value = 0;
            for (; *at != L'\0'; ++at) {
                if (*at == L' ') {
                    continue;
                }
                const int digit = (*at >= L'0' && *at <= L'9')
                                      ? *at - L'0'
                                      : (std::towupper(*at) >= L'A' &&
                                                 std::towupper(*at) <= L'F'
                                             ? std::towupper(*at) - L'A' + 10
                                             : 99);
                if (digit >= base) {
                    break;
                }
                value = value * static_cast<unsigned>(base) +
                        static_cast<unsigned>(digit);
            }
            if (value > 0xFFFFFFFFULL) {
                return 0.0;
            }
            // &H and &O numbers are Longs (or Integers up to 4 digits).
            return static_cast<double>(static_cast<std::int32_t>(value));
        }
    }
    if (*at == L'+' || *at == L'-') {
        digits.push_back(static_cast<char>(*at));
        ++at;
    }
    bool seen_point = false;
    bool seen_digit = false;
    for (; *at != L'\0'; ++at) {
        if (*at == L' ') {
            continue;
        }
        if (*at >= L'0' && *at <= L'9') {
            digits.push_back(static_cast<char>(*at));
            seen_digit = true;
        } else if (*at == L'.' && !seen_point) {
            digits.push_back('.');
            seen_point = true;
        } else {
            break;
        }
    }
    if (seen_digit &&
        (*at == L'e' || *at == L'E' || *at == L'd' || *at == L'D')) {
        std::string exponent = "e";
        const wchar_t* look = at + 1;
        if (*look == L'+' || *look == L'-') {
            exponent.push_back(static_cast<char>(*look));
            ++look;
        }
        std::string power;
        for (; *look >= L'0' && *look <= L'9'; ++look) {
            power.push_back(static_cast<char>(*look));
        }
        if (!power.empty()) {
            digits += exponent + power;
        }
    }
    if (!seen_digit) {
        return 0.0;
    }
    return std::strtod(digits.c_str(), nullptr);
}

// The truth of a condition: a Boolean or a number; Null is false; a text that
// is no number is a Type mismatch (13).
bool truth_of(const VARIANT* const condition, bool& truth) {
    const unsigned vt = condition->vt & ~static_cast<unsigned>(VT_BYREF);
    if (vt == VT_NULL) {
        truth = false;
        return true;
    }
    if (vt == VT_BSTR) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return false;
    }
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status = VariantChangeType(
        &converted, const_cast<VARIANT*>(condition), 0, VT_R8);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    truth = converted.dblVal != 0.0;
    return true;
}

std::wstring padded(const std::wstring& text, const std::size_t width) {
    return text.size() >= width
               ? text
               : std::wstring(width - text.size(), L' ') + text;
}

// The x87 instructions the reference runs (FSIN, FCOS, FPTAN) reduce the
// argument with a 66-bit value of pi, so the result for an argument near a
// multiple of pi differs from the exact one (Sin of the Double nearest to pi
// is 1.2246063538223773E-16, not 1.2246467991473532E-16). The same reduction
// is done here: half of the 66-bit pi, as a sum of two Doubles.
constexpr double kHalfPiHigh = 1.5707963267948966;
// Half of the 66-bit pi minus kHalfPiHigh (exact: (0xC90FDAA22168C234 * 4 + 3)
// * 2^-65 - kHalfPiHigh).
constexpr double kHalfPiLow = 6.123031769111886e-17;

// Only close to a multiple of pi/2 does the shorter pi change the result.
constexpr double kNear = 1.0 / 1024.0;

struct Reduced {
    double remainder = 0.0;
    int quadrant = 0;
};

Reduced reduce(const double x) {
    const double count = std::nearbyint(x / kHalfPiHigh);
    const double first = std::fma(-count, kHalfPiHigh, x);
    Reduced reduced;
    reduced.remainder = first - count * kHalfPiLow;
    reduced.quadrant = static_cast<int>(std::fmod(count, 4.0));
    if (reduced.quadrant < 0) {
        reduced.quadrant += 4;
    }
    return reduced;
}

double sine_of(const double x) {
    const Reduced reduced = reduce(x);
    if (std::fabs(reduced.remainder) >= kNear) {
        return std::sin(x);  // far from a multiple of pi/2: no difference
    }
    switch (reduced.quadrant) {
        case 0:
            return std::sin(reduced.remainder);
        case 1:
            return std::cos(reduced.remainder);
        case 2:
            return -std::sin(reduced.remainder);
        default:
            return -std::cos(reduced.remainder);
    }
}

double cosine_of(const double x) {
    const Reduced reduced = reduce(x);
    if (std::fabs(reduced.remainder) >= kNear) {
        return std::cos(x);  // far from a multiple of pi/2: no difference
    }
    switch (reduced.quadrant) {
        case 0:
            return std::cos(reduced.remainder);
        case 1:
            return -std::sin(reduced.remainder);
        case 2:
            return -std::cos(reduced.remainder);
        default:
            return std::sin(reduced.remainder);
    }
}

double tangent_of(const double x) {
    const Reduced reduced = reduce(x);
    if (std::fabs(reduced.remainder) >= kNear) {
        return std::tan(x);  // far from a multiple of pi/2: no difference
    }
    const double t = std::tan(reduced.remainder);
    return (reduced.quadrant & 1) == 0 ? t : -1.0 / t;
}

}  // namespace

extern "C" {

double WINAPI rtcSin(const double x) {
    if (!(std::fabs(x) < 9.2233720368547758e18)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    return sine_of(x);
}

double WINAPI rtcCos(const double x) {
    if (!(std::fabs(x) < 9.2233720368547758e18)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    return cosine_of(x);
}

double WINAPI rtcTan(const double x) {
    if (!(std::fabs(x) < 9.2233720368547758e18)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    return tangent_of(x);
}

double WINAPI rtcAtn(const double x) {
    return std::atan(x);
}

double WINAPI rtcExp(const double x) {
    const double answer = std::exp(x);
    if (!std::isfinite(answer)) {
        static_cast<void>(vbrun::raise(6));  // Overflow
        return 0.0;
    }
    return answer;
}

double WINAPI rtcLog(const double x) {
    if (!(x > 0.0)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    return std::log(x);
}

double WINAPI rtcSqr(const double x) {
    if (x < 0.0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    return std::sqrt(x);
}

// Val(text).
double WINAPI rtcR8ValFromBstr(const BSTR text) {
    return text == nullptr ? 0.0 : read_value(text);
}

// MonthName(month, abbreviate).
BSTR WINAPI rtcMonthName(const LONG month, const LONG abbreviate) {
    if (month < 1 || month > 12) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    const LCTYPE kind =
        static_cast<short>(abbreviate) != 0
            ? static_cast<LCTYPE>(LOCALE_SABBREVMONTHNAME1 + month - 1)
            : static_cast<LCTYPE>(LOCALE_SMONTHNAME1 + month - 1);
    wchar_t buffer[80] = {};
    const int length = GetLocaleInfoW(LOCALE_USER_DEFAULT, kind, buffer, 80);
    return SysAllocString(length > 0 ? buffer : L"");
}

// IIf(condition, true part, false part): both parts are already evaluated.
VARIANT* WINAPI rtcImmediateIf(VARIANT* const result, VARIANT* const condition,
                               VARIANT* const when_true,
                               VARIANT* const when_false) {
    bool truth = false;
    if (!truth_of(condition, truth)) {
        return result;
    }
    VariantClear(result);
    const HRESULT status =
        VariantCopyInd(result, truth ? when_true : when_false);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return result;
}

// Round(number, digits).
VARIANT* WINAPI rtcRound(VARIANT* const result, VARIANT* const number,
                         const LONG digits) {
    if ((number->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL) {
        return give_null(result);
    }
    VariantClear(result);
    const HRESULT status = VarRound(number, digits, result);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return result;
}

// Partition(number, start, stop, interval): "lower: upper" with both bounds
// padded to the width of Str(stop).
VARIANT* WINAPI rtcPartition(VARIANT* const result, VARIANT* const number,
                             VARIANT* const start, VARIANT* const stop,
                             VARIANT* const interval) {
    if ((number->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL) {
        return give_null(result);
    }
    LONG value = 0;
    LONG first = 0;
    LONG last = 0;
    LONG step = 0;
    const VARIANT* const inputs[4] = {number, start, stop, interval};
    LONG* const outputs[4] = {&value, &first, &last, &step};
    for (int index = 0; index < 4; ++index) {
        VARIANT converted;
        VariantInit(&converted);
        const HRESULT status = VariantChangeType(
            &converted, const_cast<VARIANT*>(inputs[index]), 0, VT_I4);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return result;
        }
        *outputs[index] = converted.lVal;
    }
    if (first < 0 || last <= first || step < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    const std::size_t width = std::to_wstring(last).size() + 1U;
    const std::wstring blank(width, L' ');
    std::wstring lower;
    std::wstring upper;
    if (value < first) {
        lower = blank;
        upper = padded(std::to_wstring(first - 1), width);
    } else if (value > last) {
        lower = padded(std::to_wstring(last + 1), width);
        upper = blank;
    } else {
        const LONG low = first + (value - first) / step * step;
        const LONG high = std::min<LONG>(low + step - 1, last);
        lower = padded(std::to_wstring(low), width);
        upper = padded(std::to_wstring(high), width);
    }
    return give_text(result, lower + L":" + upper);
}

}  // extern "C"
