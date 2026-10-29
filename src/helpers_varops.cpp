// REQ-0101: the operators of the language on Variants (+ - * / \ Mod ^ And Or
// Xor Eqv Imp Not, negation, Abs, Fix, Int) and the comparisons of Variants.
//
// A compiled program pushes the operands right first: the helper receives
// (result, right operand, left operand); the unary ones (result, operand).
// The operators are the Automation ones (VarAdd, ...). A failure is a Visual
// Basic error of the thread; the helper returns the result Variant as it is.

#include "vbrun_helpers.hpp"

namespace {

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

VARIANT* finish(const HRESULT status, VARIANT* const result) {
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return result;
}

// The ordering of two Variants: VARCMP_LT (0), VARCMP_EQ (1), VARCMP_GT (2)
// or VARCMP_NULL (3); an error when they cannot be compared.
HRESULT order(VARIANT* const left, VARIANT* const right, const ULONG flags) {
    return VarCmp(left, right, language_locale(), flags);
}

// The truth of an operator for an ordering (the table of the reference).
bool holds(const int ordering, const int relation) {
    if (ordering == VARCMP_NULL) {
        return false;
    }
    switch (relation) {
        case 0:  // =
            return ordering == VARCMP_EQ;
        case 1:  // <>
            return ordering != VARCMP_EQ;
        case 2:  // <
            return ordering == VARCMP_LT;
        case 3:  // <=
            return ordering != VARCMP_GT;
        case 4:  // >
            return ordering == VARCMP_GT;
        default:  // >=
            return ordering != VARCMP_LT;
    }
}

// The 16-bit/32-bit answer of a comparison used as a condition.
LONG test(VARIANT* const right, VARIANT* const left, const int relation,
          const ULONG flags) {
    const HRESULT ordering = order(left, right, flags);
    if (FAILED(ordering)) {
        static_cast<void>(vbrun::raise_hresult(ordering));
        return 0;
    }
    return holds(ordering, relation) ? -1 : 0;
}

// The answer of a comparison as a Variant: a Boolean, or Null when an
// operand is Null.
VARIANT* compare(VARIANT* const result, VARIANT* const right,
                 VARIANT* const left, const int relation, const ULONG flags) {
    const HRESULT ordering = order(left, right, flags);
    if (FAILED(ordering)) {
        static_cast<void>(vbrun::raise_hresult(ordering));
        return result;
    }
    if (ordering == VARCMP_NULL) {
        V_VT(result) = VT_NULL;
    } else {
        V_VT(result) = VT_BOOL;
        V_BOOL(result) =
            holds(ordering, relation) ? VARIANT_TRUE : VARIANT_FALSE;
    }
    return result;
}

}  // namespace

#define VBRUN_BINARY(name, call)                                     \
    extern "C" VARIANT* WINAPI name(VARIANT* result, VARIANT* right, \
                                    VARIANT* left) {                 \
        return finish(call(left, right, result), result);            \
    }

#define VBRUN_UNARY(name, call)                                          \
    extern "C" VARIANT* WINAPI name(VARIANT* result, VARIANT* operand) { \
        return finish(call(operand, result), result);                    \
    }

VBRUN_BINARY(__vbaVarAdd, VarAdd)
VBRUN_BINARY(__vbaVarSub, VarSub)
VBRUN_BINARY(__vbaVarMul, VarMul)
VBRUN_BINARY(__vbaVarDiv, VarDiv)
VBRUN_BINARY(__vbaVarIdiv, VarIdiv)
VBRUN_BINARY(__vbaVarMod, VarMod)
VBRUN_BINARY(__vbaVarPow, VarPow)
VBRUN_BINARY(__vbaVarAnd, VarAnd)
VBRUN_BINARY(__vbaVarOr, VarOr)
VBRUN_BINARY(__vbaVarXor, VarXor)
VBRUN_BINARY(__vbaVarEqv, VarEqv)
VBRUN_BINARY(__vbaVarImp, VarImp)
VBRUN_UNARY(__vbaVarNot, VarNot)
VBRUN_UNARY(__vbaVarNeg, VarNeg)
VBRUN_UNARY(__vbaVarAbs, VarAbs)
VBRUN_UNARY(__vbaVarFix, VarFix)
VBRUN_UNARY(__vbaVarInt, VarInt)

#define VBRUN_TEST(name, relation, flags)                        \
    extern "C" LONG WINAPI name(VARIANT* right, VARIANT* left) { \
        return test(right, left, relation, flags);               \
    }

#define VBRUN_COMPARE(name, relation, flags)                         \
    extern "C" VARIANT* WINAPI name(VARIANT* result, VARIANT* right, \
                                    VARIANT* left) {                 \
        return compare(result, right, left, relation, flags);        \
    }

VBRUN_TEST(__vbaVarTstEq, 0, 0)
VBRUN_TEST(__vbaVarTstNe, 1, 0)
VBRUN_TEST(__vbaVarTstLt, 2, 0)
VBRUN_TEST(__vbaVarTstLe, 3, 0)
VBRUN_TEST(__vbaVarTstGt, 4, 0)
VBRUN_TEST(__vbaVarTstGe, 5, 0)
VBRUN_TEST(__vbaVarTextTstEq, 0, NORM_IGNORECASE)
VBRUN_TEST(__vbaVarTextTstNe, 1, NORM_IGNORECASE)
VBRUN_TEST(__vbaVarTextTstLt, 2, NORM_IGNORECASE)
VBRUN_TEST(__vbaVarTextTstLe, 3, NORM_IGNORECASE)
VBRUN_TEST(__vbaVarTextTstGt, 4, NORM_IGNORECASE)
VBRUN_TEST(__vbaVarTextTstGe, 5, NORM_IGNORECASE)
VBRUN_COMPARE(__vbaVarCmpEq, 0, 0)
VBRUN_COMPARE(__vbaVarCmpNe, 1, 0)
VBRUN_COMPARE(__vbaVarCmpLt, 2, 0)
VBRUN_COMPARE(__vbaVarCmpLe, 3, 0)
VBRUN_COMPARE(__vbaVarCmpGt, 4, 0)
VBRUN_COMPARE(__vbaVarCmpGe, 5, 0)
VBRUN_COMPARE(__vbaVarTextCmpEq, 0, NORM_IGNORECASE)
VBRUN_COMPARE(__vbaVarTextCmpNe, 1, NORM_IGNORECASE)
VBRUN_COMPARE(__vbaVarTextCmpLt, 2, NORM_IGNORECASE)
VBRUN_COMPARE(__vbaVarTextCmpLe, 3, NORM_IGNORECASE)
VBRUN_COMPARE(__vbaVarTextCmpGt, 4, NORM_IGNORECASE)
VBRUN_COMPARE(__vbaVarTextCmpGe, 5, NORM_IGNORECASE)
