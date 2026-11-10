// REQ-0101: the scalar conversions of the compiler (Variant to number, number
// to String, String to number), the Currency arithmetic, the String
// comparisons and the Variant concatenation.
//
// A compiled program pushes the operands left first, so the helper receives
// the right operand first: __vbaStrCat(right, left), __vbaStrCmp(right,
// left). The conversions are the Automation ones (VarI4FromStr, VarCyAdd, ...)
// with the user's locale; a failure is a Visual Basic error of the thread
// (vbrun::raise_hresult) and the helper returns 0.

#include <cstdint>

#include "vbrun_helpers.hpp"

namespace {

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

// Converts a Variant to the given type; false after raising the error (a Null
// is error 94, the rest are the Automation errors).
bool change(const VARIANT* const value, const VARTYPE type, VARIANT& out) {
    VariantInit(&out);
    if (value == nullptr) {
        static_cast<void>(vbrun::raise(94));
        return false;
    }
    if ((value->vt & VT_TYPEMASK) == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return false;
    }
    const HRESULT result = VariantChangeTypeEx(
        &out, const_cast<VARIANT*>(value), language_locale(), 0, type);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return false;
    }
    return true;
}

// Raises the error of a failed call; true when the call worked.
bool worked(const HRESULT result) {
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return false;
    }
    return true;
}

// The text of a conversion that was written into `text`.
BSTR text_result(const HRESULT result, const BSTR* const text) {
    if (!worked(result)) {
        return nullptr;
    }
    return *text;
}

// The ordering of two Strings by code unit.
int ordinal_order(const BSTR left, const BSTR right) {
    const UINT left_size = left == nullptr ? 0U : SysStringLen(left);
    const UINT right_size = right == nullptr ? 0U : SysStringLen(right);
    const UINT common = left_size < right_size ? left_size : right_size;
    for (UINT at = 0; at < common; ++at) {
        if (left[at] != right[at]) {
            return left[at] < right[at] ? -1 : 1;
        }
    }
    if (left_size == right_size) {
        return 0;
    }
    return left_size < right_size ? -1 : 1;
}

LONG text_order(const BSTR left, const BSTR right) {
    const HRESULT ordering =
        VarBstrCmp(left, right, language_locale(),
                   NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH);
    return static_cast<LONG>(ordering) - 1;
}

// A text that is no date or time reads as a number (`&H1F`, `-5`).
bool date_of_text(const BSTR text, DATE& value) {
    if (SUCCEEDED(VarDateFromStr(text, language_locale(), 0, &value))) {
        return true;
    }
    double number = 0.0;
    if (!worked(VarR8FromStr(text, language_locale(), 0, &number))) {
        return false;
    }
    value = number;
    return true;
}

}  // namespace

extern "C" {

// CInt(v), CLng(v), CByte(v), CSng(v), CDbl(v), CBool(v), CDate(v), CCur(v).
SHORT WINAPI __vbaI2Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_I2, out)) {
        return 0;
    }
    return out.iVal;
}

LONG WINAPI __vbaI4Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_I4, out)) {
        return 0;
    }
    return out.lVal;
}

BYTE WINAPI __vbaUI1Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_UI1, out)) {
        return 0;
    }
    return out.bVal;
}

float WINAPI __vbaR4Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_R4, out)) {
        return 0.0F;
    }
    return out.fltVal;
}

double WINAPI __vbaR8Var(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_R8, out)) {
        return 0.0;
    }
    return out.dblVal;
}

SHORT WINAPI __vbaBoolVar(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_BOOL, out)) {
        return 0;
    }
    return out.boolVal;
}

double WINAPI __vbaDateVar(const VARIANT* const value) {
    if (value != nullptr && value->vt == VT_BSTR) {
        DATE date = 0.0;
        return date_of_text(value->bstrVal, date) ? date : 0.0;
    }
    VARIANT out;
    if (!change(value, VT_DATE, out)) {
        return 0.0;
    }
    return out.date;
}

LONGLONG WINAPI __vbaCyVar(const VARIANT* const value) {
    VARIANT out;
    if (!change(value, VT_CY, out)) {
        return 0;
    }
    return out.cyVal.int64;
}

// CStr(number).
BSTR WINAPI __vbaStrI2(const SHORT value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromI2(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrBool(const SHORT value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromBool(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrR4(const float value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromR4(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrR8(const double value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromR8(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrDate(const double value) {
    BSTR text = nullptr;
    const HRESULT status = VarBstrFromDate(value, language_locale(), 0, &text);
    return text_result(status, &text);
}

BSTR WINAPI __vbaStrCy(const LONGLONG value) {
    BSTR text = nullptr;
    CY money;
    money.int64 = value;
    const HRESULT status = VarBstrFromCy(money, language_locale(), 0, &text);
    return text_result(status, &text);
}

// CDbl(text), CDate(text), CCur(text).
double WINAPI __vbaR8Str(const BSTR text) {
    double value = 0.0;
    if (!worked(VarR8FromStr(text, language_locale(), 0, &value))) {
        return 0.0;
    }
    return value;
}

double WINAPI __vbaDateStr(const BSTR text) {
    DATE value = 0.0;
    return date_of_text(text, value) ? value : 0.0;
}

LONGLONG WINAPI __vbaCyStr(const BSTR text) {
    CY value;
    value.int64 = 0;
    if (!worked(VarCyFromStr(text, language_locale(), 0, &value))) {
        return 0;
    }
    return value.int64;
}

// Currency arithmetic and conversions.
LONGLONG WINAPI __vbaCyAdd(const LONGLONG right, const LONGLONG left) {
    CY a;
    CY b;
    CY sum;
    a.int64 = left;
    b.int64 = right;
    sum.int64 = 0;
    return worked(VarCyAdd(a, b, &sum)) ? sum.int64 : 0;
}

// The minuend comes first here (left - right), unlike the String helpers.
LONGLONG WINAPI __vbaCySub(const LONGLONG left, const LONGLONG right) {
    CY a;
    CY b;
    CY difference;
    a.int64 = left;
    b.int64 = right;
    difference.int64 = 0;
    return worked(VarCySub(a, b, &difference)) ? difference.int64 : 0;
}

LONGLONG WINAPI __vbaCyMul(const LONGLONG right, const LONGLONG left) {
    CY a;
    CY b;
    CY product;
    a.int64 = left;
    b.int64 = right;
    product.int64 = 0;
    return worked(VarCyMul(a, b, &product)) ? product.int64 : 0;
}

LONGLONG WINAPI __vbaCyAbs(const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyAbs(in, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyFix(const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyFix(in, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyInt(const LONGLONG value) {
    CY in;
    CY out;
    in.int64 = value;
    out.int64 = 0;
    return worked(VarCyInt(in, &out)) ? out.int64 : 0;
}

LONG WINAPI __vbaCySgn(const LONGLONG value) {
    if (value == 0) {
        return 0;
    }
    return value < 0 ? -1 : 1;
}

LONGLONG WINAPI __vbaCyI2(const SHORT value) {
    CY out;
    out.int64 = 0;
    return worked(VarCyFromI2(value, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyI4(const LONG value) {
    CY out;
    out.int64 = 0;
    return worked(VarCyFromI4(value, &out)) ? out.int64 : 0;
}

LONGLONG WINAPI __vbaCyUI1(const BYTE value) {
    CY out;
    out.int64 = 0;
    return worked(VarCyFromUI1(value, &out)) ? out.int64 : 0;
}

SHORT WINAPI __vbaI2Cy(const LONGLONG value) {
    CY in;
    SHORT out = 0;
    in.int64 = value;
    return worked(VarI2FromCy(in, &out)) ? out : 0;
}

LONG WINAPI __vbaI4Cy(const LONGLONG value) {
    CY in;
    LONG out = 0;
    in.int64 = value;
    return worked(VarI4FromCy(in, &out)) ? out : 0;
}

BYTE WINAPI __vbaUI1Cy(const LONGLONG value) {
    CY in;
    BYTE out = 0;
    in.int64 = value;
    return worked(VarUI1FromCy(in, &out)) ? out : 0;
}

float WINAPI __vbaR4Cy(const LONGLONG value) {
    CY in;
    float out = 0.0F;
    in.int64 = value;
    return worked(VarR4FromCy(in, &out)) ? out : 0.0F;
}

double WINAPI __vbaR8Cy(const LONGLONG value) {
    CY in;
    double out = 0.0;
    in.int64 = value;
    return worked(VarR8FromCy(in, &out)) ? out : 0.0;
}

// left & right on Strings.
BSTR WINAPI __vbaStrCat(const BSTR right, const BSTR left) {
    BSTR joined = nullptr;
    const HRESULT status = VarBstrCat(left, right, &joined);
    return text_result(status, &joined);
}

// Comparison of Strings: -1, 0 or 1 for left against right. The mode is 0 for
// a binary comparison and 1 for a text comparison; the database mode (2) is
// an error.
LONG WINAPI __vbaStrComp(const LONG mode, const BSTR right, const BSTR left) {
    if (mode == 0) {
        return ordinal_order(left, right);
    }
    if (mode == 1) {
        return text_order(left, right);
    }
    static_cast<void>(vbrun::raise(5));
    return 0;
}

LONG WINAPI __vbaStrCmp(const BSTR right, const BSTR left) {
    return ordinal_order(left, right);
}

LONG WINAPI __vbaStrTextCmp(const BSTR right, const BSTR left) {
    return text_order(left, right);
}

// left & right on Variants (result, right, left).
VARIANT* WINAPI __vbaVarCat(VARIANT* const result, VARIANT* const right,
                            VARIANT* const left) {
    VariantInit(result);
    const HRESULT status = VarCat(left, right, result);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return result;
}

}  // extern "C"
