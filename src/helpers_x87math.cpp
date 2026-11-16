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
// _adj_fdiv_r: a divide between st0 and another register. eax holds the form:
// the register in bits 3-5 and the operation in bits 0-2 (0: st0 = st0 / sti,
// 2: st0 = sti / st0, 4: sti = sti / st0, 5: the same and pop, 6: sti = st0 /
// sti, 7: the same and pop; 1 and 3 are invalid and trap as in the reference).
// The reference routes these through its Pentium-divide workaround; the result
// is that of the plain instruction.
__declspec(naked) void _adj_fdiv_r() {
    __asm {
        and eax, 0x3f
        cmp eax, 0
        je form0
        cmp eax, 1
        je form1
        cmp eax, 2
        je form2
        cmp eax, 3
        je form3
        cmp eax, 4
        je form4
        cmp eax, 5
        je form5
        cmp eax, 6
        je form6
        cmp eax, 7
        je form7
        cmp eax, 8
        je form8
        cmp eax, 9
        je form9
        cmp eax, 10
        je form10
        cmp eax, 11
        je form11
        cmp eax, 12
        je form12
        cmp eax, 13
        je form13
        cmp eax, 14
        je form14
        cmp eax, 15
        je form15
        cmp eax, 16
        je form16
        cmp eax, 17
        je form17
        cmp eax, 18
        je form18
        cmp eax, 19
        je form19
        cmp eax, 20
        je form20
        cmp eax, 21
        je form21
        cmp eax, 22
        je form22
        cmp eax, 23
        je form23
        cmp eax, 24
        je form24
        cmp eax, 25
        je form25
        cmp eax, 26
        je form26
        cmp eax, 27
        je form27
        cmp eax, 28
        je form28
        cmp eax, 29
        je form29
        cmp eax, 30
        je form30
        cmp eax, 31
        je form31
        cmp eax, 32
        je form32
        cmp eax, 33
        je form33
        cmp eax, 34
        je form34
        cmp eax, 35
        je form35
        cmp eax, 36
        je form36
        cmp eax, 37
        je form37
        cmp eax, 38
        je form38
        cmp eax, 39
        je form39
        cmp eax, 40
        je form40
        cmp eax, 41
        je form41
        cmp eax, 42
        je form42
        cmp eax, 43
        je form43
        cmp eax, 44
        je form44
        cmp eax, 45
        je form45
        cmp eax, 46
        je form46
        cmp eax, 47
        je form47
        cmp eax, 48
        je form48
        cmp eax, 49
        je form49
        cmp eax, 50
        je form50
        cmp eax, 51
        je form51
        cmp eax, 52
        je form52
        cmp eax, 53
        je form53
        cmp eax, 54
        je form54
        cmp eax, 55
        je form55
        cmp eax, 56
        je form56
        cmp eax, 57
        je form57
        cmp eax, 58
        je form58
        cmp eax, 59
        je form59
        cmp eax, 60
        je form60
        cmp eax, 61
        je form61
        cmp eax, 62
        je form62
        cmp eax, 63
        je form63
        ret
    form0:
        _emit 0xD8
        _emit 0xF0
        ret
    form1:
        int 6
    form2:
        _emit 0xD8
        _emit 0xF8
        ret
    form3:
        int 6
    form4:
        _emit 0xDC
        _emit 0xF8
        ret
    form5:
        _emit 0xDE
        _emit 0xF8
        ret
    form6:
        _emit 0xDC
        _emit 0xF0
        ret
    form7:
        _emit 0xDE
        _emit 0xF0
        ret
    form8:
        _emit 0xD8
        _emit 0xF1
        ret
    form9:
        int 6
    form10:
        _emit 0xD8
        _emit 0xF9
        ret
    form11:
        int 6
    form12:
        _emit 0xDC
        _emit 0xF9
        ret
    form13:
        _emit 0xDE
        _emit 0xF9
        ret
    form14:
        _emit 0xDC
        _emit 0xF1
        ret
    form15:
        _emit 0xDE
        _emit 0xF1
        ret
    form16:
        _emit 0xD8
        _emit 0xF2
        ret
    form17:
        int 6
    form18:
        _emit 0xD8
        _emit 0xFA
        ret
    form19:
        int 6
    form20:
        _emit 0xDC
        _emit 0xFA
        ret
    form21:
        _emit 0xDE
        _emit 0xFA
        ret
    form22:
        _emit 0xDC
        _emit 0xF2
        ret
    form23:
        _emit 0xDE
        _emit 0xF2
        ret
    form24:
        _emit 0xD8
        _emit 0xF3
        ret
    form25:
        int 6
    form26:
        _emit 0xD8
        _emit 0xFB
        ret
    form27:
        int 6
    form28:
        _emit 0xDC
        _emit 0xFB
        ret
    form29:
        _emit 0xDE
        _emit 0xFB
        ret
    form30:
        _emit 0xDC
        _emit 0xF3
        ret
    form31:
        _emit 0xDE
        _emit 0xF3
        ret
    form32:
        _emit 0xD8
        _emit 0xF4
        ret
    form33:
        int 6
    form34:
        _emit 0xD8
        _emit 0xFC
        ret
    form35:
        int 6
    form36:
        _emit 0xDC
        _emit 0xFC
        ret
    form37:
        _emit 0xDE
        _emit 0xFC
        ret
    form38:
        _emit 0xDC
        _emit 0xF4
        ret
    form39:
        _emit 0xDE
        _emit 0xF4
        ret
    form40:
        _emit 0xD8
        _emit 0xF5
        ret
    form41:
        int 6
    form42:
        _emit 0xD8
        _emit 0xFD
        ret
    form43:
        int 6
    form44:
        _emit 0xDC
        _emit 0xFD
        ret
    form45:
        _emit 0xDE
        _emit 0xFD
        ret
    form46:
        _emit 0xDC
        _emit 0xF5
        ret
    form47:
        _emit 0xDE
        _emit 0xF5
        ret
    form48:
        _emit 0xD8
        _emit 0xF6
        ret
    form49:
        int 6
    form50:
        _emit 0xD8
        _emit 0xFE
        ret
    form51:
        int 6
    form52:
        _emit 0xDC
        _emit 0xFE
        ret
    form53:
        _emit 0xDE
        _emit 0xFE
        ret
    form54:
        _emit 0xDC
        _emit 0xF6
        ret
    form55:
        _emit 0xDE
        _emit 0xF6
        ret
    form56:
        _emit 0xD8
        _emit 0xF7
        ret
    form57:
        int 6
    form58:
        _emit 0xD8
        _emit 0xFF
        ret
    form59:
        int 6
    form60:
        _emit 0xDC
        _emit 0xFF
        ret
    form61:
        _emit 0xDE
        _emit 0xFF
        ret
    form62:
        _emit 0xDC
        _emit 0xF7
        ret
    form63:
        _emit 0xDE
        _emit 0xF7
        ret
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

// The register divide: the quotient the form stores (see the x86 version for
// the forms); an invalid form gives a NaN.
double _adj_fdiv_r(const double st0, const double sti, const unsigned form) {
    switch (form & 7U) {
        case 0:
        case 6:
        case 7:
            return st0 / sti;
        case 2:
        case 4:
        case 5:
            return sti / st0;
        default:
            return std::nan("");
    }
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
