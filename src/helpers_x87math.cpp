// The math helpers the Visual Basic compiler calls with the operand on the x87
// stack: Sin, Cos, Tan, Atn, Exp, Log and Sqr (the C runtime's `_CI*`
// functions), the `_adj_*` stand-ins for the x87 divide, remainder and
// arctangent instructions, and the 64-bit multiply `_allmul`.
//
// On 32-bit x86 with MSVC the exports are the reference's own sequences: the
// operand arrives in st0 and the result is left in st0. The `_CI*` functions
// run at the control word 0x027F (53-bit precision, round to nearest, all
// exceptions masked) and restore the caller's word; Exp works at 0x133F, as the
// reference does. The `_adj_*` helpers exist in the reference to work around
// the Pentium FDIV flaw; on a processor without it they are the single x87
// instruction they stand for, so that is what they are here.
//
// Without an x87 stack to hand over (every architecture but 32-bit x86 with
// MSVC) the exports take their operands as doubles and use the C library.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>

#if defined(_MSC_VER) && defined(_M_IX86)

extern "C" {

// Each _CI function: save the caller's control word, set 0x027F, run the
// instruction; when the operand was out of range for the instruction (C2 set)
// reduce it modulo 2 pi and run it again; restore the control word.

__declspec(naked) void _CIsin() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x27F
        fldcw word ptr [esp + 2]
        fsin
        fnstsw ax
        sahf
        jnp done
        fldpi
        fadd st(0), st(0)
        fxch st(1)
    again:
        fprem1
        fnstsw ax
        sahf
        jp again
        fstp st(1)
        fsin
    done:
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

__declspec(naked) void _CIcos() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x27F
        fldcw word ptr [esp + 2]
        fcos
        fnstsw ax
        sahf
        jnp done
        fldpi
        fadd st(0), st(0)
        fxch st(1)
    again:
        fprem1
        fnstsw ax
        sahf
        jp again
        fstp st(1)
        fcos
    done:
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

__declspec(naked) void _CItan() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x27F
        fldcw word ptr [esp + 2]
        fptan
        fnstsw ax
        sahf
        jnp done
        fldpi
        fadd st(0), st(0)
        fxch st(1)
    again:
        fprem1
        fnstsw ax
        sahf
        jp again
        fstp st(1)
        fptan
    done:
        fstp st(0)
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

__declspec(naked) void _CIatan() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x27F
        fldcw word ptr [esp + 2]
        fld1
        fpatan
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

// e^x = 2^(x log2 e): split the exponent into its integer and fraction parts,
// f2xm1 for the fraction, fscale for the integer.
__declspec(naked) void _CIexp() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x133F
        fldcw word ptr [esp + 2]
        fldl2e
        fmulp st(1), st(0)
        fld st(0)
        frndint
        fsub st(1), st(0)
        fxch st(1)
        f2xm1
        fld1
        faddp st(1), st(0)
        fscale
        fstp st(1)
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

__declspec(naked) void _CIlog() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x27F
        fldcw word ptr [esp + 2]
        fldln2
        fxch st(1)
        fyl2x
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

__declspec(naked) void _CIsqrt() {
    __asm {
        sub esp, 8
        fnstcw word ptr [esp]
        mov word ptr [esp + 2], 0x27F
        fldcw word ptr [esp + 2]
        fsqrt
        fldcw word ptr [esp]
        add esp, 8
        ret
    }
}

// st0 = st0 / operand (or operand / st0 for the "r" forms); the operand is the
// argument on the stack.
__declspec(naked) void _adj_fdiv_m32() {
    __asm {
        fdiv dword ptr [esp + 4]
        ret 4
    }
}
__declspec(naked) void _adj_fdiv_m64() {
    __asm {
        fdiv qword ptr [esp + 4]
        ret 8
    }
}
__declspec(naked) void _adj_fdiv_m16i() {
    __asm {
        fidiv word ptr [esp + 4]
        ret 4
    }
}
__declspec(naked) void _adj_fdiv_m32i() {
    __asm {
        fidiv dword ptr [esp + 4]
        ret 4
    }
}
__declspec(naked) void _adj_fdivr_m32() {
    __asm {
        fdivr dword ptr [esp + 4]
        ret 4
    }
}
__declspec(naked) void _adj_fdivr_m64() {
    __asm {
        fdivr qword ptr [esp + 4]
        ret 8
    }
}
__declspec(naked) void _adj_fdivr_m16i() {
    __asm {
        fidivr word ptr [esp + 4]
        ret 4
    }
}
__declspec(naked) void _adj_fdivr_m32i() {
    __asm {
        fidivr dword ptr [esp + 4]
        ret 4
    }
}
__declspec(naked) void _adj_fprem() {
    __asm {
        fprem
        ret
    }
}
__declspec(naked) void _adj_fprem1() {
    __asm {
        fprem1
        ret
    }
}
__declspec(naked) void _adj_fpatan() {
    __asm {
        fpatan
        ret
    }
}
__declspec(naked) void _adj_fptan() {
    __asm {
        fptan
        ret
    }
}

// The 64-bit product of two 64-bit values (stdcall, 16 bytes of arguments).
__declspec(naked) void _allmul() {
    __asm {
        mov eax, dword ptr [esp + 8]
        mov ecx, dword ptr [esp + 0x10]
        or ecx, eax
        mov ecx, dword ptr [esp + 0xc]
        jne hard
        mov eax, dword ptr [esp + 4]
        mul ecx
        ret 0x10
    hard:
        push ebx
        mul ecx
        mov ebx, eax
        mov eax, dword ptr [esp + 8]
        mul dword ptr [esp + 0x14]
        add ebx, eax
        mov eax, dword ptr [esp + 8]
        mul ecx
        add edx, ebx
        pop ebx
        ret 0x10
    }
}

}  // extern "C"

#else

extern "C" {

double _CIsin(const double value) { return std::sin(value); }
double _CIcos(const double value) { return std::cos(value); }
double _CItan(const double value) { return std::tan(value); }
double _CIatan(const double value) { return std::atan(value); }
double _CIexp(const double value) { return std::exp(value); }
double _CIlog(const double value) { return std::log(value); }
double _CIsqrt(const double value) { return std::sqrt(value); }

// st0 / operand and operand / st0.
double _adj_fdiv_m32(const double st0, const float operand) {
    return st0 / static_cast<double>(operand);
}
double _adj_fdiv_m64(const double st0, const double operand) {
    return st0 / operand;
}
double _adj_fdiv_m16i(const double st0, const std::int16_t operand) {
    return st0 / static_cast<double>(operand);
}
double _adj_fdiv_m32i(const double st0, const std::int32_t operand) {
    return st0 / static_cast<double>(operand);
}
double _adj_fdivr_m32(const double st0, const float operand) {
    return static_cast<double>(operand) / st0;
}
double _adj_fdivr_m64(const double st0, const double operand) {
    return operand / st0;
}
double _adj_fdivr_m16i(const double st0, const std::int16_t operand) {
    return static_cast<double>(operand) / st0;
}
double _adj_fdivr_m32i(const double st0, const std::int32_t operand) {
    return static_cast<double>(operand) / st0;
}

// fprem (the remainder of a truncating division) and fprem1 (the IEEE
// remainder, a rounding division) of st0 by st1; fptan (the tangent) and fpatan
// (the arctangent of st1 / st0).
double _adj_fprem(const double st0, const double st1) {
    return std::fmod(st0, st1);
}
double _adj_fprem1(const double st0, const double st1) {
    return std::remainder(st0, st1);
}
double _adj_fpatan(const double st1, const double st0) {
    return std::atan2(st1, st0);
}
double _adj_fptan(const double value) { return std::tan(value); }

std::int64_t _allmul(const std::int64_t left, const std::int64_t right) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(left) *
                                     static_cast<std::uint64_t>(right));
}

}  // extern "C"

#endif
