// REQ-0101, REQ-0102: the functions of the language that take and give a
// Variant (Abs, Fix, Int, Sgn, Len, LenB, LCase, UCase, Trim, LTrim, RTrim).
//
// The reference calls were measured from a Visual Basic 6 program that
// declares the exports (see tests/cases/varfuncs.cases): the result Variant
// comes first, then the operand. A Null gives a Null; the other operands are
// read as a number or as text the way Automation reads them, and a failure is
// a Visual Basic error of the thread (13 for text that is no number).

#include <algorithm>
#include <cwchar>
#include <string>

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

bool is_null(const VARIANT* const value) {
    return (value->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL;
}

VARIANT* give_null(VARIANT* const result) {
    VariantClear(result);
    result->vt = VT_NULL;
    return result;
}

// The operand as text; false (after raising) when it cannot be.
bool text_of(const VARIANT* const operand, std::wstring& text) {
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status = VariantChangeTypeEx(
        &converted, const_cast<VARIANT*>(operand), language_locale(),
        VARIANT_ALPHABOOL,
        VT_BSTR);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    text.assign(converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                SysStringLen(converted.bstrVal));
    VariantClear(&converted);
    return true;
}

VARIANT* give_text(VARIANT* const result, const std::wstring& text) {
    BSTR made = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal = made;
    return result;
}

VARIANT* give_long(VARIANT* const result, const LONG number) {
    VariantClear(result);
    result->vt = VT_I4;
    result->lVal = number;
    return result;
}

VARIANT* mapped(VARIANT* const result, const VARIANT* const operand,
                const DWORD flag) {
    if (is_null(operand)) {
        return give_null(result);
    }
    std::wstring text;
    if (!text_of(operand, text)) {
        return result;
    }
    if (!text.empty()) {
        std::wstring made(text.size(), L'\0');
        if (LCMapStringW(LOCALE_USER_DEFAULT, flag, text.data(),
                         static_cast<int>(text.size()), made.data(),
                         static_cast<int>(made.size())) != 0) {
            text = made;
        }
    }
    return give_text(result, text);
}

VARIANT* trimmed(VARIANT* const result, const VARIANT* const operand,
                 const bool left, const bool right) {
    if (is_null(operand)) {
        return give_null(result);
    }
    std::wstring text;
    if (!text_of(operand, text)) {
        return result;
    }
    std::size_t first = 0;
    std::size_t last = text.size();
    while (left && first < last && text[first] == L' ') {
        ++first;
    }
    while (right && last > first && text[last - 1U] == L' ') {
        --last;
    }
    return give_text(result, text.substr(first, last - first));
}

VARIANT* length_of(VARIANT* const result, const VARIANT* const operand,
                   const bool bytes) {
    if (is_null(operand)) {
        return give_null(result);
    }
    std::wstring text;
    if (!text_of(operand, text)) {
        return result;
    }
    return give_long(result,
                     static_cast<LONG>(text.size() * (bytes ? 2U : 1U)));
}

}  // namespace

extern "C" {

VARIANT* WINAPI rtcAbsVar(VARIANT* const result, VARIANT* const operand) {
    return finish(VarAbs(operand, result), result);
}

VARIANT* WINAPI rtcFixVar(VARIANT* const result, VARIANT* const operand) {
    return finish(VarFix(operand, result), result);
}

VARIANT* WINAPI rtcIntVar(VARIANT* const result, VARIANT* const operand) {
    return finish(VarInt(operand, result), result);
}

// Sgn: an Integer, -1, 0 or 1.
VARIANT* WINAPI rtcSgnVar(VARIANT* const result, VARIANT* const operand) {
    if (is_null(operand)) {
        return give_null(result);
    }
    VARIANT number;
    VariantInit(&number);
    const HRESULT status =
        VariantChangeTypeEx(&number, operand, language_locale(), 0, VT_R8);
    if (FAILED(status)) {
        return finish(status, result);
    }
    VariantClear(result);
    result->vt = VT_I2;
    result->iVal = number.dblVal > 0.0 ? 1 : (number.dblVal < 0.0 ? -1 : 0);
    return result;
}

// LenB and Len.
VARIANT* WINAPI rtcLenVar(VARIANT* const result, VARIANT* const operand) {
    return length_of(result, operand, true);
}

VARIANT* WINAPI rtcLenCharVar(VARIANT* const result, VARIANT* const operand) {
    return length_of(result, operand, false);
}

VARIANT* WINAPI rtcLowerCaseVar(VARIANT* const result,
                                VARIANT* const operand) {
    return mapped(result, operand, LCMAP_LOWERCASE);
}

VARIANT* WINAPI rtcUpperCaseVar(VARIANT* const result,
                                VARIANT* const operand) {
    return mapped(result, operand, LCMAP_UPPERCASE);
}

VARIANT* WINAPI rtcTrimVar(VARIANT* const result, VARIANT* const operand) {
    return trimmed(result, operand, true, true);
}

VARIANT* WINAPI rtcLeftTrimVar(VARIANT* const result,
                               VARIANT* const operand) {
    return trimmed(result, operand, true, false);
}

VARIANT* WINAPI rtcRightTrimVar(VARIANT* const result,
                                VARIANT* const operand) {
    return trimmed(result, operand, false, true);
}

}  // extern "C"
