// REQ-0103: the financial functions (SLN, SYD, DDB, PMT, FV, PV, NPer) and
// WeekdayName, Replace, InStrRev and the Format functions.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/finance.cases). A failure is an error of the thread (5 outside
// the domain of a function), the result is then 0 or Null.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;

bool is_absent(const VARIANT* const value) {
    if (value == nullptr) {
        return true;
    }
    return value->vt == VT_ERROR && value->scode == DISP_E_PARAMNOTFOUND;
}

// A Variant argument as a Double; an absent one is the default.
bool number_of(const VARIANT* const value, const double fallback,
               double& number) {
    if (is_absent(value)) {
        number = fallback;
        return true;
    }
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status = VariantChangeType(
        &converted, const_cast<VARIANT*>(value), 0, VT_R8);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    number = converted.dblVal;
    return true;
}

double fail_number() {
    static_cast<void>(vbrun::raise(kInvalidArgument));
    return 0.0;
}

}  // namespace

extern "C" {

// SLN(cost, salvage, life).
double WINAPI rtcSLN(const double cost, const double salvage,
                     const double life) {
    if (life == 0.0) {
        return fail_number();
    }
    return (cost - salvage) / life;
}

// SYD(cost, salvage, life, period).
double WINAPI rtcSYD(const double cost, const double salvage,
                     const double life, const double period) {
    if (life <= 0.0 || period <= 0.0 || period > life) {
        return fail_number();
    }
    return (cost - salvage) * (life - period + 1.0) * 2.0 /
           (life * (life + 1.0));
}

// DDB(cost, salvage, life, period, factor).
double WINAPI rtcDDB(const double cost, const double salvage,
                     const double life, const double period,
                     VARIANT* const factor_argument) {
    double factor = 0.0;
    if (!number_of(factor_argument, 2.0, factor)) {
        return 0.0;
    }
    if (cost < 0.0 || salvage < 0.0 || life <= 0.0 || period <= 0.0 ||
        period > life || factor <= 0.0) {
        return fail_number();
    }
    const double rate = factor / life;
    double book = cost;
    double depreciation = 0.0;
    for (double step = 1.0; step <= period; step += 1.0) {
        depreciation = std::min(book * rate, std::max(book - salvage, 0.0));
        if (period - step < 1.0 && step < period) {
            depreciation = std::min(depreciation, book - salvage);
        }
        book -= depreciation;
    }
    return depreciation;
}

namespace {

struct Annuity {
    double rate;
    double periods;
    double payment;
    double present;
    double future;
    bool begin;
};

double future_of(const Annuity& a) {
    if (a.rate == 0.0) {
        return -(a.present + a.payment * a.periods);
    }
    const double growth = std::pow(1.0 + a.rate, a.periods);
    const double due = a.begin ? 1.0 + a.rate : 1.0;
    return -(a.present * growth +
             a.payment * due * (growth - 1.0) / a.rate);
}

}  // namespace

// PMT(rate, periods, present, future, type).
double WINAPI rtcPMT(const double rate, const double periods,
                     const double present, VARIANT* const future,
                     VARIANT* const type) {
    double end = 0.0;
    double kind = 0.0;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind)) {
        return 0.0;
    }
    if (periods == 0.0) {
        return fail_number();
    }
    const bool begin = kind != 0.0;
    if (rate == 0.0) {
        return -(present + end) / periods;
    }
    const double growth = std::pow(1.0 + rate, periods);
    const double due = begin ? 1.0 + rate : 1.0;
    return -(end + present * growth) * rate / (due * (growth - 1.0));
}

// FV(rate, periods, payment, present, type).
double WINAPI rtcFV(const double rate, const double periods,
                    const double payment, VARIANT* const present,
                    VARIANT* const type) {
    double start = 0.0;
    double kind = 0.0;
    if (!number_of(present, 0.0, start) || !number_of(type, 0.0, kind)) {
        return 0.0;
    }
    return future_of({rate, periods, payment, start, 0.0, kind != 0.0});
}

// PV(rate, periods, payment, future, type).
double WINAPI rtcPV(const double rate, const double periods,
                    const double payment, VARIANT* const future,
                    VARIANT* const type) {
    double end = 0.0;
    double kind = 0.0;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind)) {
        return 0.0;
    }
    if (rate == 0.0) {
        return -(end + payment * periods);
    }
    const double growth = std::pow(1.0 + rate, periods);
    const double due = kind != 0.0 ? 1.0 + rate : 1.0;
    return -(end + payment * due * (growth - 1.0) / rate) / growth;
}

// NPer(rate, payment, present, future, type).
double WINAPI rtcNPer(const double rate, const double payment,
                      const double present, VARIANT* const future,
                      VARIANT* const type) {
    double end = 0.0;
    double kind = 0.0;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind)) {
        return 0.0;
    }
    if (rate == 0.0) {
        if (payment == 0.0) {
            return fail_number();
        }
        return -(present + end) / payment;
    }
    const double due = kind != 0.0 ? payment * (1.0 + rate) : payment;
    const double top = due - end * rate;
    const double bottom = due + present * rate;
    if (bottom == 0.0 || top / bottom <= 0.0 || rate <= -1.0) {
        return fail_number();
    }
    return std::log(top / bottom) / std::log(1.0 + rate);
}

}  // extern "C"

namespace {

// The payment of a period split into interest and principal.
bool period_payment(const double rate, const double period,
                    const double periods, const double present,
                    VARIANT* const future, VARIANT* const type,
                    double& interest, double& payment) {
    double end = 0.0;
    double kind = 0.0;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind)) {
        return false;
    }
    if (periods <= 0.0 || period < 1.0 || period > periods) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    const bool begin = kind != 0.0;
    if (rate == 0.0) {
        payment = -(present + end) / periods;
    } else {
        const double growth = std::pow(1.0 + rate, periods);
        const double due = begin ? 1.0 + rate : 1.0;
        payment = -(end + present * growth) * rate / (due * (growth - 1.0));
    }
    if (period == 1.0) {
        interest = begin ? 0.0 : -present * rate;
    } else if (begin) {
        interest =
            (future_of({rate, period - 2.0, payment, present, 0.0, true}) -
             payment) *
            rate;
    } else {
        interest =
            future_of({rate, period - 1.0, payment, present, 0.0, false}) *
            rate;
    }
    return true;
}

}  // namespace

extern "C" {

// IPMT(rate, period, periods, present, future, type).
double WINAPI rtcIPMT(const double rate, const double period,
                      const double periods, const double present,
                      VARIANT* const future, VARIANT* const type) {
    double interest = 0.0;
    double payment = 0.0;
    if (!period_payment(rate, period, periods, present, future, type,
                        interest, payment)) {
        return 0.0;
    }
    return interest;
}

// PPMT(rate, period, periods, present, future, type).
double WINAPI rtcPPMT(const double rate, const double period,
                      const double periods, const double present,
                      VARIANT* const future, VARIANT* const type) {
    double interest = 0.0;
    double payment = 0.0;
    if (!period_payment(rate, period, periods, present, future, type,
                        interest, payment)) {
        return 0.0;
    }
    return payment - interest;
}

// WeekdayName(weekday, abbreviate, first day of the week).
BSTR WINAPI rtcWeekdayName(const LONG weekday, const LONG abbreviate,
                           const LONG first_day) {
    (void)first_day;
    if (weekday < 1 || weekday > 7) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    // Day 1 is Sunday; the locale numbers its days from Monday.
    const int index = (weekday + 5) % 7;
    const LCTYPE kind =
        static_cast<short>(abbreviate) != 0
            ? static_cast<LCTYPE>(LOCALE_SABBREVDAYNAME1 + index)
            : static_cast<LCTYPE>(LOCALE_SDAYNAME1 + index);
    wchar_t buffer[80] = {};
    const int length = GetLocaleInfoW(LOCALE_USER_DEFAULT, kind, buffer, 80);
    return SysAllocString(length > 0 ? buffer : L"");
}

// Replace(expression, find, replacement, start, count, compare).
BSTR WINAPI rtcReplace(const BSTR expression, const BSTR find,
                       const BSTR replacement, const LONG start,
                       const LONG count, const LONG compare) {
    const UINT length = SysStringLen(expression);
    if (start < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    if (static_cast<UINT>(start) > length && start != 1) {
        return nullptr;
    }
    const std::wstring source(expression == nullptr ? L"" : expression + start - 1,
                              length - static_cast<UINT>(start) + 1);
    const UINT find_length = SysStringLen(find);
    if (find_length == 0 || count == 0) {
        return SysAllocStringLen(source.data(),
                                 static_cast<UINT>(source.size()));
    }
    const std::wstring needle(find, find_length);
    const std::wstring with(replacement == nullptr ? L"" : replacement,
                            SysStringLen(replacement));
    const bool text = static_cast<short>(compare) == 1;
    auto matches = [&](const size_t at) {
        if (!text) {
            return source.compare(at, needle.size(), needle) == 0;
        }
        return CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                              source.data() + at,
                              static_cast<int>(needle.size()), needle.data(),
                              static_cast<int>(needle.size())) ==
               CSTR_EQUAL;
    };
    std::wstring built;
    LONG done = 0;
    size_t at = 0;
    while (at < source.size()) {
        if ((count < 0 || done < count) && at + needle.size() <= source.size() &&
            matches(at)) {
            built += with;
            at += needle.size();
            ++done;
        } else {
            built += source[at];
            ++at;
        }
    }
    return SysAllocStringLen(built.data(), static_cast<UINT>(built.size()));
}

// InStrRev(text, find, start, compare).
LONG WINAPI rtcInStrRev(const BSTR text, const BSTR find, const LONG start,
                        const LONG compare) {
    const LONG length = static_cast<LONG>(SysStringLen(text));
    const LONG find_length = static_cast<LONG>(SysStringLen(find));
    if (start == 0 || start < -1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0;
    }
    const LONG from = start == -1 ? length : start;
    if (from > length) {
        return 0;
    }
    if (find_length == 0) {
        return from;
    }
    const bool fold = static_cast<short>(compare) == 1;
    // `start` is where the match may end.
    for (LONG at = std::min(from - find_length + 1,
                            length - find_length + 1);
         at >= 1; --at) {
        const bool same =
            fold ? CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                  text + at - 1, find_length, find,
                                  find_length) == CSTR_EQUAL
                 : std::wmemcmp(text + at - 1, find,
                                static_cast<size_t>(find_length)) == 0;
        if (same) {
            return at;
        }
    }
    return 0;
}

}  // extern "C"

namespace {

// The Format functions: the OLE Automation formatters the reference calls.
BSTR give_formatted(const HRESULT status, BSTR const* const made_at) {
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return nullptr;
    }
    return *made_at;
}

bool is_null(const VARIANT* const value) {
    return (value->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL;
}

}  // namespace

extern "C" {

BSTR WINAPI rtcFormatNumber(VARIANT* const value, const LONG digits,
                            const LONG lead, const LONG parens,
                            const LONG group) {
    if (is_null(value)) {
        return nullptr;
    }
    BSTR made = nullptr;
    const HRESULT status =
        VarFormatNumber(value, digits, lead, parens, group, 0, &made);
    return give_formatted(status, &made);
}

BSTR WINAPI rtcFormatCurrency(VARIANT* const value, const LONG digits,
                              const LONG lead, const LONG parens,
                              const LONG group) {
    if (is_null(value)) {
        return nullptr;
    }
    BSTR made = nullptr;
    const HRESULT status =
        VarFormatCurrency(value, digits, lead, parens, group, 0, &made);
    return give_formatted(status, &made);
}

BSTR WINAPI rtcFormatPercent(VARIANT* const value, const LONG digits,
                             const LONG lead, const LONG parens,
                             const LONG group) {
    if (is_null(value)) {
        return nullptr;
    }
    BSTR made = nullptr;
    const HRESULT status =
        VarFormatPercent(value, digits, lead, parens, group, 0, &made);
    return give_formatted(status, &made);
}

BSTR WINAPI rtcFormatDateTime(VARIANT* const value, const LONG format) {
    if (is_null(value)) {
        return nullptr;
    }
    BSTR made = nullptr;
    const HRESULT status =
        VarFormatDateTime(value, format, 0, &made);
    return give_formatted(status, &made);
}

}  // extern "C"
