// REQ-0103: PutMemVar, PutMemObj, SetMemVar, the Event and NewObj forms, the
// event-sink reference counts and the record-array move.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

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
    expect(FAILED(PutMemVar(&target, &holder)), "PutMemVar fails with the call");
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

// A fake event sink: an interface pointer inside an object, its table header
// pointing at a record with the offset of the interface.
struct Sink {
    char pad[0x24];
    LONG refs;
    char more[0x8];
    void* table;  // the interface pointer is the address of this member
};

void sinks() {
    // The header sits just below the table: a pointer to the record at table
    // - 8 (32-bit layout; the bytes are written with memcpy), the record holds
    // the offset of the interface at +4.
    unsigned char record[16] = {};
    const std::uintptr_t offset = offsetof(Sink, table);
    std::memcpy(record + 4, &offset, sizeof(offset));
    unsigned char block[32] = {};
    const unsigned char* const record_pointer = record;
    std::memcpy(block + 4, &record_pointer, sizeof(record_pointer));
    Sink sink = {};
    sink.refs = 1;
    sink.table = block + 12;
    void* const iface = &sink.table;
    expect(EVENT_SINK_AddRef(iface) == 2 && sink.refs == 2,
           "EVENT_SINK_AddRef counts");
    expect(EVENT_SINK_Release(iface) == 1 && sink.refs == 1,
           "EVENT_SINK_Release counts");
    EbLibraryLoad(nullptr);
    EbLibraryUnload();
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
    arrays();
    if (failures == 0) {
        std::printf("vbrun members: all checks passed\n");
        return 0;
    }
    return 1;
}
