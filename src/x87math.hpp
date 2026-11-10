// The power and the logarithm of the x87 unit, as far as they can be
// reproduced: the reference computes pow(base, exponent) with FYL2X, F2XM1 and
// FSCALE, each result rounded to a 64-bit mantissa. The values are worked out
// here in double-double arithmetic (about 106 bits) and rounded to 64 bits at
// the same points as the unit, so the results agree with it except where the
// unit itself is off by a unit in the last place. Internal to the VBRUN
// helpers.

#pragma once

#include <cmath>
#include <cstdint>

#include "extended.hpp"

namespace vbrun {

struct DoubleDouble {
    double high;
    double low;
};

namespace dd {

inline DoubleDouble quick_sum(const double a, const double b) {
    const double sum = a + b;
    return {sum, b - (sum - a)};
}

inline DoubleDouble two_sum(const double a, const double b) {
    const double sum = a + b;
    const double bb = sum - a;
    return {sum, (a - (sum - bb)) + (b - bb)};
}

inline DoubleDouble two_product(const double a, const double b) {
    const double product = a * b;
    return {product, std::fma(a, b, -product)};
}

inline DoubleDouble add(const DoubleDouble a, const DoubleDouble b) {
    const DoubleDouble s = two_sum(a.high, b.high);
    const DoubleDouble t = two_sum(a.low, b.low);
    DoubleDouble r = quick_sum(s.high, s.low + t.high);
    return quick_sum(r.high, r.low + t.low);
}

inline DoubleDouble negate(const DoubleDouble a) {
    return {-a.high, -a.low};
}

inline DoubleDouble subtract(const DoubleDouble a, const DoubleDouble b) {
    return add(a, negate(b));
}

inline DoubleDouble multiply(const DoubleDouble a, const DoubleDouble b) {
    DoubleDouble p = two_product(a.high, b.high);
    p.low += a.high * b.low + a.low * b.high;
    return quick_sum(p.high, p.low);
}

inline DoubleDouble divide(const DoubleDouble a, const DoubleDouble b) {
    const double q1 = a.high / b.high;
    DoubleDouble r = subtract(a, multiply(b, {q1, 0.0}));
    const double q2 = r.high / b.high;
    r = subtract(r, multiply(b, {q2, 0.0}));
    const double q3 = r.high / b.high;
    DoubleDouble q = quick_sum(q1, q2);
    return add(q, {q3, 0.0});
}

inline DoubleDouble scale(const DoubleDouble a, const double factor) {
    return multiply(a, {factor, 0.0});
}

constexpr DoubleDouble kLn2 = {0.6931471805599453, 2.3190468138462996e-17};
constexpr DoubleDouble kInvLn2 = {1.4426950408889634, 2.0355273740931033e-17};

// exp(x) - 1 for |x| below about 0.4: the series of x / 2^8 and the doubling
// expm1(2y) = 2 expm1(y) + expm1(y)^2, which keeps the relative precision.
inline DoubleDouble expm1_small(const DoubleDouble x) {
    const DoubleDouble y = scale(x, 1.0 / 256.0);
    DoubleDouble term = y;
    DoubleDouble sum = y;
    for (int n = 2; n <= 14; ++n) {
        term = scale(multiply(term, y), 1.0 / n);
        sum = add(sum, term);
    }
    for (int step = 0; step < 8; ++step) {
        sum = add(scale(sum, 2.0), multiply(sum, sum));
    }
    return sum;
}

// exp(x) as a number in [0.7, 1.5) and a power of two.
inline DoubleDouble exp_parts(const DoubleDouble x, int& power) {
    const double k = std::nearbyint(x.high / kLn2.high);
    power = static_cast<int>(k);
    const DoubleDouble r = subtract(x, scale(kLn2, k));
    return add({1.0, 0.0}, expm1_small(r));
}

// log(x) for a positive x: Newton's step on the exponential from the double
// result, in double-double arithmetic.
inline DoubleDouble log_of(const DoubleDouble x) {
    const double y0 = std::log(x.high);
    int power = 0;
    const DoubleDouble mantissa = exp_parts({y0, 0.0}, power);
    const double p = std::ldexp(1.0, power);
    const DoubleDouble e = {mantissa.high * p, mantissa.low * p};
    // y = y0 + 2 (x - e) / (x + e)
    const DoubleDouble correction =
        scale(divide(subtract(x, e), add(x, e)), 2.0);
    return add({y0, 0.0}, correction);
}

inline DoubleDouble from_extended(const Extended& value) {
    const double high = value.to_double();
    const Extended rest = value - Extended::from_double(high);
    return {high, rest.to_double()};
}

inline Extended to_extended(const DoubleDouble value) {
    return Extended::from_double(value.high) + Extended::from_double(value.low);
}

}  // namespace dd

// The x87 constant of FLDLN2.
inline Extended x87_ln2() {
    return dd::to_extended(dd::kLn2);
}

// FYL2X: y * log2(x) rounded to 64 bits, for a positive x.
inline Extended fyl2x(const Extended& x, const Extended& y) {
    const DoubleDouble log2_of_x =
        dd::multiply(dd::log_of(dd::from_extended(x)), dd::kInvLn2);
    return dd::to_extended(dd::multiply(log2_of_x, dd::from_extended(y)));
}

// pow(base, exponent) as the reference computes it for a positive base:
// FYL2X, F2XM1 of the fraction, 1 added and FSCALE. (The reference passes the
// base through memory as a Double.)
inline Extended x87_power(const double base, const double exponent) {
    const Extended product =
        fyl2x(Extended::from_double(base), Extended::from_double(exponent));
    const std::int64_t whole = product.round_to_int();
    const Extended fraction = product - Extended::from_int(whole);
    Extended root_minus_one;
    if (!fraction.is_zero()) {
        root_minus_one = dd::to_extended(dd::expm1_small(
            dd::multiply(dd::from_extended(fraction), dd::kLn2)));
    }
    const Extended root = root_minus_one + Extended::from_int(1);
    return root.scaled(static_cast<int>(whole));
}

}  // namespace vbrun
