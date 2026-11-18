// The x87 conversion helpers: the portable functions on every architecture,
// and on 32-bit x86 (MSVC) the reference's own x87 sequences compared with
// them, value by value.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "../src/vbrun_helpers.hpp"
#include "../src/x87_core.hpp"

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Runs the function and returns the error it raised (0 for none).
template <typename Function>
int error_of(Function function) {
    vbrun::clear_error();
    function();
    const int error = vbrun::last_error();
    vbrun::clear_error();
    return error;
}

void integers() {
    namespace x = vbrun::x87;
    expect(x::to_i2(2.5) == 2 && x::to_i2(3.5) == 4 && x::to_i2(-2.5) == -2 &&
               x::to_i2(-3.5) == -4,
           "CInt rounds to even");
    expect(x::to_i2(32767.4) == 32767 && x::to_i2(-32768.5) == -32768,
           "CInt at the limits");
    expect(error_of([] { x::to_i2(32767.5); }) == 6, "CInt(32767.5) overflows");
    expect(error_of([] { x::to_i2(kNan); }) == 6, "CInt(NaN) overflows");
    expect(error_of([] { x::to_i2(kInf); }) == 6, "CInt(Inf) overflows");
    expect(x::to_i4(2147483647.4) == 2147483647 &&
               x::to_i4(-2147483648.4) == INT32_MIN,
           "CLng at the limits");
    expect(error_of([] { x::to_i4(2147483647.5); }) == 6,
           "CLng(2147483647.5) overflows");
    expect(x::to_ui1(255.4) == 255 && x::to_ui1(0.5) == 0 &&
               x::to_ui1(-0.4) == 0,
           "CByte");
    expect(error_of([] { x::to_ui1(255.5); }) == 6, "CByte(255.5) overflows");
    expect(error_of([] { x::to_ui1(-0.6); }) == 6, "CByte(-0.6) overflows");
    expect(x::floor_i2(-2.5) == -3 && x::floor_i2(2.5) == 2 &&
               x::trunc_i2(-2.5) == -2 && x::trunc_i2(2.9) == 2,
           "Int and Fix to Integer");
    expect(x::floor_i4(-0.5) == -1 && x::trunc_i4(-0.5) == 0,
           "Int and Fix to Long");
    expect(x::floor_value(-1.5) == -2.0 && x::trunc_value(-1.5) == -1.0,
           "Int and Fix as values");
    expect(error_of([] { x::floor_i2(40000.0); }) == 6,
           "Int to Integer overflows");
}

void floating_point() {
    namespace x = vbrun::x87;
    expect(x::round_to_single_mantissa(16777217.0) == 16777216.0 &&
               x::round_to_single_mantissa(16777219.0) == 16777220.0,
           "a Single keeps 24 bits (ties to even)");
    expect(x::round_to_single_mantissa(0.1) ==
               static_cast<double>(static_cast<float>(0.1)),
           "0.1 as a Single");
    expect(x::round_to_single_mantissa(1e300) != 0.0 &&
               std::isfinite(x::round_to_single_mantissa(1e300)),
           "the exponent range is not a Single's");
    expect(x::round_to_double_mantissa(0.1) == 0.1, "a Double is as it is");
    expect(x::through_single(3.0e38) == static_cast<double>(3.0e38F),
           "stored in a Single");
    expect(error_of([] { x::through_single(1e39); }) == 6,
           "a Single overflows");
    expect(x::through_single(kInf) == kInf, "an infinity stays");
}

void currency() {
    namespace x = vbrun::x87;
    expect(x::to_currency(1.5) == 15000 && x::to_currency(2.5) == 25000 &&
               x::to_currency(-1.5) == -15000,
           "CCur");
    expect(x::to_currency(1.23456) == 12346 &&
               x::to_currency(-1.23456) == -12346,
           "CCur rounds");
    expect(x::to_currency(-0.00004) == 0, "a small value is zero");
    expect(x::to_currency(922337203685477.0) == 9223372036854770000LL,
           "CCur near the limit");
    expect(error_of([] { x::to_currency(922337203685478.0); }) == 6,
           "CCur overflows");
    expect(error_of([] { x::to_currency(kNan); }) == 6, "CCur(NaN) overflows");
    expect(x::compare_currency(15000, 1.5) == 0 &&
               x::compare_currency(15000, 1.6) == -1 &&
               x::compare_currency(20000, 1.5) == 1,
           "comparing a Currency");
    expect(x::compare_currency(0, 1e30) == -1 &&
               x::compare_currency(0, -1e30) == 1 &&
               x::compare_currency(0, kInf) == -1 &&
               x::compare_currency(0, -kInf) == 1,
           "comparing with a huge value");
    expect(error_of([] { x::compare_currency(0, kNan); }) == 6,
           "comparing with NaN");
    expect(error_of([] { x::raise_for_status(0x40); }) == 16 &&
               error_of([] { x::raise_for_status(4); }) == 11 &&
               error_of([] { x::raise_for_status(1); }) == 6,
           "the error for an x87 status");
}

}  // namespace

#if defined(_MSC_VER) && defined(_M_IX86)

// The reference's own sequences, called with the operand on the x87 stack.
extern "C" {
void __vbaFpI2();
void __vbaFpI4();
void __vbaFpUI1();
void __vbaFpR4();
void __vbaFpR8();
void __vbaFpCSngR4();
void __vbaFpCSngR8();
void __vbaFpCDblR4();
void __vbaFpCDblR8();
void __vbaFPException();
void __vbaFpCy();
void __vbaFpCmpCy();
void __vbaFPInt();
void __vbaFPFix();
void __vbaR8IntI2();
void __vbaR8IntI4();
void __vbaR8FixI2();
void __vbaR8FixI4();
}

namespace {

SHORT asm_i2(double x) {
    int r = 0;
    __asm {
        fld qword ptr [x]
        call __vbaFpI2
        mov r, eax
    }
    return static_cast<SHORT>(r);
}

LONG asm_i4(double x) {
    LONG r = 0;
    __asm {
        fld qword ptr [x]
        call __vbaFpI4
        mov r, eax
    }
    return r;
}

BYTE asm_ui1(double x) {
    int r = 0;
    __asm {
        fld qword ptr [x]
        call __vbaFpUI1
        mov r, eax
    }
    return static_cast<BYTE>(r);
}

double asm_st0(double x, void (*function)()) {
    double r = 0;
    __asm {
        fld qword ptr [x]
        call function
        fstp qword ptr [r]
    }
    return r;
}

std::int64_t asm_cy(double x) {
    std::int64_t r = 0;
    __asm {
        fld qword ptr [x]
        call __vbaFpCy
        mov dword ptr [r], eax
        mov dword ptr [r + 4], edx
    }
    return r;
}

LONG asm_cmp(std::int64_t cy, double x) {
    LONG r = 0;
    __asm {
        fld qword ptr [x]
        push dword ptr [cy + 4]
        push dword ptr [cy]
        call __vbaFpCmpCy
        mov r, eax
    }
    return r;
}

SHORT asm_r8_i2(double x, void (*function)()) {
    int r = 0;
    __asm {
        fld qword ptr [x]
        call function
        mov r, eax
    }
    return static_cast<SHORT>(r);
}

LONG asm_r8_i4(double x, void (*function)()) {
    LONG r = 0;
    __asm {
        fld qword ptr [x]
        call function
        mov r, eax
    }
    return r;
}

// The error raised and the value only where there is none.
struct Outcome {
    int error;
    std::int64_t value;
};

template <typename Function>
Outcome run(Function function) {
    vbrun::clear_error();
    const std::int64_t value = function();
    const int error = vbrun::last_error();
    vbrun::clear_error();
    return {error, error == 0 ? value : 0};
}

bool same(const Outcome& a, const Outcome& b) {
    return a.error == b.error && a.value == b.value;
}

std::int64_t bits_of(const double value) {
    std::int64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void sequences() {
    namespace x = vbrun::x87;
    const std::vector<double> values = {
        0.0,           -0.0,           0.5,
        -0.5,          1.5,            2.5,
        -2.5,          0.1,            255.4,
        255.5,         256.0,          -0.4,
        -0.6,          32767.4,        32767.5,
        -32768.5,      -32769.0,       40000.0,
        2147483647.4,  2147483647.5,   -2147483648.4,
        -2147483649.0, 1e10,           1.23456,
        -1.23456,      16777217.0,     16777219.0,
        3.0e38,        1e39,           1e300,
        922337203685477.0, 922337203685478.0, 1e15,
        -1e15,         kInf,           -kInf,
        kNan};
    for (const double v : values) {
        char label[96];
        std::snprintf(label, sizeof(label), "x87 sequences for %.17g", v);
        expect(same(run([&] { return std::int64_t{asm_i2(v)}; }),
                    run([&] { return std::int64_t{x::to_i2(v)}; })),
               label);
        expect(same(run([&] { return std::int64_t{asm_i4(v)}; }),
                    run([&] { return std::int64_t{x::to_i4(v)}; })),
               label);
        expect(same(run([&] { return std::int64_t{asm_ui1(v)}; }),
                    run([&] { return std::int64_t{x::to_ui1(v)}; })),
               label);
        expect(same(run([&] { return asm_cy(v); }),
                    run([&] { return x::to_currency(v); })),
               label);
        for (const std::int64_t cy :
             {std::int64_t{0}, std::int64_t{15000}, std::int64_t{12346},
              INT64_MAX, INT64_MIN}) {
            expect(same(run([&] { return std::int64_t{asm_cmp(cy, v)}; }),
                        run([&] {
                            return std::int64_t{x::compare_currency(cy, v)};
                        })),
                   label);
        }
        expect(same(run([&] {
                        return std::int64_t{asm_r8_i2(v, __vbaR8IntI2)};
                    }),
                    run([&] { return std::int64_t{x::floor_i2(v)}; })),
               label);
        expect(same(run([&] {
                        return std::int64_t{asm_r8_i4(v, __vbaR8IntI4)};
                    }),
                    run([&] { return std::int64_t{x::floor_i4(v)}; })),
               label);
        expect(same(run([&] {
                        return std::int64_t{asm_r8_i2(v, __vbaR8FixI2)};
                    }),
                    run([&] { return std::int64_t{x::trunc_i2(v)}; })),
               label);
        expect(same(run([&] {
                        return std::int64_t{asm_r8_i4(v, __vbaR8FixI4)};
                    }),
                    run([&] { return std::int64_t{x::trunc_i4(v)}; })),
               label);
        if (std::isfinite(v)) {
            expect(bits_of(asm_st0(v, __vbaFPInt)) ==
                       bits_of(x::floor_value(v)),
                   label);
            expect(bits_of(asm_st0(v, __vbaFPFix)) ==
                       bits_of(x::trunc_value(v)),
                   label);
            expect(bits_of(asm_st0(v, __vbaFpR4)) ==
                       bits_of(x::round_to_single_mantissa(v)),
                   label);
            expect(bits_of(asm_st0(v, __vbaFpR8)) ==
                       bits_of(x::round_to_double_mantissa(v)),
                   label);
            expect(bits_of(asm_st0(v, __vbaFpCDblR8)) == bits_of(v), label);
            expect(bits_of(asm_st0(v, __vbaFpCDblR4)) ==
                       bits_of(x::through_double(v)),
                   label);
            vbrun::clear_error();
            const double single = asm_st0(v, __vbaFpCSngR4);
            const int error = vbrun::last_error();
            vbrun::clear_error();
            const int core_error = error_of([&] { x::through_single(v); });
            expect(error == core_error, label);
            if (error == 0) {
                expect(bits_of(single) == bits_of(x::through_single(v)),
                       label);
            }
            vbrun::clear_error();
            const double single8 = asm_st0(v, __vbaFpCSngR8);
            const int error8 = vbrun::last_error();
            vbrun::clear_error();
            expect(error8 == core_error &&
                       (error8 != 0 || bits_of(single8) == bits_of(single)),
                   label);
        }
    }
}

// The error __vbaFPException raises after a / b (with the exceptions
// masked, the division leaves its flag in the status word).
int fp_exception_after(double a, double b) {
    vbrun::clear_error();
    __asm {
        fnclex
        fld qword ptr [a]
        fdiv qword ptr [b]
        fstp st(0)
        call __vbaFPException
        fnclex
    }
    const int error = vbrun::last_error();
    vbrun::clear_error();
    return error;
}

void fp_exception() {
    expect(fp_exception_after(1.0, 0.0) == 11,
           "__vbaFPException after a division by zero is error 11");
    expect(fp_exception_after(0.0, 0.0) == 6,
           "__vbaFPException after an invalid operation is error 6");
}

}  // namespace

#else

extern "C" {
double __vbaFpCSngR8(double value);
double __vbaFpCDblR4(double value);
void __vbaFPException();
}

namespace {

// Elsewhere the exports take the operand as a double, and there is no x87
// status to read: __vbaFPException is the overflow error.
void exports_elsewhere() {
    expect(__vbaFpCSngR8(1.1) == static_cast<double>(1.1F),
           "__vbaFpCSngR8 rounds to a Single");
    expect(error_of([] { __vbaFpCSngR8(1e300); }) == 6,
           "__vbaFpCSngR8 of a Double too large is error 6");
    expect(__vbaFpCDblR4(0.5) == 0.5, "__vbaFpCDblR4 keeps the value");
    expect(error_of([] { __vbaFPException(); }) == 6,
           "__vbaFPException is error 6");
}

}  // namespace

#endif

int main() {
    integers();
    floating_point();
    currency();
#if defined(_MSC_VER) && defined(_M_IX86)
    sequences();
    fp_exception();
#else
    exports_elsewhere();
#endif
    if (failures == 0) {
        std::printf("vbrun x87: all checks passed\n");
        return 0;
    }
    return 1;
}
