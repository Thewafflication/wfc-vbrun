// REQ-0103: the member-assignment helpers that go with GetMem and SetMemObj
// (PutMemVar, PutMemObj, SetMemVar and the Event and NewObj forms that are the
// same code), the event-sink reference counting, the library load pair and the
// record-array move. Written from the disassembly of the installed MSVBVM60
// 6.00.9848; every helper is stdcall and returns an HRESULT (zero for success)
// unless it says otherwise.

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <vector>

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
                          DISPATCH_METHOD | DISPATCH_PROPERTYGET, &none, &value,
                          &excep, nullptr);
}

// obj.<default member> = value.
HRESULT put_default(IDispatch* const object, VARIANT& value, EXCEPINFO& excep) {
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
    object =
        (V_VT(value) & kByRef) != 0 ? *V_DISPATCHREF(value) : V_DISPATCH(value);
    return true;
}

// An event sink is an interface inside a Visual Basic object. Its table is
// preceded by a header of three slots (pointer-sized: 0xc bytes on x86): the
// second points at the sink record of the class, the third at the class.
// The record (in slots too): flags (+0: 8 the sink answers IDispatch, 0x10 it
// hands QueryInterface to its object and fails a DISPID it does not handle),
// the offset of the interface in its object (slot 1), the event interface's
// IID (slot 2), the number of handlers (UInt16 at slot 4) and the handler
// table (slot 5): pairs of a DISPID and the handler's address, a slot each.
constexpr std::size_t kSlot = sizeof(void*);

template <typename T>
T slot_load(const char* const at) {
    T value{};
    std::memcpy(&value, at, sizeof value);
    return value;
}

const char* sink_record(void* const interface_pointer) {
    const char* const table = *static_cast<char**>(interface_pointer);
    return slot_load<const char*>(table - 3 * kSlot + kSlot);
}

char* sink_object(void* const interface_pointer) {
    const char* const record = sink_record(interface_pointer);
    if (record == nullptr) {
        return nullptr;
    }
    const std::uintptr_t offset = slot_load<std::uintptr_t>(record + kSlot);
    return static_cast<char*>(interface_pointer) - offset;
}

constexpr unsigned kSinkDispatch = 0x8;
constexpr unsigned kSinkDelegates = 0x10;

void* library_handle = nullptr;
thread_local void* context_worker_thread = nullptr;

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
    IUnknown* const object =
        (V_VT(from) & kByRef) != 0 ? *V_UNKNOWNREF(from) : V_UNKNOWN(from);
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

// QueryInterface of an event sink: IUnknown, IDispatch (when the class says
// so) and the event interface give the sink itself, counted; a sink whose
// class hands the question on asks its object.
HRESULT WINAPI EVENT_SINK_QueryInterface(void* const self, const IID* const iid,
                                         void** const out) {
    *out = nullptr;
    const char* const record = sink_record(self);
    if (record == nullptr) {
        return E_NOINTERFACE;
    }
    const unsigned flags = static_cast<unsigned char>(record[0]);
    if ((flags & kSinkDelegates) != 0) {
        IUnknown* const object = reinterpret_cast<IUnknown*>(sink_object(self));
        return object->QueryInterface(*iid, out);
    }
    const IID* const events = slot_load<const IID*>(record + 2 * kSlot);
    if (IsEqualIID(*iid, IID_IUnknown) ||
        ((flags & kSinkDispatch) != 0 && IsEqualIID(*iid, IID_IDispatch)) ||
        (events != nullptr && IsEqualIID(*iid, *events))) {
        *out = self;
        static_cast<IUnknown*>(self)->AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

// The sink has no names of its own: every name is unknown.
HRESULT WINAPI EVENT_SINK_GetIDsOfNames(void* /*self*/, const IID* const iid,
                                        OLECHAR** /*names*/, const UINT count,
                                        LCID /*locale*/, DISPID* const ids) {
    if (!IsEqualIID(*iid, IID_NULL)) {
        return DISP_E_UNKNOWNINTERFACE;
    }
    for (UINT i = 0; i < count; ++i) {
        ids[i] = DISPID_UNKNOWN;
    }
    return DISP_E_UNKNOWNNAME;
}

// An event fired at the sink: the handler of the DISPID is called as a method
// of the sink's object (stdcall, the object first, then the arguments in the
// order of the declaration, each as the Variant gives it: by value or by
// reference) and its HRESULT is the result. An event without a handler is
// ignored, or DISP_E_MEMBERNOTFOUND when the class says so.
HRESULT WINAPI EVENT_SINK_Invoke(void* const self, const DISPID member,
                                 const IID* const iid, LCID /*locale*/,
                                 WORD /*flags*/, DISPPARAMS* const params,
                                 VARIANT* /*result*/, EXCEPINFO* /*excep*/,
                                 UINT* /*argument_error*/) {
    if (!IsEqualIID(*iid, IID_NULL)) {
        return DISP_E_UNKNOWNINTERFACE;
    }
    const char* const record = sink_record(self);
    if (record == nullptr) {
        return DISP_E_MEMBERNOTFOUND;
    }
    const unsigned count = slot_load<std::uint16_t>(record + 4 * kSlot);
    const char* const table = slot_load<const char*>(record + 5 * kSlot);
    void* handler = nullptr;
    for (unsigned i = 0; i < count && table != nullptr; ++i) {
        const char* const entry = table + 2 * kSlot * i;
        if (slot_load<DISPID>(entry) == member) {
            handler = slot_load<void*>(entry + kSlot);
            break;
        }
    }
    if (handler == nullptr) {
        return (static_cast<unsigned char>(record[0]) & kSinkDelegates) != 0
                   ? DISP_E_MEMBERNOTFOUND
                   : S_OK;
    }
    const UINT arguments = params != nullptr ? params->cArgs : 0;
    std::vector<VARTYPE> types(arguments + 1);
    std::vector<VARIANTARG*> values(arguments + 1);
    VARIANTARG object;
    VariantInit(&object);
    V_VT(&object) = VT_UNKNOWN;
    V_UNKNOWN(&object) = reinterpret_cast<IUnknown*>(sink_object(self));
    types[0] = VT_UNKNOWN;
    values[0] = &object;
    for (UINT i = 0; i < arguments; ++i) {
        VARIANTARG* const argument = &params->rgvarg[arguments - 1 - i];
        types[i + 1] = V_VT(argument);
        values[i + 1] = argument;
    }
    VARIANT returned;
    VariantInit(&returned);
    const HRESULT called = DispCallFunc(
        nullptr, reinterpret_cast<ULONG_PTR>(handler), CC_STDCALL, VT_ERROR,
        arguments + 1, types.data(), values.data(), &returned);
    if (FAILED(called)) {
        return called;
    }
    return V_VT(&returned) == VT_ERROR ? V_ERROR(&returned) : S_OK;
}

// Set object.member = value for a WithEvents member reached through memory:
// the slot takes the value (counted) and releases what it held. Connecting
// the object's events to the new value is the engine's and not reproduced.
HRESULT WINAPI SetMemEvent(void* /*object*/, const WORD /*member*/,
                           IUnknown* const value, IUnknown** const slot) {
    if (slot == nullptr) {
        return E_POINTER;
    }
    if (value != nullptr) {
        value->AddRef();
    }
    IUnknown* const old = *slot;
    *slot = value;
    if (old != nullptr) {
        old->Release();
    }
    return S_OK;
}

}  // extern "C"

namespace {

// The event source of an object: the object itself in slot 2 (counted while
// the event runs) and, from slot 13 (+0x34 on x86), the list of the sinks
// connected to it: nodes of {next, sink}; the low bit of the first node's
// sink marks an event in progress.
struct SinkNode {
    SinkNode* next;
    std::uintptr_t sink;
};

void raise_event(void* const source, const DISPID member, const LONG count,
                 VARIANTARG* const arguments) {
    if (source == nullptr) {
        return;
    }
    char* const base = static_cast<char*>(source);
    SinkNode* const head = reinterpret_cast<SinkNode*>(base + 13 * kSlot);
    if ((head->sink & ~std::uintptr_t{1}) == 0 && head->next == nullptr) {
        return;
    }
    IUnknown* const owner = slot_load<IUnknown*>(base + 2 * kSlot);
    if (owner != nullptr) {
        owner->AddRef();
    }
    const std::uintptr_t busy = head->sink & 1;
    head->sink |= 1;
    DISPPARAMS params = {count > 0 ? arguments : nullptr, nullptr,
                         static_cast<UINT>(count > 0 ? count : 0), 0};
    for (SinkNode* node = head; node != nullptr; node = node->next) {
        auto* const sink =
            reinterpret_cast<IDispatch*>(node->sink & ~std::uintptr_t{1});
        if (sink == nullptr) {
            continue;
        }
        EXCEPINFO excep = {};
        UINT argument_error = 0;
        static_cast<void>(sink->Invoke(member, IID_NULL, 0x409, DISPATCH_METHOD,
                                       &params, nullptr, &excep,
                                       &argument_error));
        SysFreeString(excep.bstrSource);
        SysFreeString(excep.bstrDescription);
        SysFreeString(excep.bstrHelpFile);
    }
    head->sink = (head->sink & ~std::uintptr_t{1}) | busy;
    if (owner != nullptr) {
        owner->Release();
    }
}

}  // namespace

extern "C" {

// RaiseEvent: every sink connected to the source is called (Invoke of the
// event's DISPID, DISPATCH_METHOD, locale 0x409) with the arguments; what a
// sink answers is not an error of the raiser. On x86 the arguments are the
// Variants that follow on the stack (the last argument first, as rgvarg);
// elsewhere the fourth argument points at that array.
#if defined(_M_IX86)
void __cdecl __vbaRaiseEvent(void* const source, const DISPID member,
                             const LONG count, ...) {
    raise_event(source, member, count,
                reinterpret_cast<VARIANTARG*>(const_cast<LONG*>(&count) + 1));
}
#else
void __cdecl __vbaRaiseEvent(void* const source, const DISPID member,
                             const LONG count, ...) {
    va_list list;
    va_start(list, count);
    VARIANTARG* const arguments = va_arg(list, VARIANTARG*);
    va_end(list);
    raise_event(source, member, count, arguments);
}
#endif

// The handle of the loaded library is only remembered.
void WINAPI EbLibraryLoad(void* const handle) {
    library_handle = handle;
}

void WINAPI EbLibraryUnload() {}

// The worker thread of the calling thread's context is only remembered.
void WINAPI EbSetContextWorkerThread(void* const thread) {
    context_worker_thread = thread;
}

// The interface identifier the reference exports as data:
// {1D6F7D1E-EACD-11CD-9570-00AA0051E5D4}.
extern const GUID IID_IVbaHost;
const GUID IID_IVbaHost = {0x1D6F7D1E,
                           0xEACD,
                           0x11CD,
                           {0x95, 0x70, 0x00, 0xAA, 0x00, 0x51, 0xE5, 0xD4}};

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
