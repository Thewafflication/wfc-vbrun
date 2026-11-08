// REQ-0103: DateAdd, DateDiff and DatePart.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/dates.cases). A failure is an error of the thread (5 for an
// interval that is not one of the known ones, or a date out of range).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;

enum class Interval {
    year,
    quarter,
    month,
    day_of_year,
    day,
    weekday,
    week,
    hour,
    minute,
    second,
    unknown
};

Interval interval_of(const BSTR text) {
    std::wstring name(text == nullptr ? L"" : text, SysStringLen(text));
    for (wchar_t& c : name) {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    if (name == L"yyyy") return Interval::year;
    if (name == L"q") return Interval::quarter;
    if (name == L"m") return Interval::month;
    if (name == L"y") return Interval::day_of_year;
    if (name == L"d") return Interval::day;
    if (name == L"w") return Interval::weekday;
    if (name == L"ww") return Interval::week;
    if (name == L"h") return Interval::hour;
    if (name == L"n") return Interval::minute;
    if (name == L"s") return Interval::second;
    return Interval::unknown;
}

bool is_null(const VARIANT* const value) {
    return (value->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL;
}

VARIANT* give_null(VARIANT* const result) {
    VariantClear(result);
    result->vt = VT_NULL;
    return result;
}

// A Variant as a date.
bool date_of(const VARIANT* const value, DATE& date) {
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status =
        VariantChangeType(&converted, const_cast<VARIANT*>(value), 0, VT_DATE);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    date = converted.date;
    return true;
}

LONG long_of(const VARIANT* const value, const LONG fallback) {
    if (value == nullptr || value->vt == VT_ERROR) {
        return fallback;
    }
    VARIANT converted;
    VariantInit(&converted);
    if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(value), 0,
                                 VT_I4))) {
        return fallback;
    }
    return converted.lVal;
}

bool split(const DATE date, SYSTEMTIME& time) {
    return VariantTimeToSystemTime(date, &time) != FALSE;
}

int days_in(const int year, const int month) {
    static const int lengths[12] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : lengths[month - 1];
}

bool join(const int year, const int month, const int day,
          const double fraction, DATE& date) {
    SYSTEMTIME time = {};
    time.wYear = static_cast<WORD>(year);
    time.wMonth = static_cast<WORD>(month);
    time.wDay = static_cast<WORD>(day);
    double base = 0.0;
    if (SystemTimeToVariantTime(&time, &base) == FALSE) {
        return false;
    }
    date = base + fraction;
    return true;
}

double whole_day(const DATE date) {
    return std::floor(date);
}

std::int64_t seconds_of(const DATE date) {
    return static_cast<std::int64_t>(
        std::llround((date - std::floor(date)) * 86400.0));
}

std::int64_t floor_div(const std::int64_t a, const std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

// The weekday of a day number: 0 is Sunday.
int weekday_of(const double day) {
    const std::int64_t n = static_cast<std::int64_t>(day);
    return static_cast<int>(((n + 6) % 7 + 7) % 7);
}

// The first day (day number) of week 1 of a year.
double first_week(const int year, const int first_day, const int rule) {
    double jan1 = 0.0;
    join(year, 1, 1, 0.0, jan1);
    const int offset = (weekday_of(jan1) - (first_day - 1) + 7) % 7;
    const double start = jan1 - offset;
    if (rule == 2 && 7 - offset < 4) {
        return start + 7;
    }
    if (rule == 3 && offset != 0) {
        return start + 7;
    }
    return start;
}

int week_of(const double day, const int year, const int first_day,
            const int rule) {
    const double start = first_week(year, first_day, rule);
    if (day < start) {
        return week_of(day, year - 1, first_day, rule);
    }
    return static_cast<int>((day - start) / 7.0) + 1;
}

}  // namespace

extern "C" {

// DateAdd(interval, number, date).
VARIANT* WINAPI rtcDateAdd(VARIANT* const result, const BSTR interval,
                           const double number, VARIANT* const when) {
    const Interval kind = interval_of(interval);
    if (kind == Interval::unknown) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    if (is_null(when)) {
        return give_null(result);
    }
    DATE date = 0.0;
    if (!date_of(when, date)) {
        return result;
    }
    const double count = std::trunc(number);
    DATE answer = date;
    switch (kind) {
        case Interval::year:
        case Interval::quarter:
        case Interval::month: {
            SYSTEMTIME time = {};
            if (!split(date, time)) {
                static_cast<void>(vbrun::raise(kInvalidArgument));
                return result;
            }
            const double step = kind == Interval::year      ? 12.0
                                : kind == Interval::quarter ? 3.0
                                                            : 1.0;
            const double total =
                time.wYear * 12.0 + (time.wMonth - 1) + count * step;
            const int year = static_cast<int>(std::floor(total / 12.0));
            const int month = static_cast<int>(total - year * 12.0) + 1;
            if (year < 100 || year > 9999) {
                static_cast<void>(vbrun::raise(kInvalidArgument));
                return result;
            }
            const int day = std::min<int>(time.wDay, days_in(year, month));
            const double fraction = date - whole_day(date);
            if (!join(year, month, day, fraction, answer)) {
                static_cast<void>(vbrun::raise(kInvalidArgument));
                return result;
            }
            break;
        }
        case Interval::day_of_year:
        case Interval::day:
        case Interval::weekday:
            answer = date + count;
            break;
        case Interval::week:
            answer = date + count * 7.0;
            break;
        case Interval::hour:
            answer = date + count / 24.0;
            break;
        case Interval::minute:
            answer = date + count / 1440.0;
            break;
        case Interval::second:
            answer = date + count / 86400.0;
            break;
        default:
            break;
    }
    VariantClear(result);
    result->vt = VT_DATE;
    result->date = answer;
    return result;
}

// DateDiff(interval, date 1, date 2, first day of the week, first week).
VARIANT* WINAPI rtcDateDiff(VARIANT* const result, const BSTR interval,
                            VARIANT* const first, VARIANT* const second,
                            VARIANT* const first_day,
                            VARIANT* const first_week_rule) {
    static_cast<void>(first_week_rule);
    const Interval kind = interval_of(interval);
    if (kind == Interval::unknown) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    if (is_null(first) || is_null(second)) {
        return give_null(result);
    }
    DATE a = 0.0;
    DATE b = 0.0;
    if (!date_of(first, a) || !date_of(second, b)) {
        return result;
    }
    SYSTEMTIME ta = {};
    SYSTEMTIME tb = {};
    if (!split(a, ta) || !split(b, tb)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    const LONG day_one = std::max<LONG>(long_of(first_day, 1), 1);
    const double da = whole_day(a);
    const double db = whole_day(b);
    std::int64_t answer = 0;
    switch (kind) {
        case Interval::year:
            answer = tb.wYear - ta.wYear;
            break;
        case Interval::quarter:
            answer = (tb.wYear * 4 + (tb.wMonth - 1) / 3) -
                     (ta.wYear * 4 + (ta.wMonth - 1) / 3);
            break;
        case Interval::month:
            answer =
                (tb.wYear * 12 + tb.wMonth) - (ta.wYear * 12 + ta.wMonth);
            break;
        case Interval::day_of_year:
        case Interval::day:
            answer = static_cast<std::int64_t>(db - da);
            break;
        case Interval::weekday:
            answer = static_cast<std::int64_t>(db - da) / 7;
            break;
        case Interval::week: {
            const int offset_a = (weekday_of(da) - (day_one - 1) + 7) % 7;
            const int offset_b = (weekday_of(db) - (day_one - 1) + 7) % 7;
            answer = static_cast<std::int64_t>(
                ((db - offset_b) - (da - offset_a)) / 7.0);
            break;
        }
        case Interval::hour:
        case Interval::minute:
        case Interval::second: {
            const std::int64_t unit = kind == Interval::hour     ? 3600
                                      : kind == Interval::minute ? 60
                                                                 : 1;
            const std::int64_t sa =
                static_cast<std::int64_t>(da) * 86400 + seconds_of(a);
            const std::int64_t sb =
                static_cast<std::int64_t>(db) * 86400 + seconds_of(b);
            answer = floor_div(sb, unit) - floor_div(sa, unit);
            break;
        }
        default:
            break;
    }
    VariantClear(result);
    result->vt = VT_I4;
    result->lVal = static_cast<LONG>(answer);
    return result;
}

// DatePart(interval, date, first day of the week, first week of the year).
VARIANT* WINAPI rtcDatePart(VARIANT* const result, const BSTR interval,
                            VARIANT* const when, VARIANT* const first_day,
                            VARIANT* const first_week_rule) {
    const Interval kind = interval_of(interval);
    if (kind == Interval::unknown) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    if (is_null(when)) {
        return give_null(result);
    }
    DATE date = 0.0;
    if (!date_of(when, date)) {
        return result;
    }
    SYSTEMTIME time = {};
    if (!split(date, time)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    const LONG day_one = std::max<LONG>(long_of(first_day, 1), 1);
    const LONG rule = std::max<LONG>(long_of(first_week_rule, 1), 1);
    const double day = whole_day(date);
    int part = 0;
    switch (kind) {
        case Interval::year:
            part = time.wYear;
            break;
        case Interval::quarter:
            part = (time.wMonth - 1) / 3 + 1;
            break;
        case Interval::month:
            part = time.wMonth;
            break;
        case Interval::day_of_year: {
            double jan1 = 0.0;
            join(time.wYear, 1, 1, 0.0, jan1);
            part = static_cast<int>(day - jan1) + 1;
            break;
        }
        case Interval::day:
            part = time.wDay;
            break;
        case Interval::weekday:
            part = (weekday_of(day) - (day_one - 1) + 7) % 7 + 1;
            break;
        case Interval::week:
            part = week_of(day, time.wYear, day_one, rule);
            break;
        case Interval::hour:
            part = time.wHour;
            break;
        case Interval::minute:
            part = time.wMinute;
            break;
        case Interval::second:
            part = time.wSecond;
            break;
        default:
            break;
    }
    VariantClear(result);
    result->vt = VT_I2;
    result->iVal = static_cast<SHORT>(part);
    return result;
}

}  // extern "C"
