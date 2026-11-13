// For Each: the enumeration of an array (a locked SAFEARRAY and a running
// index) or of a collection (an IEnumVARIANT from the _NewEnum member of the
// object).
//
// Written from the disassembly of MSVBVM60. The state of a loop is a pair of
// slots the compiled program keeps: the array (or enumerator) being walked,
// and the index. The Var forms take either kind and a "kind" slot that tells
// which one a loop is (zero for a collection, the element type for an array).

#include "vbrun_helpers.hpp"

namespace {

constexpr int kForNotInitialized = 92;
constexpr int kObjectRequired = 424;
constexpr int kTypeMismatch = 13;
constexpr int kUnsupportedType = 458;
constexpr int kBadEnumerator = 451;
constexpr int kNoMember = 438;

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

void release(IUnknown** const slot) {
    if (*slot != nullptr) {
        (*slot)->Release();
        *slot = nullptr;
    }
}

ULONG element_count(const SAFEARRAY* const array) {
    ULONG count = 1;
    for (USHORT dimension = 0; dimension < array->cDims; ++dimension) {
        count *= array->rgsabound[dimension].cElements;
    }
    return count;
}

// The next element of the array in `out`; false when the array is done.
LONG next_in_array(const unsigned short type, SAFEARRAY** const state,
                   ULONG* const index, VARIANT* const out) {
    if (*state == nullptr) {
        static_cast<void>(vbrun::raise(kForNotInitialized));
        return 0;
    }
    if ((out->vt & VT_BYREF) != 0) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return 0;
    }
    VariantClear(out);
    SAFEARRAY* const array = *state;
    if (*index >= element_count(array)) {
        SafeArrayUnlock(array);
        *state = nullptr;
        *index = 0;
        return 0;
    }
    const auto at = *index;
    const char* const data = static_cast<const char*>(array->pvData);
    switch (type) {
        case VT_I2:
        case VT_BOOL:
            out->vt = type;
            out->iVal = reinterpret_cast<const SHORT*>(data)[at];
            break;
        case VT_I4:
        case VT_R4:
            out->vt = type;
            out->lVal = reinterpret_cast<const LONG*>(data)[at];
            break;
        case VT_R8:
        case VT_CY:
        case VT_DATE:
            out->vt = type;
            out->llVal = reinterpret_cast<const LONGLONG*>(data)[at];
            break;
        case VT_UI1:
            out->vt = type;
            out->bVal = reinterpret_cast<const BYTE*>(data)[at];
            break;
        case VT_BSTR: {
            const BSTR text = reinterpret_cast<const BSTR*>(data)[at];
            out->vt = VT_BSTR;
            out->bstrVal = text == nullptr
                               ? SysAllocStringByteLen(nullptr, 0)
                               : SysAllocStringByteLen(
                                     reinterpret_cast<const char*>(text),
                                     SysStringByteLen(text));
            break;
        }
        case VT_DISPATCH: {
            IDispatch* const object =
                reinterpret_cast<IDispatch* const*>(data)[at];
            out->vt = VT_DISPATCH;
            out->pdispVal = object;
            if (object != nullptr) {
                object->AddRef();
            }
            break;
        }
        case VT_VARIANT:
            VariantCopy(out, &reinterpret_cast<const VARIANT*>(data)[at]);
            break;
        case VT_DECIMAL: {
            const DECIMAL value = reinterpret_cast<const DECIMAL*>(data)[at];
            out->decVal = value;
            out->vt = VT_DECIMAL;
            break;
        }
        case VT_ERROR:
            static_cast<void>(vbrun::raise(kTypeMismatch));
            return 0;
        default:
            static_cast<void>(vbrun::raise(kUnsupportedType));
            return 0;
    }
    ++*index;
    return -1;
}

// Starts the collection loop: the enumerator of the _NewEnum member.
void new_enumerator(IDispatch* const object, IUnknown** const slot) {
    DISPPARAMS none = {};
    VARIANT result;
    VariantInit(&result);
    EXCEPINFO excep = {};
    UINT bad_argument = 0;
    const HRESULT status = object->Invoke(
        DISPID_NEWENUM, IID_NULL, language_locale(),
        DISPATCH_METHOD | DISPATCH_PROPERTYGET, &none, &result, &excep,
        &bad_argument);
    if (FAILED(status)) {
        if (status == DISP_E_MEMBERNOTFOUND || status == DISP_E_UNKNOWNNAME) {
            static_cast<void>(vbrun::raise(kNoMember));
        } else {
            static_cast<void>(vbrun::raise_hresult(status));
        }
        SysFreeString(excep.bstrSource);
        SysFreeString(excep.bstrDescription);
        SysFreeString(excep.bstrHelpFile);
        return;
    }
    if ((result.vt != VT_DISPATCH && result.vt != VT_UNKNOWN) ||
        result.punkVal == nullptr) {
        static_cast<void>(vbrun::raise(kBadEnumerator));
        VariantClear(&result);
        return;
    }
    const HRESULT found = result.punkVal->QueryInterface(
        IID_IEnumVARIANT, reinterpret_cast<void**>(slot));
    result.punkVal->Release();
    if (FAILED(found)) {
        static_cast<void>(vbrun::raise(kBadEnumerator));
    }
}

// The next element of the collection in `out`.
LONG next_in_collection(IUnknown** const slot, VARIANT* const out) {
    if ((out->vt & VT_BYREF) != 0) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return 0;
    }
    VariantClear(out);
    if (*slot == nullptr) {
        static_cast<void>(vbrun::raise(kForNotInitialized));
        return 0;
    }
    auto* const enumerator = static_cast<IEnumVARIANT*>(*slot);
    const HRESULT status = enumerator->Next(1, out, nullptr);
    if (status == S_OK) {
        return -1;
    }
    *slot = nullptr;
    enumerator->Release();
    if (status != S_FALSE) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
    return 0;
}

// The object of the element in `element` as the interface of `iid`.
void element_interface(IUnknown* const element, const IID* const iid,
                       IUnknown** const target) {
    if (element == nullptr) {
        *target = nullptr;
        return;
    }
    HRESULT status = element->QueryInterface(
        iid == nullptr ? IID_IDispatch : *iid,
        reinterpret_cast<void**>(target));
    if (SUCCEEDED(status) && *target == nullptr) {
        status = E_NOINTERFACE;
    }
    if (status == E_NOINTERFACE) {
        status = static_cast<HRESULT>(0x800A000D);  // type mismatch
    }
    element->Release();
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
    }
}

}  // namespace

extern "C" {

// For Each over an array: locks it and gives the first element.
LONG WINAPI __vbaForEachAry(const unsigned short type, SAFEARRAY** const state,
                            ULONG* const index, VARIANT* const out,
                            SAFEARRAY* const array) {
    if (array == nullptr || array->cDims == 0) {
        static_cast<void>(vbrun::raise(kForNotInitialized));
        return 0;
    }
    if (*state != nullptr) {
        SafeArrayUnlock(*state);
    }
    SafeArrayLock(array);
    *state = array;
    *index = 0;
    return next_in_array(type, state, index, out);
}

LONG WINAPI __vbaNextEachAry(const unsigned short type,
                             SAFEARRAY** const state, ULONG* const index,
                             VARIANT* const out) {
    return next_in_array(type, state, index, out);
}

void WINAPI __vbaExitEachAry(SAFEARRAY** const state) {
    if (*state == nullptr) {
        static_cast<void>(vbrun::raise(kForNotInitialized));
        return;
    }
    SafeArrayUnlock(*state);
    *state = nullptr;
}

void WINAPI __vbaExitEachColl(IUnknown** const slot) {
    if (*slot == nullptr) {
        static_cast<void>(vbrun::raise(kForNotInitialized));
        return;
    }
    release(slot);
}

// For Each over a collection: the element is a Variant.
LONG WINAPI __vbaForEachCollVar(IUnknown** const slot, VARIANT* const out,
                                IDispatch* const object) {
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kObjectRequired));
        return 0;
    }
    release(slot);
    new_enumerator(object, slot);
    if (*slot == nullptr) {
        return 0;
    }
    return next_in_collection(slot, out);
}

LONG WINAPI __vbaNextEachCollVar(IUnknown** const slot, VARIANT* const out) {
    return next_in_collection(slot, out);
}

// The element is an object (IDispatch).
LONG WINAPI __vbaForEachCollAd(IUnknown** const slot,
                               IDispatch** const element,
                               IDispatch* const object) {
    VARIANT value;
    VariantInit(&value);
    const LONG found = __vbaForEachCollVar(slot, &value, object);
    release(reinterpret_cast<IUnknown**>(element));
    if (found != 0) {
        if (value.vt != VT_DISPATCH) {
            static_cast<void>(vbrun::raise(kObjectRequired));
            VariantClear(&value);
            return found;
        }
        *element = value.pdispVal;
    }
    return found;
}

LONG WINAPI __vbaNextEachCollAd(IUnknown** const slot,
                                IDispatch** const element) {
    VARIANT value;
    VariantInit(&value);
    const LONG found = __vbaNextEachCollVar(slot, &value);
    release(reinterpret_cast<IUnknown**>(element));
    if (found != 0) {
        if (value.vt != VT_DISPATCH) {
            static_cast<void>(vbrun::raise(kObjectRequired));
            VariantClear(&value);
            return found;
        }
        *element = value.pdispVal;
    }
    return found;
}

// The element as an interface of its own (the IID of the variable).
LONG WINAPI __vbaForEachCollObj(const IID* const iid, IUnknown** const slot,
                                IUnknown** const target,
                                IDispatch* const object) {
    IDispatch* element = nullptr;
    const LONG found = __vbaForEachCollAd(slot, &element, object);
    release(target);
    element_interface(element, iid, target);
    return found;
}

LONG WINAPI __vbaNextEachCollObj(const IID* const iid, IUnknown** const slot,
                                 IUnknown** const target) {
    IDispatch* element = nullptr;
    const LONG found = __vbaNextEachCollAd(slot, &element);
    release(target);
    element_interface(element, iid, target);
    return found;
}

// For Each over a Variant: an object, or an array.
LONG WINAPI __vbaForEachVar(SAFEARRAY** const state, ULONG* const index,
                            LONG* const kind, IUnknown** const enumerator,
                            VARIANT* const out, const VARIANT* const source) {
    if (source->vt == VT_DISPATCH) {
        *kind = 0;
        return __vbaForEachCollVar(enumerator, out, source->pdispVal);
    }
    if ((source->vt & VT_ARRAY) != 0) {
        const auto type = static_cast<unsigned short>(source->vt & ~VT_ARRAY);
        *kind = type;
        return __vbaForEachAry(type, state, index, out, source->parray);
    }
    static_cast<void>(vbrun::raise(kTypeMismatch));
    return 0;
}

LONG WINAPI __vbaNextEachVar(SAFEARRAY** const state, ULONG* const index,
                             const LONG* const kind,
                             IUnknown** const enumerator,
                             VARIANT* const out) {
    const auto type = static_cast<unsigned short>(*kind);
    if (type == 0) {
        return __vbaNextEachCollVar(enumerator, out);
    }
    return __vbaNextEachAry(type, state, index, out);
}

void WINAPI __vbaExitEachVar(SAFEARRAY** const state, const LONG* const kind,
                             IUnknown** const enumerator) {
    if (*kind == 0) {
        __vbaExitEachColl(enumerator);
    } else {
        __vbaExitEachAry(state);
    }
}

}  // extern "C"
