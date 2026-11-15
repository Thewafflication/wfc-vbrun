// The x87 conversion helpers of the reference, as plain functions.
//
// The Visual Basic compiler leaves the operand of CInt, CLng, CByte, CCur,
// CSng, Int and Fix on the x87 stack (`st0`) and calls a helper that works on
// it. Only 32-bit x86 programs have an x87 stack to hand over, so on x86 the
// exports are the reference's own x87 sequences (helpers_x87.cpp). On every
// other architecture, and for testing, these functions do the same work on a
// `double`: the reference's control words round to nearest even, so every
// result is exactly computable, and an operand that came from a `double` is
// never wider than 53 bits. Each function that fails raises the error the
// reference raises (6, overflow) and returns 0.

#pragma once

#include <cstdint>

namespace vbrun::x87 {

// CInt, CLng and CByte of a floating-point value: round to nearest even, then
// the value must fit (CByte: 0 to 255) or the error is an overflow.
std::int16_t to_i2(double value);
std::int32_t to_i4(double value);
std::uint8_t to_ui1(double value);

// Int and Fix of a value, as a value (floor and truncation) and converted to
// Integer or Long (error 6 when it does not fit).
double floor_value(double value);
double trunc_value(double value);
std::int16_t floor_i2(double value);
std::int32_t floor_i4(double value);
std::int16_t trunc_i2(double value);
std::int32_t trunc_i4(double value);

// The value rounded to the 24-bit mantissa of a Single (the exponent range of
// the x87 is wider than a Single's, so nothing overflows) and to the 53-bit
// mantissa of a Double.
double round_to_single_mantissa(double value);
double round_to_double_mantissa(double value);

// The value stored in a Single or a Double and loaded back: a Single that
// overflows is an overflow error.
double through_single(double value);
double through_double(double value);

// CCur: the value times 10,000 rounded to nearest even, as the 64-bit integer
// the Currency holds (error 6 when it does not fit).
std::int64_t to_currency(double value);

// Compares a Currency (64-bit integer) with a value times 10,000 rounded to
// nearest even: -1, 0 or 1 as the Currency is below, equal to or above it.
// A value that is not a number is an overflow error.
int compare_currency(std::int64_t currency, double value);

// What __vbaFPException does for an x87 status word: error 16 when the stack
// faulted (bit 6), 11 for a division by zero (bit 2), else 6.
void raise_for_status(unsigned status);

}  // namespace vbrun::x87
