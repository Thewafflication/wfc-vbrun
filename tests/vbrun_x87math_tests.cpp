// The math helpers that work on the x87 stack (_CI*, _adj_*, _allmul): on
// 32-bit x86 (MSVC) the reference's sequences are called with the operand on
// the x87 stack; everywhere else the exports take doubles.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

bool close(const double actual, const double wanted) {
    if (std::isnan(wanted)) {
        return std::isnan(actual);
    }
    if (std::isinf(wanted)) {
        return actual == wanted;
    }
    const double scale = std::fabs(wanted) > 1.0 ? std::fabs(wanted) : 1.0;
    return std::fabs(actual - wanted) <= 1e-12 * scale;
}

}  // namespace

extern "C" {
#if defined(_MSC_VER) && defined(_M_IX86)
void _CIsin();
void _CIcos();
void _CItan();
void _CIatan();
void _CIexp();
void _CIlog();
void _CIsqrt();
void _adj_fdiv_m32();
void _adj_fdiv_m64();
void _adj_fdiv_m16i();
void _adj_fdiv_m32i();
void _adj_fdivr_m32();
void _adj_fdivr_m64();
void _adj_fdivr_m16i();
void _adj_fdivr_m32i();
void _adj_fprem();
void _adj_fdiv_r();
void _adj_fprem1();
void _adj_fpatan();
void _adj_fptan();
void _allmul();
#else
double _CIsin(double);
double _CIcos(double);
double _CItan(double);
double _CIatan(double);
double _CIexp(double);
double _CIlog(double);
double _CIsqrt(double);
double _adj_fdiv_m32(double, float);
double _adj_fdiv_m64(double, double);
double _adj_fdiv_m16i(double, std::int16_t);
double _adj_fdiv_m32i(double, std::int32_t);
double _adj_fdivr_m32(double, float);
double _adj_fdivr_m64(double, double);
double _adj_fdivr_m16i(double, std::int16_t);
double _adj_fdivr_m32i(double, std::int32_t);
double _adj_fprem(double, double);
double _adj_fdiv_r(double, double, unsigned);
double _adj_fprem1(double, double);
double _adj_fpatan(double, double);
double _adj_fptan(double);
std::int64_t _allmul(std::int64_t, std::int64_t);
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86)

namespace {

// The unary functions: the operand goes onto the x87 stack, the function is
// called, the result comes off it.
double unary(void (*function)(), double value) {
    double result = 0.0;
    __asm {
        fld value
        call function
        fstp result
    }
    return result;
}

double tangent(const double value) {
    double result = 0.0;
    __asm {
        fld value
        call _adj_fptan
        fstp st(0)
        fstp result
    }
    return result;
}

double div32(void (*function)(), const double dividend, const float operand) {
    double result = 0.0;
    __asm {
        fld dividend
        mov eax, dword ptr operand
        push eax
        call function
        fstp result
    }
    return result;
}

// st0 = a, st1 = b, st2 = c; the form runs and the registers left are stored
// (two after a popping form, three otherwise).
void register_divide(const unsigned form, const bool pops, const double a,
                     const double b, const double c, double out[3]) {
    double r0 = 0.0;
    double r1 = 0.0;
    double r2 = 0.0;
    __asm {
        fld c
        fld b
        fld a
        mov eax, form
        call _adj_fdiv_r
        fstp r0
        fstp r1
    }
    if (!pops) {
        __asm {
            fstp r2
        }
    }
    out[0] = r0;
    out[1] = r1;
    out[2] = r2;
}

double div64(void (*function)(), const double dividend, const double operand) {
    double result = 0.0;
    __asm {
        fld dividend
        push dword ptr [operand + 4]
        push dword ptr [operand]
        call function
        fstp result
    }
    return result;
}

double div16i(void (*function)(), const double dividend, const short operand) {
    double result = 0.0;
    __asm {
        fld dividend
        movsx eax, word ptr operand
        push eax
        call function
        fstp result
    }
    return result;
}

double div32i(void (*function)(), const double dividend, const long operand) {
    double result = 0.0;
    __asm {
        fld dividend
        push operand
        call function
        fstp result
    }
    return result;
}

__int64 multiply64(const __int64 left, const __int64 right) {
    __int64 result = 0;
    __asm {
        push dword ptr [right + 4]
        push dword ptr [right]
        push dword ptr [left + 4]
        push dword ptr [left]
        call _allmul
        mov dword ptr [result], eax
        mov dword ptr [result + 4], edx
    }
    return result;
}

}  // namespace

#endif

namespace {

void functions() {
    const double values[] = {0.0,  0.5,    1.0,  2.0,  3.14159, -0.75,
                             10.0, 100.25, -2.5, 1e-9, 700.0,   1e6};
    for (const double value : values) {
#if defined(_MSC_VER) && defined(_M_IX86)
        expect(close(unary(_CIsin, value), std::sin(value)), "Sin");
        expect(close(unary(_CIcos, value), std::cos(value)), "Cos");
        expect(close(unary(_CItan, value), std::tan(value)), "Tan");
        expect(close(unary(_CIatan, value), std::atan(value)), "Atn");
        expect(close(unary(_CIexp, value), std::exp(value)), "Exp");
        expect(close(unary(_CIsqrt, value), std::sqrt(value)), "Sqr");
        expect(close(unary(_CIlog, value), std::log(value)), "Log");
        expect(close(tangent(value), std::tan(value)), "fptan");
#else
        expect(close(_CIsin(value), std::sin(value)), "Sin");
        expect(close(_CIcos(value), std::cos(value)), "Cos");
        expect(close(_CItan(value), std::tan(value)), "Tan");
        expect(close(_CIatan(value), std::atan(value)), "Atn");
        expect(close(_CIexp(value), std::exp(value)), "Exp");
        expect(close(_CIsqrt(value), std::sqrt(value)), "Sqr");
        expect(close(_CIlog(value), std::log(value)), "Log");
        expect(close(_adj_fptan(value), std::tan(value)), "fptan");
#endif
    }
    // A huge operand needs the range reduction.
    const double big = 1e15;
#if defined(_MSC_VER) && defined(_M_IX86)
    expect(close(unary(_CIsin, big), std::sin(big)), "Sin of a huge value");
    expect(close(unary(_CIcos, big), std::cos(big)), "Cos of a huge value");
    expect(close(unary(_CItan, big), std::tan(big)), "Tan of a huge value");
#else
    expect(close(_CIsin(big), std::sin(big)), "Sin of a huge value");
    expect(close(_CIcos(big), std::cos(big)), "Cos of a huge value");
    expect(close(_CItan(big), std::tan(big)), "Tan of a huge value");
#endif
}

void arithmetic() {
#if defined(_MSC_VER) && defined(_M_IX86)
    expect(div32(_adj_fdiv_m32, 7.0, 2.0F) == 3.5, "fdiv m32");
    expect(div64(_adj_fdiv_m64, 7.0, 2.0) == 3.5, "fdiv m64");
    expect(div16i(_adj_fdiv_m16i, 7.0, 2) == 3.5, "fdiv m16i");
    expect(div32i(_adj_fdiv_m32i, 7.0, 2) == 3.5, "fdiv m32i");
    expect(div32(_adj_fdivr_m32, 2.0, 7.0F) == 3.5, "fdivr m32");
    expect(div64(_adj_fdivr_m64, 2.0, 7.0) == 3.5, "fdivr m64");
    expect(div16i(_adj_fdivr_m16i, 2.0, 7) == 3.5, "fdivr m16i");
    expect(div32i(_adj_fdivr_m32i, 2.0, 7) == 3.5, "fdivr m32i");
    // st0 = 7, st1 = 2, st2 = 4.
    double r[3] = {};
    register_divide(8, false, 7.0, 2.0, 4.0, r);
    expect(r[0] == 3.5 && r[1] == 2.0 && r[2] == 4.0, "fdiv st0, st1");
    register_divide(10, false, 7.0, 2.0, 4.0, r);
    expect(r[0] == 2.0 / 7.0 && r[1] == 2.0 && r[2] == 4.0, "fdivr st0, st1");
    register_divide(12, false, 2.0, 7.0, 4.0, r);
    expect(r[0] == 2.0 && r[1] == 3.5 && r[2] == 4.0, "st1 = st1 / st0");
    register_divide(13, true, 2.0, 7.0, 4.0, r);
    expect(r[0] == 3.5 && r[1] == 4.0, "st1 = st1 / st0, pop");
    register_divide(14, false, 7.0, 2.0, 4.0, r);
    expect(r[0] == 7.0 && r[1] == 3.5 && r[2] == 4.0, "st1 = st0 / st1");
    register_divide(15, true, 7.0, 2.0, 4.0, r);
    expect(r[0] == 3.5 && r[1] == 4.0, "st1 = st0 / st1, pop");
    register_divide(16, false, 7.0, 4.0, 2.0, r);
    expect(r[0] == 3.5 && r[1] == 4.0 && r[2] == 2.0, "fdiv st0, st2");
    register_divide(21, true, 2.0, 4.0, 7.0, r);
    expect(r[0] == 4.0 && r[1] == 3.5, "st2 = st2 / st0, pop");
    register_divide(0, false, 7.0, 2.0, 4.0, r);
    expect(r[0] == 1.0 && r[1] == 2.0, "fdiv st0, st0");
#else
    expect(_adj_fdiv_r(7.0, 2.0, 8) == 3.5, "fdiv st0, st1");
    expect(_adj_fdiv_r(2.0, 7.0, 10) == 3.5, "fdivr st0, st1");
    expect(_adj_fdiv_r(2.0, 7.0, 13) == 3.5, "st1 = st1 / st0, pop");
    expect(_adj_fdiv_r(7.0, 2.0, 15) == 3.5, "st1 = st0 / st1, pop");
    expect(std::isnan(_adj_fdiv_r(7.0, 2.0, 9)), "an invalid form");
    expect(_adj_fdiv_m32(7.0, 2.0F) == 3.5, "fdiv m32");
    expect(_adj_fdiv_m64(7.0, 2.0) == 3.5, "fdiv m64");
    expect(_adj_fdiv_m16i(7.0, 2) == 3.5, "fdiv m16i");
    expect(_adj_fdiv_m32i(7.0, 2) == 3.5, "fdiv m32i");
    expect(_adj_fdivr_m32(2.0, 7.0F) == 3.5, "fdivr m32");
    expect(_adj_fdivr_m64(2.0, 7.0) == 3.5, "fdivr m64");
    expect(_adj_fdivr_m16i(2.0, 7) == 3.5, "fdivr m16i");
    expect(_adj_fdivr_m32i(2.0, 7) == 3.5, "fdivr m32i");
    expect(_adj_fprem(7.5, 2.0) == 1.5, "fprem");
    expect(_adj_fprem1(7.5, 2.0) == -0.5, "fprem1");
    expect(close(_adj_fpatan(1.0, 1.0), std::atan(1.0)), "fpatan");
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86)
std::int64_t _allmul_value(const std::int64_t a, const std::int64_t b) {
    return multiply64(a, b);
}
#else
std::int64_t _allmul_value(const std::int64_t a, const std::int64_t b) {
    return _allmul(a, b);
}
#endif

void multiply() {
    expect(_allmul_value(6, 7) == 42, "_allmul");
    expect(_allmul_value(-6, 7) == -42 && _allmul_value(6, -7) == -42 &&
               _allmul_value(-6, -7) == 42,
           "_allmul signs");
    expect(_allmul_value(0x100000000LL, 3) == 0x300000000LL,
           "_allmul high word");
    expect(_allmul_value(0x12345678LL, 0x9ABCDEF0LL) ==
               0x12345678LL * 0x9ABCDEF0LL,
           "_allmul wide");
    expect(_allmul_value(0x7FFFFFFFFFFFFFFFLL, 2) == -2, "_allmul wraps");
}

}  // namespace

int main() {
    functions();
    arithmetic();
    multiply();
    if (failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::puts("vbrun_x87math_tests: ok");
    return 0;
}
