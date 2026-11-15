// The helpers that work on the x87 stack (REQ-0101, REQ-0104): CInt, CLng,
// CByte, CSng, CDbl and CCur of a floating-point operand, Int and Fix, and the
// Currency comparison.
//
// Written from the disassembly of the installed MSVBVM60 6.00.9848. The
// reference runs these at the control word 0x133F (64-bit precision, round to
// nearest even, every exception masked) and sets 0x123F (53-bit), 0x103F
// (24-bit), 0x173F (round down) and 0x1F3F (truncate) for the conversions
// that need them. On x86 the exports are the reference's own sequences; the
// portable functions (x87_core.hpp) do the same work on a double.

#include "x87_core.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

#include "vbrun_helpers.hpp"

namespace vbrun::x87 {
namespace {

// The value times 10,000, rounded to nearest even, worked out exactly: a
// double has a 53-bit mantissa and 10,000 is 625 * 2^4, so the product needs
// 63 bits and the rounding is an integer shift.
struct Rounded {
    bool negative = false;
    bool huge = false;  // too large for the magnitude below
    std::uint64_t magnitude = 0;
};

Rounded times_ten_thousand(const double value) {
    Rounded result;
    result.negative = std::signbit(value);
    const double magnitude = std::fabs(value);
    if (magnitude == 0.0) {
        return result;
    }
    int exponent = 0;
    const double fraction = std::frexp(magnitude, &exponent);
    const auto mantissa =
        static_cast<std::uint64_t>(std::ldexp(fraction, 53));
    const std::uint64_t product = mantissa * 625U;  // below 2^63
    const int shift = exponent - 53 + 4;
    if (shift >= 0) {
        if (shift >= 64 || product > (UINT64_MAX >> shift)) {
            result.huge = true;
        } else {
            result.magnitude = product << shift;
        }
        return result;
    }
    const int down = -shift;
    if (down >= 64) {
        return result;  // below one half
    }
    std::uint64_t quotient = product >> down;
    const std::uint64_t remainder = product & ((std::uint64_t{1} << down) - 1U);
    const std::uint64_t half = std::uint64_t{1} << (down - 1);
    if (remainder > half || (remainder == half && (quotient & 1U) != 0U)) {
        ++quotient;
    }
    result.magnitude = quotient;
    return result;
}

// The signed 64-bit value of a rounded product, if it has one.
bool fits_int64(const Rounded& rounded, std::int64_t& out) {
    if (rounded.huge) {
        return false;
    }
    if (rounded.negative) {
        if (rounded.magnitude > (std::uint64_t{1} << 63)) {
            return false;
        }
        out = rounded.magnitude == (std::uint64_t{1} << 63)
                  ? INT64_MIN
                  : -static_cast<std::int64_t>(rounded.magnitude);
        return true;
    }
    if (rounded.magnitude > static_cast<std::uint64_t>(INT64_MAX)) {
        return false;
    }
    out = static_cast<std::int64_t>(rounded.magnitude);
    return true;
}

bool fits(const double rounded, const double low, const double high) {
    return rounded >= low && rounded <= high;  // false for a NaN
}

// Round to nearest even on `bits` bits of mantissa (the exponent range stays
// that of the x87, so only the mantissa changes).
double round_mantissa(const double value, const int bits) {
    if (value == 0.0 || !std::isfinite(value)) {
        return value;
    }
    int exponent = 0;
    const double fraction = std::frexp(value, &exponent);
    const double scaled = std::nearbyint(std::ldexp(fraction, bits));
    return std::ldexp(scaled, exponent - bits);
}

}  // namespace

std::int16_t to_i2(const double value) {
    const double rounded = std::nearbyint(value);
    if (!fits(rounded, -32768.0, 32767.0)) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    return static_cast<std::int16_t>(rounded);
}

std::int32_t to_i4(const double value) {
    const double rounded = std::nearbyint(value);
    if (!fits(rounded, -2147483648.0, 2147483647.0)) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    return static_cast<std::int32_t>(rounded);
}

std::uint8_t to_ui1(const double value) {
    const double rounded = std::nearbyint(value);
    if (!fits(rounded, 0.0, 255.0)) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    return static_cast<std::uint8_t>(rounded);
}

double floor_value(const double value) { return std::floor(value); }

double trunc_value(const double value) { return std::trunc(value); }

std::int16_t floor_i2(const double value) {
    return to_i2(std::floor(value));
}

std::int32_t floor_i4(const double value) {
    return to_i4(std::floor(value));
}

std::int16_t trunc_i2(const double value) {
    return to_i2(std::trunc(value));
}

std::int32_t trunc_i4(const double value) {
    return to_i4(std::trunc(value));
}

double round_to_single_mantissa(const double value) {
    return round_mantissa(value, 24);
}

double round_to_double_mantissa(const double value) {
    return round_mantissa(value, 53);
}

double through_single(const double value) {
    const auto stored = static_cast<float>(value);
    if (std::isinf(stored) && std::isfinite(value)) {
        static_cast<void>(vbrun::raise(6));
        return 0.0;
    }
    return static_cast<double>(stored);
}

double through_double(const double value) { return value; }

std::int64_t to_currency(const double value) {
    if (!std::isfinite(value)) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    std::int64_t result = 0;
    if (!fits_int64(times_ten_thousand(value), result)) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    return result;
}

int compare_currency(const std::int64_t currency, const double value) {
    if (std::isnan(value)) {
        static_cast<void>(vbrun::raise(6));
        return 0;
    }
    if (std::isinf(value)) {
        return value > 0.0 ? -1 : 1;
    }
    const Rounded rounded = times_ten_thousand(value);
    std::int64_t other = 0;
    if (!fits_int64(rounded, other)) {
        return rounded.negative ? 1 : -1;
    }
    return currency < other ? -1 : (currency > other ? 1 : 0);
}

void raise_for_status(const unsigned status) {
    if ((status & 0x40U) != 0U) {
        static_cast<void>(vbrun::raise(16));
    } else if ((status & 4U) != 0U) {
        static_cast<void>(vbrun::raise(11));
    } else {
        static_cast<void>(vbrun::raise(6));
    }
}

}  // namespace vbrun::x87

#if defined(_MSC_VER) && defined(_M_IX86)

// The reference's own x87 sequences: the operand arrives in st0 and the result
// is in EAX, EDX:EAX or st0. A failure (an x87 exception flag in the status
// word) raises the error and leaves the neutral result.

extern "C" {

// The exception flags of the x87 stay set until they are cleared, so the
// error clears them (the reference's error path resets the unit) or the next
// conversion would fail on the old flags.
void __cdecl vbrun_x87_fail(unsigned status) {
    __asm fnclex
    vbrun::x87::raise_for_status(status);
}

void __cdecl vbrun_x87_overflow() { static_cast<void>(vbrun::raise(6)); }

extern const unsigned short vbrun_cw_default;
extern const unsigned short vbrun_cw_double;
extern const unsigned short vbrun_cw_single;
extern const unsigned short vbrun_cw_trunc;
extern const unsigned short vbrun_cw_floor;
extern const float vbrun_ten_thousand;

const unsigned short vbrun_cw_default = 0x133F;
const unsigned short vbrun_cw_double = 0x123F;
const unsigned short vbrun_cw_single = 0x103F;
const unsigned short vbrun_cw_trunc = 0x1F3F;
const unsigned short vbrun_cw_floor = 0x173F;
const float vbrun_ten_thousand = 10000.0F;

// CInt of st0.
__declspec(naked) void __vbaFpI2() {
    __asm {
        frndint
        sub esp, 0xc
        fist word ptr [esp]
        fistp qword ptr [esp + 4]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        add esp, 8
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 16
        xor eax, eax
        ret
    }
}

// CLng of st0.
__declspec(naked) void __vbaFpI4() {
    __asm {
        frndint
        sub esp, 0xc
        fist dword ptr [esp]
        fistp qword ptr [esp + 4]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        add esp, 8
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 16
        xor eax, eax
        ret
    }
}

// CByte of st0.
__declspec(naked) void __vbaFpUI1() {
    __asm {
        frndint
        sub esp, 0xc
        fist dword ptr [esp]
        fistp qword ptr [esp + 4]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        add esp, 8
        test eax, 0xffffff00
        jne overflow
        ret
    overflow:
        call vbrun_x87_overflow
        xor eax, eax
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 16
        xor eax, eax
        ret
    }
}

// CSng and CDbl of st0 kept on the stack at the precision of the type.
__declspec(naked) void __vbaFpR4() {
    __asm {
        fld1
        fldcw word ptr [vbrun_cw_single]
        fmulp st(1), st
        wait
        fnstsw ax
        fldcw word ptr [vbrun_cw_default]
        ret
    }
}

__declspec(naked) void __vbaFpR8() {
    __asm {
        fld1
        fldcw word ptr [vbrun_cw_double]
        fmulp st(1), st
        wait
        fnstsw ax
        fldcw word ptr [vbrun_cw_default]
        ret
    }
}

// st0 stored in a Single or a Double and loaded again.
__declspec(naked) void __vbaFpCSngR4() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        fld dword ptr [esp]
        add esp, 4
        fnstsw ax
        test al, 0xd
        jne failed
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 4
        ret
    }
}

__declspec(naked) void __vbaFpCSngR8() {
    __asm {
        sub esp, 4
        fstp dword ptr [esp]
        fld dword ptr [esp]
        add esp, 4
        fnstsw ax
        test al, 0xd
        jne failed
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 4
        ret
    }
}

__declspec(naked) void __vbaFpCDblR4() {
    __asm {
        sub esp, 8
        fstp qword ptr [esp]
        fld qword ptr [esp]
        add esp, 8
        fnstsw ax
        test al, 0xd
        jne failed
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 4
        ret
    }
}

__declspec(naked) void __vbaFpCDblR8() {
    __asm {
        sub esp, 8
        fstp qword ptr [esp]
        fld qword ptr [esp]
        add esp, 8
        fnstsw ax
        test al, 0xd
        jne failed
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 4
        ret
    }
}

// CCur of st0: the Currency in EDX:EAX.
__declspec(naked) void __vbaFpCy() {
    __asm {
        fldcw word ptr [vbrun_cw_default]
        fmul dword ptr [vbrun_ten_thousand]
        sub esp, 8
        fistp qword ptr [esp]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        pop edx
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 12
        xor eax, eax
        xor edx, edx
        ret
    }
}

// Compares the Currency on the stack with st0 * 10,000 rounded.
__declspec(naked) void __vbaFpCmpCy() {
    __asm {
        fldcw word ptr [vbrun_cw_default]
        fmul dword ptr [vbrun_ten_thousand]
        frndint
        fild qword ptr [esp + 4]
        fcompp
        fnstsw ax
        test al, 0xd
        jne failed
        sahf
        mov eax, 0
        je done
        sbb eax, eax
        sbb eax, -1
    done:
        ret 8
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 4
        xor eax, eax
        ret 8
    }
}

// Int and Fix of st0, left on the stack.
__declspec(naked) void __vbaFPInt() {
    __asm {
        wait
        fnstsw ax
        fldcw word ptr [vbrun_cw_floor]
        frndint
        fldcw word ptr [vbrun_cw_default]
        ret
    }
}

__declspec(naked) void __vbaFPFix() {
    __asm {
        wait
        fnstsw ax
        fldcw word ptr [vbrun_cw_trunc]
        frndint
        fldcw word ptr [vbrun_cw_default]
        ret
    }
}

// The error for the x87 status word of the moment.
__declspec(naked) void __vbaFPException() {
    __asm {
        fnstsw ax
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 4
        ret
    }
}

// Int and Fix of st0 as an Integer or a Long.
__declspec(naked) void __vbaR8IntI2() {
    __asm {
        call __vbaFPInt
        sub esp, 4
        fistp word ptr [esp]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 8
        xor eax, eax
        ret
    }
}

__declspec(naked) void __vbaR8IntI4() {
    __asm {
        call __vbaFPInt
        sub esp, 4
        fistp dword ptr [esp]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 8
        xor eax, eax
        ret
    }
}

__declspec(naked) void __vbaR8FixI2() {
    __asm {
        call __vbaFPFix
        sub esp, 4
        fistp word ptr [esp]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 8
        xor eax, eax
        ret
    }
}

__declspec(naked) void __vbaR8FixI4() {
    __asm {
        call __vbaFPFix
        sub esp, 4
        fistp dword ptr [esp]
        fnstsw ax
        test al, 0xd
        jne failed
        pop eax
        ret
    failed:
        movzx eax, ax
        push eax
        call vbrun_x87_fail
        add esp, 8
        xor eax, eax
        ret
    }
}

}  // extern "C"

#else

// Without an x87 stack to hand over (every architecture but 32-bit x86 with
// MSVC) the exports take the operand as a double.

extern "C" {

SHORT __vbaFpI2(const double value) { return vbrun::x87::to_i2(value); }
LONG __vbaFpI4(const double value) { return vbrun::x87::to_i4(value); }
BYTE __vbaFpUI1(const double value) { return vbrun::x87::to_ui1(value); }
double __vbaFpR4(const double value) {
    return vbrun::x87::round_to_single_mantissa(value);
}
double __vbaFpR8(const double value) {
    return vbrun::x87::round_to_double_mantissa(value);
}
double __vbaFpCSngR4(const double value) {
    return vbrun::x87::through_single(value);
}
double __vbaFpCSngR8(const double value) {
    return vbrun::x87::through_single(value);
}
double __vbaFpCDblR4(const double value) {
    return vbrun::x87::through_double(value);
}
double __vbaFpCDblR8(const double value) {
    return vbrun::x87::through_double(value);
}
std::int64_t __vbaFpCy(const double value) {
    return vbrun::x87::to_currency(value);
}
LONG __vbaFpCmpCy(const std::int64_t currency, const double value) {
    return vbrun::x87::compare_currency(currency, value);
}
double __vbaFPInt(const double value) {
    return vbrun::x87::floor_value(value);
}
double __vbaFPFix(const double value) {
    return vbrun::x87::trunc_value(value);
}
void __vbaFPException() { vbrun::x87::raise_for_status(0); }
SHORT __vbaR8IntI2(const double value) {
    return vbrun::x87::floor_i2(value);
}
LONG __vbaR8IntI4(const double value) {
    return vbrun::x87::floor_i4(value);
}
SHORT __vbaR8FixI2(const double value) {
    return vbrun::x87::trunc_i2(value);
}
LONG __vbaR8FixI4(const double value) {
    return vbrun::x87::trunc_i4(value);
}

}  // extern "C"

#endif
