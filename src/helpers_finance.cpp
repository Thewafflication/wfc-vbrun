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
#include "x87math.hpp"

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
    const HRESULT status =
        VariantChangeType(&converted, const_cast<VARIANT*>(value), 0, VT_R8);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    number = converted.dblVal;
    return true;
}

using vbrun::Extended;

Extended ext(const double value) {
    return Extended::from_double(value);
}

// The growth factor (1 + rate)^periods that the annuity functions compute on
// the x87 unit (the base reaches the power as a Double), and the factor of the
// payments at the start of the period (also stored as a Double). False when the
// base is not positive.
struct Growth {
    Extended factor;
    Extended due;
};

bool growth_of(const double rate, const double periods, const bool begin,
               Growth& growth) {
    const double base = (ext(rate) + ext(1.0)).to_double();
    if (!(base > 0.0)) {
        return false;
    }
    growth.factor = vbrun::x87_power(base, periods);
    growth.due = ext(begin ? base : 1.0);
    return true;
}

// The value that Rate sets to zero: the future value of the payments minus
// the future value wanted, as the reference computes it (a Double).
double rate_error(const double rate, const double periods, const double payment,
                  const double present, const double future, const bool begin) {
    if (rate == 0.0) {
        return (ext(payment) * ext(periods) + ext(future) + ext(present))
            .to_double();
    }
    const double base = (ext(rate) + ext(1.0)).to_double();
    if (!(base > 0.0)) {
        const double growth = std::pow(base, periods);
        const double due = begin ? base : 1.0;
        return (growth - 1.0) * due * payment / rate + present * growth +
               future;
    }
    const Extended growth = vbrun::x87_power(base, periods);
    const Extended due = ext(begin ? base : 1.0);
    const Extended stream =
        (((growth - ext(1.0)) * due) * ext(payment)) / ext(rate);
    return (stream + growth * ext(present) + ext(future)).to_double();
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
double WINAPI rtcSYD(const double cost, const double salvage, const double life,
                     const double period) {
    if (life <= 0.0 || period <= 0.0 || period > life) {
        return fail_number();
    }
    return (cost - salvage) * (life - period + 1.0) * 2.0 /
           (life * (life + 1.0));
}

// DDB(cost, salvage, life, period, factor): the reference's own formulas on
// the x87 unit, not a period by period loop.
double WINAPI rtcDDB(const double cost, const double salvage, const double life,
                     const double period, VARIANT* const factor_argument) {
    double factor = 0.0;
    if (!number_of(factor_argument, 2.0, factor)) {
        return 0.0;
    }
    if (factor <= 0.0 || salvage < 0.0 || period <= 0.0 || period > life) {
        return fail_number();
    }
    if (cost <= 0.0) {
        return 0.0;
    }
    const Extended one = ext(1.0);
    if (life < 2.0) {
        return (ext(cost) - ext(salvage)).to_double();
    }
    if (life == 2.0) {
        return period <= 1.0 ? (ext(cost) - ext(salvage)).to_double() : 0.0;
    }
    if (period <= 1.0) {
        const double most = (ext(cost) - ext(salvage)).to_double();
        const Extended share = (ext(factor) * ext(cost)) / ext(life);
        return compare(share, ext(most)) <= 0 ? share.to_double() : most;
    }
    const Extended ratio = (ext(life) - ext(factor)) / ext(life);
    const double base = ratio.to_double();
    const auto power = [&](const double exponent) {
        return base > 0.0 ? vbrun::x87_power(base, exponent)
                          : ext(std::pow(base, exponent));
    };
    const Extended before = power((ext(period) - one).to_double());
    const Extended share = (ext(cost) * ext(factor)) / ext(life);
    double depreciation = (before * share).to_double();
    const Extended after = power(period);
    const Extended excess =
        (one - after) * ext(cost) - ext(cost) + ext(salvage);
    if (compare(excess, ext(0.0)) > 0) {
        depreciation = (ext(depreciation) - excess).to_double();
    }
    return depreciation < 0.0 ? 0.0 : depreciation;
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
    return -(a.present * growth + a.payment * due * (growth - 1.0) / a.rate);
}

// The payment of an annuity; false when the periods are zero.
bool payment_of(const double rate, const double periods, const double present,
                const double end, const bool begin, double& payment) {
    if (periods == 0.0) {
        static_cast<void>(fail_number());
        return false;
    }
    if (rate == 0.0) {
        payment = ((ext(-end) - ext(present)) / ext(periods)).to_double();
        return true;
    }
    Growth g;
    if (growth_of(rate, periods, begin, g)) {
        const Extended top = ext(-end) - g.factor * ext(present);
        const Extended bottom = (g.factor - ext(1.0)) * g.due;
        payment = ((top / bottom) * ext(rate)).to_double();
        return true;
    }
    const double growth = std::pow(1.0 + rate, periods);
    const double due = begin ? 1.0 + rate : 1.0;
    payment = -(end + present * growth) * rate / (due * (growth - 1.0));
    return true;
}

double future_value(const double rate, const double periods,
                    const double payment, const double start,
                    const bool begin) {
    if (rate == 0.0) {
        return (ext(-start) - ext(payment) * ext(periods)).to_double();
    }
    Growth g;
    if (growth_of(rate, periods, begin, g)) {
        const Extended annuity =
            (((g.factor - ext(1.0)) * g.due) * ext(payment)) / ext(rate);
        return ((ext(start) * g.factor).negated() - annuity).to_double();
    }
    return future_of({rate, periods, payment, start, 0.0, begin});
}

}  // namespace

// PMT(rate, periods, present, future, type).
double WINAPI rtcPMT(const double rate, const double periods,
                     const double present, VARIANT* const future,
                     VARIANT* const type) {
    double end = 0.0;
    double kind = 0.0;
    double payment = 0.0;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind) ||
        !payment_of(rate, periods, present, end, kind != 0.0, payment)) {
        return 0.0;
    }
    return payment;
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
    return future_value(rate, periods, payment, start, kind != 0.0);
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
        return (ext(-end) - ext(payment) * ext(periods)).to_double();
    }
    Growth g;
    if (growth_of(rate, periods, kind != 0.0, g)) {
        const Extended annuity =
            (((g.factor - ext(1.0)) * g.due) * ext(payment)) / ext(rate);
        return ((annuity + ext(end)) / g.factor).negated().to_double();
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
        return ((ext(end) + ext(present)) / ext(payment)).negated().to_double();
    }
    const Extended base = ext(rate) + ext(1.0);
    if (!base.is_negative() && !base.is_zero()) {
        const Extended factor = kind != 0.0 ? base : ext(1.0);
        const Extended stream = (ext(payment) * factor) / ext(rate);
        double top = (stream - ext(end)).to_double();
        double bottom = (stream + ext(present)).to_double();
        if (top == 0.0 || bottom == 0.0 || (top < 0.0) != (bottom < 0.0)) {
            return fail_number();
        }
        if (top < 0.0) {
            top = -top;
            bottom = -bottom;
        }
        const Extended ln2 = vbrun::x87_ln2();
        const Extended difference =
            vbrun::fyl2x(ext(top), ln2) - vbrun::fyl2x(ext(bottom), ln2);
        return (difference / vbrun::fyl2x(base, ln2)).to_double();
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

// The interest of a period, as the reference leaves it on the x87 stack (not
// rounded to a Double), and the payment.
bool period_payment(const double rate, const double period,
                    const double periods, const double present,
                    VARIANT* const future, VARIANT* const type,
                    Extended& interest, double& payment) {
    double end = 0.0;
    double kind = 0.0;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind)) {
        return false;
    }
    const bool begin = kind != 0.0;
    if (!(period > 0.0) || compare(ext(periods) + ext(1.0), ext(period)) <= 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    if (begin && period == 1.0) {
        interest = Extended();
        // the payment is still worked out (it can raise an error)
        return payment_of(rate, periods, present, end, begin, payment);
    }
    if (!payment_of(rate, periods, present, end, begin, payment)) {
        return false;
    }
    const double stream =
        begin ? (ext(payment) + ext(present)).to_double() : present;
    const double before = (ext(period) - ext(begin ? 2.0 : 1.0)).to_double();
    interest =
        ext(future_value(rate, before, payment, stream, false)) * ext(rate);
    return true;
}

}  // namespace

extern "C" {

// IPMT(rate, period, periods, present, future, type).
double WINAPI rtcIPMT(const double rate, const double period,
                      const double periods, const double present,
                      VARIANT* const future, VARIANT* const type) {
    Extended interest;
    double payment = 0.0;
    if (!period_payment(rate, period, periods, present, future, type, interest,
                        payment)) {
        return 0.0;
    }
    return interest.to_double();
}

// PPMT(rate, period, periods, present, future, type).
double WINAPI rtcPPMT(const double rate, const double period,
                      const double periods, const double present,
                      VARIANT* const future, VARIANT* const type) {
    Extended interest;
    double payment = 0.0;
    if (!period_payment(rate, period, periods, present, future, type, interest,
                        payment)) {
        return 0.0;
    }
    return (ext(payment) - interest).to_double();
}

// Rate(periods, payment, present, future, type, guess): a secant search from
// the guess, 40 steps at most, on the x87 unit like the reference.
double WINAPI rtcRate(const double periods, const double payment,
                      const double present, VARIANT* const future,
                      VARIANT* const type, VARIANT* const guess) {
    double end = 0.0;
    double kind = 0.0;
    double start = 0.1;
    if (!number_of(future, 0.0, end) || !number_of(type, 0.0, kind) ||
        !number_of(guess, 0.1, start)) {
        return 0.0;
    }
    if (!(periods > 0.0)) {
        return fail_number();
    }
    const bool begin = kind != 0.0;
    const auto error = [&](const double rate) {
        return rate_error(rate, periods, payment, present, end, begin);
    };
    double x0 = start;
    double g0 = error(x0);
    double x1 = g0 > 0.0 ? (ext(x0) * ext(0.5)).to_double()
                         : (ext(x0) + ext(x0)).to_double();
    double g1 = error(x1);
    for (int round = 0; round < 40; ++round) {
        if (g1 == g0) {
            const double direction = x1 > x0 ? 1.0 : -1.0;
            x0 = (ext(x0) - ext(direction) * ext(1.0e-5)).to_double();
            g0 = error(x0);
            if (g1 == g0) {
                return fail_number();
            }
        }
        const double x2 =
            (ext(x1) - ((ext(x1) - ext(x0)) * ext(g1)) / (ext(g1) - ext(g0)))
                .to_double();
        const double g2 = error(x2);
        if (std::fabs(g2) < 1.0e-7) {
            return x2;
        }
        g0 = g1;
        g1 = g2;
        x0 = x1;
        x1 = x2;
    }
    return fail_number();
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
    const std::wstring source(
        expression == nullptr ? L"" : expression + start - 1,
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
                              static_cast<int>(needle.size())) == CSTR_EQUAL;
    };
    std::wstring built;
    LONG done = 0;
    size_t at = 0;
    while (at < source.size()) {
        if ((count < 0 || done < count) &&
            at + needle.size() <= source.size() && matches(at)) {
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
    for (LONG at = std::min(from - find_length + 1, length - find_length + 1);
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
    const HRESULT status = VarFormatDateTime(value, format, 0, &made);
    return give_formatted(status, &made);
}

}  // extern "C"
