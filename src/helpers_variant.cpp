// REQ-0102: the type tests of the language runtime on Variants (VarType,
// IsEmpty, IsNull, IsArray, IsObject, IsError).
//
// They return a 16-bit result (the reference sets only AX): -1 for True.

#include <cstdint>
#include <cwctype>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

// The type of a Variant as the language sees it: the by-reference flag is
// dropped, and an object stands for the type of its default property.
std::uint16_t language_type(const VARIANT* const value) {
    const unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    if (vt != VT_DISPATCH) {
        return static_cast<std::uint16_t>(vt);
    }
    IDispatch* const object =
        (value->vt & VT_BYREF) != 0
            ? (value->ppdispVal != nullptr ? *value->ppdispVal : nullptr)
            : value->pdispVal;
    if (object == nullptr) {
        return static_cast<std::uint16_t>(vt);
    }
    DISPPARAMS none = {nullptr, nullptr, 0, 0};
    VARIANT held;
    VariantInit(&held);
    const HRESULT result = object->Invoke(
        DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD | DISPATCH_PROPERTYGET,
        &none, &held, nullptr, nullptr);
    if (FAILED(result)) {
        return static_cast<std::uint16_t>(vt);
    }
    const auto type = static_cast<std::uint16_t>(held.vt);
    VariantClear(&held);
    return type;
}

SHORT truth(const bool condition) {
    return condition ? static_cast<SHORT>(-1) : static_cast<SHORT>(0);
}

// The name of the class of an object, from its type information; "Object"
// when it has none.
std::wstring object_class_name(IDispatch* const object) {
    if (object == nullptr) {
        return L"Nothing";
    }
    ITypeInfo* info = nullptr;
    if (SUCCEEDED(object->GetTypeInfo(0, LOCALE_USER_DEFAULT, &info)) &&
        info != nullptr) {
        BSTR name = nullptr;
        const HRESULT status = info->GetDocumentation(
            MEMBERID_NIL, &name, nullptr, nullptr, nullptr);
        info->Release();
        if (SUCCEEDED(status) && name != nullptr) {
            std::wstring text(name, SysStringLen(name));
            SysFreeString(name);
            return text;
        }
    }
    return L"Object";
}

const wchar_t* scalar_type_name(const unsigned vt) {
    switch (vt) {
        case VT_EMPTY:
            return L"Empty";
        case VT_NULL:
            return L"Null";
        case VT_I2:
            return L"Integer";
        case VT_I4:
            return L"Long";
        case VT_R4:
            return L"Single";
        case VT_R8:
            return L"Double";
        case VT_CY:
            return L"Currency";
        case VT_DATE:
            return L"Date";
        case VT_BSTR:
            return L"String";
        case VT_ERROR:
            return L"Error";
        case VT_BOOL:
            return L"Boolean";
        case VT_VARIANT:
            return L"Variant";
        case VT_UNKNOWN:
            return L"Unknown";
        case VT_DECIMAL:
            return L"Decimal";
        case VT_UI1:
            return L"Byte";
        default:
            return nullptr;
    }
}

// Whether a string reads as a number: a number of the user's locale, or a
// hexadecimal or octal literal (`&H1F`, `&O17`).
bool string_is_numeric(const wchar_t* const text) {
    if (text == nullptr) {
        return false;
    }
    const wchar_t* at = text;
    while (*at == L' ') {
        ++at;
    }
    if (at[0] == L'&' &&
        (at[1] == L'H' || at[1] == L'h' || at[1] == L'O' || at[1] == L'o')) {
        const bool hex = at[1] == L'H' || at[1] == L'h';
        const wchar_t* digits = at + 2;
        if (*digits == 0) {
            return false;
        }
        for (; *digits != 0 && *digits != L'&' && *digits != L' '; ++digits) {
            const bool ok = hex ? iswxdigit(*digits) != 0
                                : (*digits >= L'0' && *digits <= L'7');
            if (!ok) {
                return false;
            }
        }
        while (*digits == L' ') {
            ++digits;
        }
        return *digits == 0 || (*digits == L'&' && digits[1] == 0);
    }
    double value = 0.0;
    return SUCCEEDED(VarR8FromStr(text, GetUserDefaultLCID(), 0, &value));
}

}  // namespace

extern "C" {

LONG WINAPI rtcVarType(const VARIANT* value) {
    return language_type(value);
}

SHORT WINAPI rtcIsEmpty(const VARIANT* value) {
    return truth(language_type(value) == VT_EMPTY);
}

SHORT WINAPI rtcIsNull(const VARIANT* value) {
    return truth(language_type(value) == VT_NULL);
}

SHORT WINAPI rtcIsArray(const VARIANT* value) {
    return truth((language_type(value) & VT_ARRAY) != 0);
}

SHORT WINAPI rtcIsError(const VARIANT* value) {
    return truth(language_type(value) == VT_ERROR);
}

// IsObject looks at the type itself (no default property is read).
SHORT WINAPI rtcIsObject(const VARIANT* value) {
    const unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    return truth(vt == VT_DISPATCH);
}

// TypeName: the name of the type of a Variant ("()" after an array's).
BSTR WINAPI rtcTypeName(const VARIANT* value) {
    unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    const bool array = (vt & VT_ARRAY) != 0;
    std::wstring name;
    if (array) {
        const wchar_t* element = scalar_type_name(vt & ~VT_ARRAY);
        name = element != nullptr ? element : L"Variant";
        name += L"()";
    } else if (vt == VT_DISPATCH) {
        IDispatch* const object =
            (value->vt & VT_BYREF) != 0
                ? (value->ppdispVal != nullptr ? *value->ppdispVal : nullptr)
                : value->pdispVal;
        name = object_class_name(object);
    } else if (const wchar_t* known = scalar_type_name(vt)) {
        name = known;
    } else {
        name = L"Unknown";
    }
    return SysAllocStringLen(name.c_str(), static_cast<UINT>(name.size()));
}

// IsNumeric: a 16-bit -1 for a number, a Boolean, an Empty or a string that
// reads as a number.
SHORT WINAPI rtcIsNumeric(const VARIANT* value) {
    const unsigned vt = value->vt & ~static_cast<unsigned>(VT_BYREF);
    switch (vt) {
        case VT_EMPTY:
        case VT_I2:
        case VT_I4:
        case VT_R4:
        case VT_R8:
        case VT_CY:
        case VT_BOOL:
        case VT_DECIMAL:
        case VT_UI1:
            return truth(true);
        case VT_BSTR: {
            const BSTR text =
                (value->vt & VT_BYREF) != 0
                    ? (value->pbstrVal != nullptr ? *value->pbstrVal : nullptr)
                    : value->bstrVal;
            return truth(string_is_numeric(text));
        }
        default:
            return truth(false);
    }
}

}  // extern "C"
