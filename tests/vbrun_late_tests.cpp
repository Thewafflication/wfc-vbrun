// Late binding helpers against a small IDispatch of the test.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void __cdecl __vbaLateMemCall(IDispatch* object, BSTR name, LONG count, ...);
VARIANT* __cdecl __vbaLateMemCallLd(VARIANT* result, IDispatch* object,
                                    BSTR name, LONG count, ...);
void __cdecl __vbaLateMemCallSt(IDispatch* object, BSTR name, LONG count, ...);
void __cdecl __vbaLateMemStAd(IDispatch* object, BSTR name, LONG count, ...);
void WINAPI __vbaLateMemSt(IDispatch* object, BSTR name, VARIANT value);
void __cdecl __vbaLateIdCall(IDispatch* object, LONG id, LONG count, ...);
VARIANT* __cdecl __vbaLateIdCallLd(VARIANT* result, IDispatch* object, LONG id,
                                   LONG count, ...);
void __cdecl __vbaLateIdCallSt(IDispatch* object, LONG id, LONG count, ...);
void __cdecl __vbaLateIdStAd(IDispatch* object, LONG id, LONG count, ...);
void WINAPI __vbaLateIdSt(IDispatch* object, LONG id, VARIANT value);
void __cdecl __vbaLateMemNamedCall(IDispatch* object, BSTR name, LONG count,
                                   LONG name_count, ...);
VARIANT* __cdecl __vbaLateMemNamedCallLd(VARIANT* result, IDispatch* object,
                                         BSTR name, LONG count,
                                         LONG name_count, ...);
void __cdecl __vbaLateIdNamedCall(IDispatch* object, LONG id, LONG count,
                                  LONG name_count, ...);
void __cdecl __vbaLateMemNamedCallSt(IDispatch* object, BSTR name, LONG count,
                                     LONG name_count, ...);
void __cdecl __vbaLateMemNamedStAd(IDispatch* object, BSTR name, LONG count,
                                   LONG name_count, ...);
void __cdecl __vbaLateIdNamedCallSt(IDispatch* object, LONG id, LONG count,
                                    LONG name_count, ...);
void __cdecl __vbaLateIdNamedStAd(IDispatch* object, LONG id, LONG count,
                                  LONG name_count, ...);
VARIANT* __cdecl __vbaVarLateMemCallLd(VARIANT* result, const VARIANT* holder,
                                       BSTR name, LONG count, ...);
VARIANT* __cdecl __vbaVarLateMemCallLdRf(VARIANT* result,
                                         const VARIANT* holder, BSTR name,
                                         LONG count, ...);
void __cdecl __vbaVarLateMemCallSt(const VARIANT* holder, BSTR name,
                                   LONG count, ...);
void __cdecl __vbaVarLateMemStAd(const VARIANT* holder, BSTR name, LONG count,
                                 ...);
void WINAPI __vbaVarLateMemSt(const VARIANT* holder, BSTR name,
                              VARIANT value);
VARIANT* __cdecl __vbaLateIdNamedCallLd(VARIANT* result, IDispatch* object,
                                        LONG id, LONG count, LONG name_count,
                                        ...);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// Members: Sum (10) adds its arguments, Value (11) is a property, Gone is
// unknown, Boom (12) fails with an exception, Tell (13) records its
// named arguments.
class Dispatch final : public IDispatch {
public:
    long references = 1;
    WORD last_flags = 0;
    DISPID last_id = 0;
    unsigned last_count = 0;
    unsigned last_named = 0;
    DISPID last_named_id = 0;
    long value = 0;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                             void** out) override {
        if (iid == IID_IUnknown || iid == IID_IDispatch) {
            *out = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {
        *count = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* names, UINT count,
                                            LCID, DISPID* ids) override {
        HRESULT result = S_OK;
        for (UINT index = 0; index < count; ++index) {
            const std::wstring name = names[index];
            if (name == L"Sum") {
                ids[index] = 10;
            } else if (name == L"Value") {
                ids[index] = 11;
            } else if (name == L"Boom") {
                ids[index] = 12;
            } else if (name == L"Tell") {
                ids[index] = 13;
            } else if (name == L"Who") {
                ids[index] = 21;
            } else {
                ids[index] = DISPID_UNKNOWN;
                result = DISP_E_UNKNOWNNAME;
            }
        }
        return result;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* parameters, VARIANT* result,
                                     EXCEPINFO* excep, UINT*) override {
        last_flags = flags;
        last_id = id;
        last_count = parameters->cArgs;
        last_named = parameters->cNamedArgs;
        last_named_id = parameters->cNamedArgs != 0
                            ? parameters->rgdispidNamedArgs[0]
                            : 0;
        if (id == 12) {
            excep->scode = static_cast<SCODE>(0x800A0035);
            return DISP_E_EXCEPTION;
        }
        if (id == 10) {
            long total = 0;
            for (UINT index = 0; index < parameters->cArgs; ++index) {
                total += parameters->rgvarg[index].lVal;
            }
            if (result != nullptr) {
                result->vt = VT_I4;
                result->lVal = total;
            }
            return S_OK;
        }
        if (id == 11) {
            if ((flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) !=
                0) {
                value = parameters->rgvarg[0].lVal;
                return S_OK;
            }
            if (result != nullptr) {
                result->vt = VT_I4;
                result->lVal = value;
            }
            return S_OK;
        }
        return DISP_E_MEMBERNOTFOUND;
    }
};

VARIANT number(const long value) {
    VARIANT variant;
    VariantInit(&variant);
    variant.vt = VT_I4;
    variant.lVal = value;
    return variant;
}

}  // namespace

int main() {
    Dispatch object;
    BSTR sum = SysAllocString(L"Sum");
    BSTR value = SysAllocString(L"Value");
    BSTR boom = SysAllocString(L"Boom");
    BSTR gone = SysAllocString(L"Gone");
    BSTR tell = SysAllocString(L"Tell");
    BSTR who = SysAllocString(L"Who");

    VARIANT result;
    VariantInit(&result);
    __vbaLateMemCallLd(&result, &object, sum, 3, number(1), number(2),
                       number(4));
    expect(result.vt == VT_I4 && result.lVal == 7, "call with result");
    expect((object.last_flags & DISPATCH_PROPERTYGET) != 0 &&
               (object.last_flags & DISPATCH_METHOD) != 0 &&
               object.last_count == 3,
           "call flags and count");

    __vbaLateMemCall(&object, sum, 1, number(5));
    expect(object.last_flags == DISPATCH_METHOD && object.last_id == 10,
           "call as a statement");

    __vbaLateMemSt(&object, value, number(42));
    expect(object.value == 42 && object.last_flags == DISPATCH_PROPERTYPUT &&
               object.last_named == 1 &&
               object.last_named_id == DISPID_PROPERTYPUT,
           "property assignment");
    VariantInit(&result);
    __vbaLateMemCallLd(&result, &object, value, 0);
    expect(result.vt == VT_I4 && result.lVal == 42, "property read");

    __vbaLateMemCallSt(&object, value, 0, number(7));
    expect(object.value == 7 && object.last_count == 1,
           "call-style assignment");

    vbrun::clear_error();
    __vbaLateMemStAd(&object, value, 0, number(9));
    expect(vbrun::last_error() == 424, "Set of a non-object is error 424");
    vbrun::clear_error();
    {
        VARIANT reference;
        VariantInit(&reference);
        reference.vt = VT_DISPATCH;
        reference.pdispVal = &object;
        __vbaLateMemStAd(&object, value, 0, reference);
        expect(vbrun::last_error() == 0 &&
                   object.last_flags == DISPATCH_PROPERTYPUTREF,
               "Set assignment");
    }

    vbrun::clear_error();
    __vbaLateMemCall(&object, gone, 0);
    expect(vbrun::last_error() == 438, "unknown member is error 438");
    vbrun::clear_error();
    __vbaLateMemCall(&object, boom, 0);
    expect(vbrun::last_error() == 0x35, "exception code of the object");
    vbrun::clear_error();
    __vbaLateMemCall(nullptr, sum, 0);
    expect(vbrun::last_error() == 91, "no object is error 91");
    vbrun::clear_error();

    VariantInit(&result);
    __vbaLateIdCallLd(&result, &object, 10, 2, number(3), number(4));
    expect(result.vt == VT_I4 && result.lVal == 7, "call by id");
    __vbaLateIdCall(&object, 10, 0);
    expect(object.last_flags == DISPATCH_METHOD, "call by id as a statement");
    __vbaLateIdSt(&object, 11, number(5));
    expect(object.value == 5, "assignment by id");
    __vbaLateIdCallSt(&object, 11, 0, number(6));
    expect(object.value == 6, "call-style assignment by id");
    __vbaLateIdStAd(&object, 11, 0, number(8));
    expect(vbrun::last_error() == 424, "Set by id needs an object");
    vbrun::clear_error();

    VariantInit(&result);
    __vbaLateMemNamedCallLd(&result, &object, sum, 2, 1, who, number(1),
                            number(2));
    expect(result.vt == VT_I4 && result.lVal == 3 && object.last_named == 1 &&
               object.last_named_id == 21,
           "named argument by name");
    __vbaLateMemNamedCall(&object, sum, 1, 1, who, number(1));
    expect(object.last_flags == DISPATCH_METHOD && object.last_named == 1,
           "named argument as a statement");
    vbrun::clear_error();
    BSTR missing = SysAllocString(L"Nope");
    __vbaLateMemNamedCall(&object, sum, 1, 1, missing, number(1));
    expect(vbrun::last_error() == 448, "unknown argument name is error 448");
    vbrun::clear_error();
    SysFreeString(missing);

    VariantInit(&result);
    __vbaLateIdNamedCallLd(&result, &object, 10, 2, 1, 33, number(1),
                           number(2));
    expect(result.lVal == 3 && object.last_named_id == 33,
           "named argument by id");
    __vbaLateIdNamedCall(&object, 10, 1, 1, 34, number(1));
    expect(object.last_named_id == 34, "named argument by id as a statement");
    __vbaLateIdNamedCall(&object, 10, 2, 2, 1, 2, number(1), number(2));
    expect(vbrun::last_error() == 5, "more than one named argument by id");
    vbrun::clear_error();

    __vbaLateMemNamedCallSt(&object, value, 0, 1, who, number(21));
    expect(object.value == 21 && object.last_named == 2 &&
               object.last_flags == DISPATCH_PROPERTYPUT,
           "named assignment");
    vbrun::clear_error();
    __vbaLateMemNamedStAd(&object, value, 0, 1, who, number(22));
    expect(vbrun::last_error() == 424, "named Set needs an object");
    vbrun::clear_error();
    __vbaLateIdNamedCallSt(&object, 11, 0, 1, 40, number(23));
    expect(object.value == 23 && object.last_named == 2 &&
               object.last_named_id == DISPID_PROPERTYPUT,
           "named assignment by id");
    __vbaLateIdNamedStAd(&object, 11, 0, 1, 40, number(24));
    expect(vbrun::last_error() == 424, "named Set by id needs an object");
    vbrun::clear_error();

    {
        VARIANT holder;
        VariantInit(&holder);
        holder.vt = VT_DISPATCH;
        holder.pdispVal = &object;
        VARIANT outer;
        VariantInit(&outer);
        outer.vt = VT_VARIANT | VT_BYREF;
        outer.pvarVal = &holder;
        VariantInit(&result);
        __vbaVarLateMemCallLd(&result, &holder, sum, 2, number(5), number(6));
        expect(result.vt == VT_I4 && result.lVal == 11, "call on a Variant");
        VariantInit(&result);
        __vbaVarLateMemCallLdRf(&result, &outer, sum, 1, number(9));
        expect(result.lVal == 9, "call on a Variant reference");
        __vbaVarLateMemSt(&holder, value, number(31));
        expect(object.value == 31, "assignment on a Variant");
        __vbaVarLateMemCallSt(&holder, value, 0, number(32));
        expect(object.value == 32, "call-style assignment on a Variant");
        vbrun::clear_error();
        __vbaVarLateMemStAd(&holder, value, 0, number(33));
        expect(vbrun::last_error() == 424, "Set on a Variant needs an object");
        vbrun::clear_error();
        VARIANT plain = number(1);
        __vbaVarLateMemCallLd(&result, &plain, sum, 0);
        expect(vbrun::last_error() == 424, "not an object is error 424");
        vbrun::clear_error();
        __vbaVarLateMemCallLd(&result, nullptr, sum, 0);
        expect(vbrun::last_error() == 91, "no Variant is error 91");
        vbrun::clear_error();
    }

    SysFreeString(sum);
    SysFreeString(value);
    SysFreeString(boom);
    SysFreeString(gone);
    SysFreeString(tell);
    SysFreeString(who);
    if (failures == 0) {
        std::puts("late ok");
    }
    return failures == 0 ? 0 : 1;
}
