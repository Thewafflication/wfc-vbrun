// REQ-0103: the member-assignment helpers that go with GetMem and SetMemObj
// (PutMemVar, PutMemObj, SetMemVar and the Event and NewObj forms that are the
// same code), the event-sink reference counting, the library load pair and the
// record-array move. Written from the disassembly of the installed MSVBVM60
// 6.00.9848; every helper is stdcall and returns an HRESULT (zero for success)
// unless it says otherwise.

#include <cstdint>
#include <cstring>

#include "vbrun_helpers.hpp"

namespace {

constexpr VARTYPE kByRef = 0x4000;
constexpr int kObjectVariableNotSet = 91;
constexpr int kTypeMismatch = 13;

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

// The error of a failed Invoke: an exception of the object carries its own
// code, and its strings are freed.
HRESULT invoke_failed(HRESULT result, EXCEPINFO& excep) {
    if (result == DISP_E_EXCEPTION) {
        result = excep.scode;
    }
    static_cast<void>(vbrun::raise_hresult(result));
    SysFreeString(excep.bstrSource);
    SysFreeString(excep.bstrDescription);
    SysFreeString(excep.bstrHelpFile);
    return result;
}

// obj.<default member> read (flags 3: method or property get).
HRESULT default_value(IDispatch* const object, VARIANT& value,
                      EXCEPINFO& excep) {
    DISPPARAMS none = {nullptr, nullptr, 0, 0};
    return object->Invoke(DISPID_VALUE, IID_NULL, language_locale(),
                          DISPATCH_METHOD | DISPATCH_PROPERTYGET, &none,
                          &value, &excep, nullptr);
}

// obj.<default member> = value.
HRESULT put_default(IDispatch* const object, VARIANT& value,
                    EXCEPINFO& excep) {
    DISPID named = DISPID_PROPERTYPUT;
    DISPPARAMS params = {&value, &named, 1, 1};
    return object->Invoke(DISPID_VALUE, IID_NULL, language_locale(),
                          DISPATCH_PROPERTYPUT, &params, nullptr, &excep,
                          nullptr);
}

// The object of an object Variant (or of a reference to one), or null.
bool object_variant(const VARIANT* const value, IDispatch*& object) {
    if ((V_VT(value) & ~kByRef) != VT_DISPATCH) {
        return false;
    }
    object = (V_VT(value) & kByRef) != 0 ? *V_DISPATCHREF(value)
                                         : V_DISPATCH(value);
    return true;
}

// The sink object of an event-sink interface pointer: the vtable of the
// interface is preceded by a header whose second word points at a record
// holding the offset of the interface inside the object.
char* sink_object(void* const interface_pointer) {
    char* const table = *static_cast<char**>(interface_pointer);
    char* const record = *reinterpret_cast<char**>(table - 0xc + 4);
    if (record == nullptr) {
        return nullptr;
    }
    const std::uintptr_t offset =
        *reinterpret_cast<const std::uintptr_t*>(record + 4);
    return static_cast<char*>(interface_pointer) - offset;
}

void* library_handle = nullptr;

}  // namespace

extern "C" {

// A Variant stored into another, an object giving its default value.
HRESULT WINAPI PutMemVar(VARIANT* const to, VARIANT* const from) {
    IDispatch* object = nullptr;
    if (object_variant(from, object)) {
        if (object == nullptr) {
            return vbrun::raise(kObjectVariableNotSet);
        }
        VARIANT value;
        VariantInit(&value);
        EXCEPINFO excep = {};
        const HRESULT result = default_value(object, value, excep);
        if (FAILED(result)) {
            return invoke_failed(result, excep);
        }
        const HRESULT copied = VariantCopy(to, &value);
        VariantClear(&value);
        return FAILED(copied) ? vbrun::raise_hresult(copied) : S_OK;
    }
    const HRESULT copied = VariantCopy(to, from);
    return FAILED(copied) ? vbrun::raise_hresult(copied) : S_OK;
}

// The default member of the object in the slot assigned a value; an object
// value gives its own default value.
HRESULT WINAPI PutMemObj(IDispatch** const target, VARIANT* const value) {
    if (*target == nullptr) {
        return vbrun::raise(kObjectVariableNotSet);
    }
    IDispatch* source = nullptr;
    HRESULT result = S_OK;
    EXCEPINFO excep = {};
    if (object_variant(value, source)) {
        if (source == nullptr) {
            return vbrun::raise(kObjectVariableNotSet);
        }
        VARIANT inner;
        VariantInit(&inner);
        result = default_value(source, inner, excep);
        if (SUCCEEDED(result)) {
            result = put_default(*target, inner, excep);
            VariantClear(&inner);
        }
    } else {
        result = put_default(*target, *value, excep);
    }
    if (FAILED(result)) {
        return invoke_failed(result, excep);
    }
    return result;
}

// An object stored into a Variant (Set v = obj): the Variant is cleared, then
// holds the object, counted.
HRESULT WINAPI SetMemVar(VARIANT* const to, VARIANT* const from) {
    const VARTYPE type = V_VT(from) & static_cast<VARTYPE>(~kByRef);
    if (type != VT_DISPATCH && type != VT_UNKNOWN) {
        return vbrun::raise(kTypeMismatch);
    }
    IUnknown* old = nullptr;
    if (V_VT(to) == VT_DISPATCH || V_VT(to) == VT_UNKNOWN) {
        old = V_UNKNOWN(to);
    } else {
        const HRESULT result = VariantClear(to);
        if (FAILED(result)) {
            return vbrun::raise_hresult(result);
        }
    }
    IUnknown* const object = (V_VT(from) & kByRef) != 0
                                 ? *V_UNKNOWNREF(from)
                                 : V_UNKNOWN(from);
    V_VT(to) = type;
    V_UNKNOWN(to) = object;
    if (object != nullptr) {
        object->AddRef();
    }
    if (old != nullptr) {
        old->Release();
    }
    return S_OK;
}

// The Event and NewObj forms take two leading arguments they do not use, and
// then are GetMemObj, PutMemObj and SetMemObj.
HRESULT WINAPI GetMemEvent(void*, void*, IUnknown** const from,
                           IUnknown** const to) {
    IUnknown* const object = *from;
    *to = object;
    if (object != nullptr) {
        object->AddRef();
    }
    return S_OK;
}

HRESULT WINAPI PutMemEvent(void*, void*, IDispatch** const target,
                           VARIANT* const value) {
    return PutMemObj(target, value);
}

// The auto-instantiating forms: the reference creates the object of a
// "Dim x As New T" variable on first use through the engine's class table,
// which is not reproduced here; an empty slot is error 91. A filled slot is
// GetMemObj and PutMemObj.
HRESULT WINAPI GetMemNewObj(void*, void*, IUnknown** const from,
                            IUnknown** const to) {
    if (*from == nullptr) {
        return vbrun::raise(kObjectVariableNotSet);
    }
    return GetMemEvent(nullptr, nullptr, from, to);
}

HRESULT WINAPI PutMemNewObj(void*, void*, IDispatch** const target,
                            VARIANT* const value) {
    return PutMemObj(target, value);
}

HRESULT WINAPI SetMemNewObj(void*, void*, IUnknown** const to,
                            IUnknown* const value) {
    if (value != nullptr) {
        value->AddRef();
    }
    IUnknown* const old = *to;
    *to = value;
    if (old != nullptr) {
        old->Release();
    }
    return S_OK;
}

// The reference count of an event sink lives at offset 0x24 of its object;
// the plain sink counts there and never destroys, the second kind counts
// through the object's own AddRef and Release.
ULONG WINAPI EVENT_SINK_AddRef(void* const self) {
    char* const object = sink_object(self);
    if (object == nullptr) {
        return 0;
    }
    return static_cast<ULONG>(++*reinterpret_cast<LONG*>(object + 0x24));
}

ULONG WINAPI EVENT_SINK_Release(void* const self) {
    char* const object = sink_object(self);
    if (object == nullptr) {
        return 0;
    }
    return static_cast<ULONG>(--*reinterpret_cast<LONG*>(object + 0x24));
}

ULONG WINAPI EVENT_SINK2_AddRef(void* const self) {
    IUnknown* const object = reinterpret_cast<IUnknown*>(sink_object(self));
    return object != nullptr ? object->AddRef() : 0;
}

ULONG WINAPI EVENT_SINK2_Release(void* const self) {
    IUnknown* const object = reinterpret_cast<IUnknown*>(sink_object(self));
    return object != nullptr ? object->Release() : 0;
}

// The handle of the loaded library is only remembered.
void WINAPI EbLibraryLoad(void* const handle) { library_handle = handle; }

void WINAPI EbLibraryUnload() {}

// An array of records moved from the source slot to the destination slot; the
// array the destination held is destroyed (a fixed-size or embedded array
// cannot be replaced: type mismatch).
HRESULT WINAPI __vbaAryRecMove(void* /*record_info*/,
                               SAFEARRAY** const destination,
                               SAFEARRAY** const source) {
    if (*destination != nullptr) {
        if (((*destination)->fFeatures & 0x17) != 0) {
            return vbrun::raise(kTypeMismatch);
        }
        if (*source == *destination) {
            return S_OK;
        }
        const HRESULT result = SafeArrayDestroy(*destination);
        if (FAILED(result)) {
            return vbrun::raise_hresult(result);
        }
    }
    *destination = *source;
    *source = nullptr;
    return S_OK;
}

}  // extern "C"
