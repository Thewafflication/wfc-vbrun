// REQ-0103: NPV, MIRR, Choose and Switch, the functions that take an array of
// values by reference.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/lists.cases). A failure is an error of the thread (5 for an
// array that is not one-dimensional, 11 when MIRR has no gain or no loss).

#include <cmath>
#include <vector>

#include "extended.hpp"
#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kDivisionByZero = 11;

// The values of a one-dimensional array of numbers (passed as a pointer to
// the pointer to the array).
bool numbers_of(SAFEARRAY* const* const cell, std::vector<double>& out) {
    SAFEARRAY* const array = cell == nullptr ? nullptr : *cell;
    if (array == nullptr || SafeArrayGetDim(array) != 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    VARTYPE kind = VT_EMPTY;
    SafeArrayGetVartype(array, &kind);
    LONG low = 0;
    LONG high = -1;
    SafeArrayGetLBound(array, 1, &low);
    SafeArrayGetUBound(array, 1, &high);
    for (LONG at = low; at <= high; ++at) {
        double number = 0.0;
        if (kind == VT_R8) {
            SafeArrayGetElement(array, &at, &number);
        } else {
            VARIANT item;
            VariantInit(&item);
            VARIANT converted;
            VariantInit(&converted);
            if (kind == VT_VARIANT) {
                SafeArrayGetElement(array, &at, &item);
            } else {
                item.vt = kind;
                SafeArrayGetElement(array, &at, &item.byref);
            }
            const HRESULT status =
                VariantChangeType(&converted, &item, 0, VT_R8);
            if (kind == VT_VARIANT) {
                VariantClear(&item);
            }
            if (FAILED(status)) {
                static_cast<void>(vbrun::raise_hresult(status));
                return false;
            }
            number = converted.dblVal;
        }
        out.push_back(number);
    }
    return true;
}

double present_value(const double rate, const std::vector<double>& values) {
    double total = 0.0;
    double factor = 1.0;
    for (std::size_t at = 0; at < values.size(); ++at) {
        factor *= 1.0 + rate;
        total += values[at] / factor;
    }
    return total;
}

// The value that IRR sets to zero, as the reference computes it on the x87
// stack: the divisor is kept as a Double, the rest in extended precision.
vbrun::Extended irr_value(const double rate,
                          const std::vector<double>& values) {
    using vbrun::Extended;
    const Extended base =
        Extended::from_double(rate) + Extended::from_double(1.0);
    Extended total;
    double divisor = 1.0;
    for (const double item : values) {
        divisor = (Extended::from_double(divisor) * base).to_double();
        if (divisor == 0.0) {
            return Extended::from_double(1.0e300);
        }
        total = total +
                Extended::from_double(item) / Extended::from_double(divisor);
    }
    return total;
}

bool truth_of(const VARIANT* const value) {
    VARIANT converted;
    VariantInit(&converted);
    if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(value), 0,
                                 VT_BOOL))) {
        return false;
    }
    return converted.boolVal != VARIANT_FALSE;
}

// The Variants of a ParamArray.
bool members_of(SAFEARRAY* const* const cell, std::vector<VARIANT>& out,
                SAFEARRAY*& array) {
    array = cell == nullptr ? nullptr : *cell;
    if (array == nullptr) {
        return true;
    }
    if (SafeArrayGetDim(array) != 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    LONG low = 0;
    LONG high = -1;
    SafeArrayGetLBound(array, 1, &low);
    SafeArrayGetUBound(array, 1, &high);
    for (LONG at = low; at <= high; ++at) {
        VARIANT item;
        VariantInit(&item);
        SafeArrayGetElement(array, &at, &item);
        out.push_back(item);
    }
    return true;
}

}  // namespace

extern "C" {

// NPV(rate, values).
double WINAPI rtcNPV(const double rate, SAFEARRAY* const* const values) {
    std::vector<double> list;
    if (!numbers_of(values, list)) {
        return 0.0;
    }
    return present_value(rate, list);
}

// IRR(values, guess): a secant search from the guess, 40 steps at most.
double WINAPI rtcIRR(SAFEARRAY* const* const values, VARIANT* const guess) {
    using vbrun::Extended;
    double start = 0.1;
    if (!(guess->vt == VT_ERROR &&
          static_cast<unsigned>(guess->scode) == 0x80020004U)) {
        VARIANT converted;
        VariantInit(&converted);
        const HRESULT status = VariantChangeType(&converted, guess, 0, VT_R8);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return 0.0;
        }
        start = converted.dblVal;
    }
    std::vector<double> list;
    if (!numbers_of(values, list)) {
        return 0.0;
    }
    if (!(start > -1.0) || list.size() <= 1U) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    Extended largest = Extended::from_double(list.front()).absolute();
    for (const double item : list) {
        const Extended size = Extended::from_double(item).absolute();
        if (compare(size, largest) > 0) {
            largest = size;
        }
    }
    const double tolerance =
        (largest * Extended::from_double(1.0e-9)).to_double();
    const auto make = [](const double value) {
        return Extended::from_double(value);
    };
    double x0 = start;
    const Extended first = irr_value(x0, list);
    double f0 = first.to_double();
    const double step = compare(first, Extended()) > 0 ? 1.0e-5 : -1.0e-5;
    double x1 = (make(start) + make(step)).to_double();
    if (!(x1 > -1.0)) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0.0;
    }
    double f1 = irr_value(x1, list).to_double();
    for (int round = 0; round < 40; ++round) {
        if (f1 == f0) {
            const double direction = x1 > x0 ? 1.0 : -1.0;
            x0 = (make(x0) - make(direction) * make(1.0e-5)).to_double();
            f0 = irr_value(x0, list).to_double();
            if (f1 == f0) {
                static_cast<void>(vbrun::raise(kInvalidArgument));
                return 0.0;
            }
        }
        const Extended next =
            make(x1) - (make(x1) - make(x0)) * make(f1) / (make(f1) - make(f0));
        double x2 = next.to_double();
        if (compare(next, make(-1.0)) <= 0) {
            x2 = ((make(x1) - make(1.0)) * make(0.5)).to_double();
        }
        const Extended value = irr_value(x2, list);
        const Extended moved =
            x2 > x1 ? make(x2) - make(x1) : make(x1) - make(x2);
        if (compare(value.absolute(), make(tolerance)) < 0 &&
            compare(moved, make(1.0e-7)) < 0) {
            return x2;
        }
        f0 = f1;
        f1 = value.to_double();
        x0 = x1;
        x1 = x2;
    }
    static_cast<void>(vbrun::raise(kInvalidArgument));
    return 0.0;
}

// MIRR(values, finance rate, reinvest rate).
double WINAPI rtcMIRR(SAFEARRAY* const* const values, const double finance,
                      const double reinvest) {
    std::vector<double> list;
    if (!numbers_of(values, list)) {
        return 0.0;
    }
    std::vector<double> gains(list.size(), 0.0);
    std::vector<double> losses(list.size(), 0.0);
    bool has_gain = false;
    bool has_loss = false;
    for (std::size_t at = 0; at < list.size(); ++at) {
        if (list[at] > 0.0) {
            gains[at] = list[at];
            has_gain = true;
        } else if (list[at] < 0.0) {
            losses[at] = list[at];
            has_loss = true;
        }
    }
    if (!has_gain || !has_loss || list.size() < 2) {
        static_cast<void>(vbrun::raise(has_gain && has_loss ? kInvalidArgument
                                                            : kDivisionByZero));
        return 0.0;
    }
    const double count = static_cast<double>(list.size());
    const double top =
        -present_value(reinvest, gains) * std::pow(1.0 + reinvest, count);
    const double bottom = present_value(finance, losses) * (1.0 + finance);
    return std::pow(top / bottom, 1.0 / (count - 1.0)) - 1.0;
}

// Choose(index, choices...): Null when the index is outside the choices.
VARIANT* WINAPI rtcChoose(VARIANT* const result, const float index,
                          SAFEARRAY* const* const choices) {
    VariantClear(result);
    result->vt = VT_NULL;
    SAFEARRAY* array = nullptr;
    std::vector<VARIANT> list;
    if (!members_of(choices, list, array)) {
        return result;
    }
    const double position = std::trunc(static_cast<double>(index));
    if (position >= 1.0 && position <= static_cast<double>(list.size())) {
        const HRESULT status = VariantCopyInd(
            result, &list[static_cast<std::size_t>(position) - 1U]);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
        }
    }
    for (VARIANT& item : list) {
        VariantClear(&item);
    }
    return result;
}

// Switch(expression, value, ...): the value of the first true expression.
VARIANT* WINAPI rtcSwitch(VARIANT* const result,
                          SAFEARRAY* const* const pairs) {
    VariantClear(result);
    result->vt = VT_NULL;
    SAFEARRAY* array = nullptr;
    std::vector<VARIANT> list;
    if (!members_of(pairs, list, array)) {
        return result;
    }
    if (list.size() % 2U != 0U) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
    } else {
        for (std::size_t at = 0; at + 1U < list.size(); at += 2U) {
            const unsigned vt = list[at].vt & ~static_cast<unsigned>(VT_BYREF);
            if (vt != VT_NULL && truth_of(&list[at])) {
                VariantCopyInd(result, &list[at + 1U]);
                break;
            }
        }
    }
    for (VARIANT& item : list) {
        VariantClear(&item);
    }
    return result;
}

}  // extern "C"
