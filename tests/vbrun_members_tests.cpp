// REQ-0005: PutMemVar, PutMemObj, SetMemVar, the Event and NewObj forms, the
// event-sink reference counts and the record-array move.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/vbrun_helpers.hpp"

extern "C" {
HRESULT WINAPI PutMemVar(VARIANT* to, VARIANT* from);
HRESULT WINAPI PutMemObj(IDispatch** target, VARIANT* value);
HRESULT WINAPI SetMemVar(VARIANT* to, VARIANT* from);
HRESULT WINAPI GetMemEvent(void*, void*, IUnknown** from, IUnknown** to);
HRESULT WINAPI PutMemEvent(void*, void*, IDispatch** target, VARIANT* value);
HRESULT WINAPI SetMemNewObj(void*, void*, IUnknown** to, IUnknown* value);
HRESULT WINAPI GetMemNewObj(void*, void*, IUnknown** from, IUnknown** to);
HRESULT WINAPI PutMemNewObj(void*, void*, IDispatch** target, VARIANT* value);
ULONG WINAPI EVENT_SINK_AddRef(void* self);
ULONG WINAPI EVENT_SINK_Release(void* self);
ULONG WINAPI EVENT_SINK2_AddRef(void* self);
ULONG WINAPI EVENT_SINK2_Release(void* self);
void WINAPI EbLibraryLoad(void* handle);
void WINAPI EbLibraryUnload();
void WINAPI EbSetContextWorkerThread(void* thread);
HRESULT WINAPI EVENT_SINK_QueryInterface(void* self, const IID* iid,
                                         void** out);
HRESULT WINAPI EVENT_SINK_GetIDsOfNames(void* self, const IID* iid,
                                        OLECHAR** names, UINT count,
                                        LCID locale, DISPID* ids);
HRESULT WINAPI EVENT_SINK_Invoke(void* self, DISPID member, const IID* iid,
                                 LCID locale, WORD flags, DISPPARAMS* params,
                                 VARIANT* result, EXCEPINFO* excep,
                                 UINT* argument_error);
HRESULT WINAPI SetMemEvent(void* object, WORD member, IUnknown* value,
                           IUnknown** slot);
void __cdecl __vbaRaiseEvent(void* source, DISPID member, LONG count, ...);
extern const GUID IID_IVbaHost;
HRESULT WINAPI __vbaAryRecMove(void* info, SAFEARRAY** destination,
                               SAFEARRAY** source);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// An object whose default member is 41 + extra and that records the value
// assigned to it.
struct Object final : IDispatch {
    LONG count = 1;
    LONG stored = 0;
    bool put_seen = false;
    HRESULT fail = S_OK;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override {
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++count; }
    ULONG STDMETHODCALLTYPE Release() override { return --count; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID,
                                            DISPID*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* params, VARIANT* result,
                                     EXCEPINFO*, UINT*) override {
        if (fail != S_OK) {
            return fail;
        }
        if (id != DISPID_VALUE) {
            return DISP_E_MEMBERNOTFOUND;
        }
        if ((flags & DISPATCH_PROPERTYPUT) != 0) {
            put_seen = true;
            stored = V_I4(&params->rgvarg[0]);
            return S_OK;
        }
        V_VT(result) = VT_I4;
        V_I4(result) = 41;
        return S_OK;
    }
};

void variants() {
    Object object;
    VARIANT holder;
    VariantInit(&holder);
    V_VT(&holder) = VT_DISPATCH;
    V_DISPATCH(&holder) = &object;

    // PutMemVar: an object gives its default value, anything else is copied.
    VARIANT target;
    VariantInit(&target);
    expect(PutMemVar(&target, &holder) == S_OK && V_VT(&target) == VT_I4 &&
               V_I4(&target) == 41,
           "PutMemVar of an object gives the default value");
    VARIANT plain;
    VariantInit(&plain);
    V_VT(&plain) = VT_R8;
    V_R8(&plain) = 2.5;
    expect(PutMemVar(&target, &plain) == S_OK && V_VT(&target) == VT_R8 &&
               V_R8(&target) == 2.5,
           "PutMemVar of a value copies it");
    VARIANT nothing;
    VariantInit(&nothing);
    V_VT(&nothing) = VT_DISPATCH;
    V_DISPATCH(&nothing) = nullptr;
    vbrun::clear_error();
    expect(PutMemVar(&target, &nothing) == static_cast<HRESULT>(0x800A005B) &&
               vbrun::last_error() == 91,
           "PutMemVar of Nothing is error 91");
    vbrun::clear_error();
    object.fail = DISP_E_MEMBERNOTFOUND;
    expect(FAILED(PutMemVar(&target, &holder)),
           "PutMemVar fails with the call");
    object.fail = S_OK;
    vbrun::clear_error();

    // PutMemObj assigns the default member.
    Object receiver;
    IDispatch* slot = &receiver;
    VARIANT number;
    VariantInit(&number);
    V_VT(&number) = VT_I4;
    V_I4(&number) = 7;
    expect(PutMemObj(&slot, &number) == S_OK && receiver.put_seen &&
               receiver.stored == 7,
           "PutMemObj assigns a value to the default member");
    receiver.put_seen = false;
    expect(PutMemObj(&slot, &holder) == S_OK && receiver.put_seen &&
               receiver.stored == 41,
           "PutMemObj assigns the default value of an object");
    IDispatch* empty = nullptr;
    vbrun::clear_error();
    expect(PutMemObj(&empty, &number) == static_cast<HRESULT>(0x800A005B) &&
               vbrun::last_error() == 91,
           "PutMemObj on Nothing is error 91");
    vbrun::clear_error();
    receiver.put_seen = false;
    expect(PutMemEvent(nullptr, nullptr, &slot, &number) == S_OK &&
               receiver.put_seen,
           "PutMemEvent is PutMemObj");

    // SetMemVar holds an object, counted; the old one is released.
    Object first;
    Object second;
    VARIANT set;
    VariantInit(&set);
    V_VT(&set) = VT_DISPATCH;
    V_DISPATCH(&set) = &first;
    first.count = 1;
    VARIANT source;
    VariantInit(&source);
    V_VT(&source) = VT_DISPATCH;
    V_DISPATCH(&source) = &second;
    expect(SetMemVar(&set, &source) == S_OK && V_DISPATCH(&set) == &second &&
               second.count == 2 && first.count == 0,
           "SetMemVar counts the new object and releases the old one");
    VARIANT wrong;
    VariantInit(&wrong);
    V_VT(&wrong) = VT_I4;
    vbrun::clear_error();
    expect(SetMemVar(&set, &wrong) == static_cast<HRESULT>(0x800A000D) &&
               vbrun::last_error() == 13,
           "SetMemVar of a number is a type mismatch");
    vbrun::clear_error();
}

void events_and_objects() {
    Object object;
    IUnknown* from = &object;
    IUnknown* to = nullptr;
    expect(GetMemEvent(nullptr, nullptr, &from, &to) == S_OK && to == &object &&
               object.count == 2,
           "GetMemEvent copies and counts");
    IUnknown* slot = nullptr;
    expect(SetMemNewObj(nullptr, nullptr, &slot, &object) == S_OK &&
               slot == &object && object.count == 3,
           "SetMemNewObj stores and counts");
    expect(SetMemNewObj(nullptr, nullptr, &slot, nullptr) == S_OK &&
               slot == nullptr && object.count == 2,
           "SetMemNewObj releases the old object");
    const unsigned char host[16] = {0x1e, 0x7d, 0x6f, 0x1d, 0xcd, 0xea,
                                    0xcd, 0x11, 0x95, 0x70, 0x00, 0xaa,
                                    0x00, 0x51, 0xe5, 0xd4};
    expect(std::memcmp(&IID_IVbaHost, host, sizeof host) == 0,
           "IID_IVbaHost has the reference's bytes");
    EbSetContextWorkerThread(nullptr);
    IUnknown* empty = nullptr;
    IUnknown* got = nullptr;
    vbrun::clear_error();
    expect(GetMemNewObj(nullptr, nullptr, &empty, &got) ==
                   static_cast<HRESULT>(0x800A005B) &&
               vbrun::last_error() == 91,
           "GetMemNewObj of an empty slot is error 91");
    vbrun::clear_error();
    IUnknown* filled = &object;
    expect(GetMemNewObj(nullptr, nullptr, &filled, &got) == S_OK &&
               got == &object && object.count == 3,
           "GetMemNewObj of a filled slot copies and counts");
}

// A fake event sink: an interface pointer inside an object. Its table is
// preceded by a header of three pointer-sized slots whose second points at
// the sink record: flags, the offset of the interface in the object, the
// event IID, the handler count (slot 4) and the handler table (slot 5).
struct Sink {
    char pad[0x24];
    LONG refs;
    char more[0x8];
    void* table;  // the interface pointer is the address of this member
    LONG pinged;
    wchar_t seen[4];
};

constexpr std::size_t kSlot = sizeof(void*);

const IID kEvents = {0x9c1e0001,
                     0x1111,
                     0x2222,
                     {0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};

// The handler of event 1: Ping(ByVal n As Long, s As String), as the probe
// has it ("ping 4 x", then s becomes "xy").
HRESULT WINAPI on_ping(Sink* const object, const LONG n, BSTR* const s) {
    object->pinged = n;
    std::memcpy(object->seen, *s, (SysStringLen(*s) + 1) * sizeof(wchar_t));
    BSTR changed = SysAllocString(L"xy");
    SysFreeString(*s);
    *s = changed;
    return S_OK;
}

struct SinkSetup {
    unsigned char record[6 * sizeof(void*)] = {};
    unsigned char handlers[2 * sizeof(void*)] = {};
};

void make_sink(SinkSetup& setup, Sink& sink, const unsigned flags) {
    setup.record[0] = static_cast<unsigned char>(flags);
    const std::uintptr_t offset = offsetof(Sink, table);
    std::memcpy(setup.record + kSlot, &offset, sizeof offset);
    const IID* const events = &kEvents;
    std::memcpy(setup.record + 2 * kSlot, &events, sizeof events);
    const std::uint16_t count = 1;
    std::memcpy(setup.record + 4 * kSlot, &count, sizeof count);
    const unsigned char* const table = setup.handlers;
    std::memcpy(setup.record + 5 * kSlot, &table, sizeof table);
    const DISPID ping = 1;
    std::memcpy(setup.handlers, &ping, sizeof ping);
    void* const handler = reinterpret_cast<void*>(&on_ping);
    std::memcpy(setup.handlers + kSlot, &handler, sizeof handler);
    static_cast<void>(sink);
}

void sinks() {
    // The table lives right after the header, so the header and the methods
    // are one block.
    struct Block {
        unsigned char header[3 * sizeof(void*)];
        void* methods[7];
    } block = {};
    SinkSetup setup;
    Sink sink = {};
    sink.refs = 1;
    make_sink(setup, sink, 0);
    const unsigned char* const record = setup.record;
    std::memcpy(block.header + kSlot, &record, sizeof record);
    block.methods[0] = reinterpret_cast<void*>(&EVENT_SINK_QueryInterface);
    block.methods[1] = reinterpret_cast<void*>(&EVENT_SINK_AddRef);
    block.methods[2] = reinterpret_cast<void*>(&EVENT_SINK_Release);
    sink.table = block.methods;
    void* const iface = &sink.table;
    expect(EVENT_SINK_AddRef(iface) == 2 && sink.refs == 2,
           "EVENT_SINK_AddRef counts");
    expect(EVENT_SINK_Release(iface) == 1 && sink.refs == 1,
           "EVENT_SINK_Release counts");

    void* out = nullptr;
    expect(EVENT_SINK_QueryInterface(iface, &IID_IUnknown, &out) == S_OK &&
               out == iface && sink.refs == 2,
           "a sink is its own IUnknown, counted");
    expect(EVENT_SINK_QueryInterface(iface, &kEvents, &out) == S_OK &&
               out == iface && sink.refs == 3,
           "a sink answers its event interface");
    expect(EVENT_SINK_QueryInterface(iface, &IID_IDispatch, &out) ==
                   E_NOINTERFACE &&
               out == nullptr,
           "IDispatch only when the class says so");
    setup.record[0] = 8;
    expect(EVENT_SINK_QueryInterface(iface, &IID_IDispatch, &out) == S_OK,
           "flag 8: the sink is an IDispatch");
    sink.refs = 1;

    DISPID ids[1] = {0};
    OLECHAR name[] = L"Ping";
    OLECHAR* names[] = {name};
    expect(EVENT_SINK_GetIDsOfNames(iface, &IID_NULL, names, 1, 0, ids) ==
                   DISP_E_UNKNOWNNAME &&
               ids[0] == DISPID_UNKNOWN,
           "a sink knows no names");

    // Invoke of event 1 calls the handler with the object and the arguments
    // in the order of the declaration; a by-reference argument comes back.
    VARIANTARG args[2];
    BSTR s = SysAllocString(L"x");
    VariantInit(&args[0]);
    V_VT(&args[0]) = VT_BYREF | VT_BSTR;
    V_BSTRREF(&args[0]) = &s;
    VariantInit(&args[1]);
    V_VT(&args[1]) = VT_I4;
    V_I4(&args[1]) = 4;
    DISPPARAMS params = {args, nullptr, 2, 0};
    expect(EVENT_SINK_Invoke(iface, 1, &IID_NULL, 0, DISPATCH_METHOD, &params,
                             nullptr, nullptr, nullptr) == S_OK &&
               sink.pinged == 4 && std::wstring(sink.seen) == L"x" &&
               std::wstring(s) == L"xy",
           "EVENT_SINK_Invoke calls the handler ('ping 4 x', s = xy)");
    expect(EVENT_SINK_Invoke(iface, 2, &IID_NULL, 0, DISPATCH_METHOD, &params,
                             nullptr, nullptr, nullptr) == S_OK,
           "an event without a handler is ignored");
    setup.record[0] = 0x10;
    expect(
        EVENT_SINK_Invoke(iface, 2, &IID_NULL, 0, DISPATCH_METHOD, &params,
                          nullptr, nullptr, nullptr) == DISP_E_MEMBERNOTFOUND,
        "flag 0x10: an unhandled event is DISP_E_MEMBERNOTFOUND");
    expect(
        EVENT_SINK_Invoke(iface, 1, &IID_IUnknown, 0, DISPATCH_METHOD, &params,
                          nullptr, nullptr, nullptr) == DISP_E_UNKNOWNINTERFACE,
        "Invoke for an interface is DISP_E_UNKNOWNINTERFACE");
    SysFreeString(s);
    EbLibraryLoad(nullptr);
    EbLibraryUnload();
}

// A sink that records the events fired at it.
class Listener final : public IDispatch {
public:
    int calls = 0;
    DISPID last = 0;
    LONG first_argument = 0;
    UINT count = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override {
        *out = this;
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n) override {
        *n = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID,
                                            DISPID*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* params, VARIANT*, EXCEPINFO*,
                                     UINT*) override {
        ++calls;
        last = id;
        count = params->cArgs;
        if (flags == DISPATCH_METHOD && params->cArgs == 2) {
            first_argument = V_I4(&params->rgvarg[1]);
        }
        return S_OK;
    }
};

void events() {
    // The source block: the object in slot 2, the sink list from slot 13.
    struct Node {
        Node* next;
        std::uintptr_t sink;
    };
    Object owner;
    Listener first;
    Listener second;
    std::vector<unsigned char> source(16 * kSlot, 0);
    IUnknown* const owner_pointer = &owner;
    std::memcpy(&source[2 * kSlot], &owner_pointer, sizeof owner_pointer);
    Node tail = {nullptr, reinterpret_cast<std::uintptr_t>(
                              static_cast<IDispatch*>(&second))};
    Node* const head = reinterpret_cast<Node*>(&source[13 * kSlot]);
    head->next = &tail;
    head->sink =
        reinterpret_cast<std::uintptr_t>(static_cast<IDispatch*>(&first));
    VARIANTARG args[2];
    VariantInit(&args[0]);
    V_VT(&args[0]) = VT_BSTR;
    V_BSTR(&args[0]) = SysAllocString(L"s");
    VariantInit(&args[1]);
    V_VT(&args[1]) = VT_I4;
    V_I4(&args[1]) = 3;
#if defined(_M_IX86)
    __vbaRaiseEvent(source.data(), 5, 2, args[0], args[1]);
#else
    __vbaRaiseEvent(source.data(), 5, 2, args);
#endif
    expect(first.calls == 1 && second.calls == 1 && first.last == 5 &&
               first.count == 2 && first.first_argument == 3,
           "RaiseEvent calls every connected sink with the arguments");
    expect((head->sink & 1) == 0 && owner.count == 1,
           "RaiseEvent leaves the list and the object as they were");
    std::vector<unsigned char> quiet(16 * kSlot, 0);
#if defined(_M_IX86)
    __vbaRaiseEvent(quiet.data(), 5, 0);
#else
    __vbaRaiseEvent(quiet.data(), 5, 0, nullptr);
#endif
    expect(first.calls == 1, "an object without sinks raises nothing");
    VariantClear(&args[0]);

    IUnknown* slot = nullptr;
    expect(SetMemEvent(nullptr, 0, &owner, &slot) == S_OK && slot == &owner &&
               owner.count == 2,
           "SetMemEvent stores the object, counted");
    expect(SetMemEvent(nullptr, 0, nullptr, &slot) == S_OK && slot == nullptr &&
               owner.count == 1,
           "SetMemEvent to Nothing releases it");
}

void arrays() {
    SAFEARRAYBOUND bound = {2, 0};
    SAFEARRAY* made = SafeArrayCreate(VT_I4, 1, &bound);
    SAFEARRAY* source = made;
    SAFEARRAY* destination = nullptr;
    expect(__vbaAryRecMove(nullptr, &destination, &source) == S_OK &&
               destination == made && source == nullptr,
           "AryRecMove moves the array");
    SAFEARRAY* other = SafeArrayCreate(VT_I4, 1, &bound);
    source = other;
    expect(__vbaAryRecMove(nullptr, &destination, &source) == S_OK &&
               destination == other && source == nullptr,
           "AryRecMove replaces and destroys the old array");
    SafeArrayDestroy(destination);
}

}  // namespace

int main() {
    variants();
    events_and_objects();
    sinks();
    events();
    arrays();
    if (failures == 0) {
        std::printf("vbrun members: all checks passed\n");
        return 0;
    }
    return 1;
}
