// For Each over arrays, Variants and a collection of the test.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <cwchar>

#include "../src/vbrun_helpers.hpp"

extern "C" {
LONG WINAPI __vbaForEachAry(unsigned short type, SAFEARRAY** state,
                            ULONG* index, VARIANT* out, SAFEARRAY* array);
LONG WINAPI __vbaNextEachAry(unsigned short type, SAFEARRAY** state,
                             ULONG* index, VARIANT* out);
void WINAPI __vbaExitEachAry(SAFEARRAY** state);
void WINAPI __vbaExitEachColl(IUnknown** slot);
LONG WINAPI __vbaForEachCollVar(IUnknown** slot, VARIANT* out,
                                IDispatch* object);
LONG WINAPI __vbaNextEachCollVar(IUnknown** slot, VARIANT* out);
LONG WINAPI __vbaForEachCollAd(IUnknown** slot, IDispatch** element,
                               IDispatch* object);
LONG WINAPI __vbaNextEachCollAd(IUnknown** slot, IDispatch** element);
LONG WINAPI __vbaForEachCollObj(const IID* iid, IUnknown** slot,
                                IUnknown** target, IDispatch* object);
LONG WINAPI __vbaNextEachCollObj(const IID* iid, IUnknown** slot,
                                 IUnknown** target);
LONG WINAPI __vbaForEachVar(SAFEARRAY** state, ULONG* index, LONG* kind,
                            IUnknown** enumerator, VARIANT* out,
                            const VARIANT* source);
LONG WINAPI __vbaNextEachVar(SAFEARRAY** state, ULONG* index,
                             const LONG* kind, IUnknown** enumerator,
                             VARIANT* out);
void WINAPI __vbaExitEachVar(SAFEARRAY** state, const LONG* kind,
                             IUnknown** enumerator);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// An enumerator of the numbers 10, 20 and 30.
class Numbers final : public IEnumVARIANT {
public:
    long references = 1;
    long position = 0;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                             void** out) override {
        if (iid == IID_IUnknown || iid == IID_IEnumVARIANT) {
            *out = static_cast<IEnumVARIANT*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override {
        const long left = --references;
        if (left == 0) {
            delete this;
        }
        return static_cast<ULONG>(left);
    }
    HRESULT STDMETHODCALLTYPE Next(ULONG, VARIANT* out, ULONG*) override {
        if (position >= 3) {
            return S_FALSE;
        }
        out->vt = VT_I4;
        out->lVal = static_cast<LONG>(++position * 10);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Reset() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT**) override {
        return E_NOTIMPL;
    }
};

class Collection final : public IDispatch {
public:
    long references = 1;
    bool objects = false;

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
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID,
                                            DISPID*) override {
        return DISP_E_UNKNOWNNAME;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD,
                                     DISPPARAMS*, VARIANT* result, EXCEPINFO*,
                                     UINT*) override {
        if (id != DISPID_NEWENUM) {
            return DISP_E_MEMBERNOTFOUND;
        }
        result->vt = VT_UNKNOWN;
        result->punkVal = new Numbers();
        return S_OK;
    }
};

class Nothing final : public IDispatch {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override {
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {
        *count = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID,
                                            DISPID*) override {
        return DISP_E_UNKNOWNNAME;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*,
                                     VARIANT*, EXCEPINFO*, UINT*) override {
        return DISP_E_MEMBERNOTFOUND;
    }
};

SAFEARRAY* longs(const long count) {
    SAFEARRAY* array = SafeArrayCreateVector(VT_I4, 0, count);
    for (long index = 0; index < count; ++index) {
        const long value = (index + 1) * 100;
        SafeArrayPutElement(array, &index, const_cast<long*>(&value));
    }
    return array;
}

}  // namespace

int main() {
    {
        SAFEARRAY* array = longs(3);
        SAFEARRAY* state = nullptr;
        ULONG index = 0;
        VARIANT out;
        VariantInit(&out);
        long total = 0;
        int rounds = 0;
        LONG more = __vbaForEachAry(VT_I4, &state, &index, &out, array);
        while (more != 0) {
            total += out.lVal;
            ++rounds;
            more = __vbaNextEachAry(VT_I4, &state, &index, &out);
        }
        expect(rounds == 3 && total == 600, "array elements");
        expect(state == nullptr && index == 0, "state after the last");
        expect(array->cLocks == 0, "array unlocked");
        SafeArrayDestroy(array);
    }
    {
        // A loop left early unlocks the array.
        SAFEARRAY* array = longs(2);
        SAFEARRAY* state = nullptr;
        ULONG index = 0;
        VARIANT out;
        VariantInit(&out);
        __vbaForEachAry(VT_I4, &state, &index, &out, array);
        expect(array->cLocks == 1, "array locked in the loop");
        __vbaExitEachAry(&state);
        expect(array->cLocks == 0 && state == nullptr, "Exit For");
        SafeArrayDestroy(array);
    }
    {
        SAFEARRAY* array = SafeArrayCreateVector(VT_BSTR, 0, 2);
        long at = 0;
        SafeArrayPutElement(array, &at, SysAllocString(L"one"));
        at = 1;
        SafeArrayPutElement(array, &at, SysAllocString(L"two"));
        SAFEARRAY* state = nullptr;
        ULONG index = 0;
        VARIANT out;
        VariantInit(&out);
        __vbaForEachAry(VT_BSTR, &state, &index, &out, array);
        expect(out.vt == VT_BSTR && std::wcscmp(out.bstrVal, L"one") == 0,
               "string element");
        __vbaNextEachAry(VT_BSTR, &state, &index, &out);
        expect(std::wcscmp(out.bstrVal, L"two") == 0, "second string");
        __vbaNextEachAry(VT_BSTR, &state, &index, &out);
        VariantClear(&out);
        SafeArrayDestroy(array);
    }
    {
        vbrun::clear_error();
        VARIANT out;
        VariantInit(&out);
        SAFEARRAY* state = nullptr;
        ULONG index = 0;
        __vbaForEachAry(VT_I4, &state, &index, &out, nullptr);
        expect(vbrun::last_error() == 92, "no array is error 92");
        vbrun::clear_error();
        SAFEARRAY* array = longs(1);
        __vbaForEachAry(VT_UNKNOWN, &state, &index, &out, array);
        expect(vbrun::last_error() == 458, "unsupported element type");
        vbrun::clear_error();
        __vbaExitEachAry(&state);
        SafeArrayDestroy(array);
        vbrun::clear_error();
    }
    {
        Collection collection;
        IUnknown* slot = nullptr;
        VARIANT out;
        VariantInit(&out);
        long total = 0;
        LONG more = __vbaForEachCollVar(&slot, &out, &collection);
        while (more != 0) {
            total += out.lVal;
            more = __vbaNextEachCollVar(&slot, &out);
        }
        expect(total == 60 && slot == nullptr, "collection elements");
        more = __vbaForEachCollVar(&slot, &out, &collection);
        expect(more != 0 && out.lVal == 10, "first element");
        __vbaExitEachColl(&slot);
        expect(slot == nullptr, "Exit For of a collection");
        vbrun::clear_error();
        Nothing nothing;
        expect(__vbaForEachCollVar(&slot, &out, &nothing) == 0 &&
                   vbrun::last_error() == 438,
               "an object without _NewEnum is error 438");
        vbrun::clear_error();
        expect(__vbaForEachCollVar(&slot, &out, nullptr) == 0 &&
                   vbrun::last_error() == 424,
               "no object is error 424");
        vbrun::clear_error();
    }
    {
        Collection collection;
        IUnknown* slot = nullptr;
        IDispatch* element = nullptr;
        expect(__vbaForEachCollAd(&slot, &element, &collection) == 0 ||
                   vbrun::last_error() == 424,
               "a number is not an object");
        vbrun::clear_error();
        if (slot != nullptr) {
            slot->Release();
            slot = nullptr;
        }
        IUnknown* target = nullptr;
        __vbaForEachCollObj(&IID_IDispatch, &slot, &target, &collection);
        expect(vbrun::last_error() == 424, "elements as objects");
        vbrun::clear_error();
        if (slot != nullptr) {
            slot->Release();
        }
    }
    {
        // Variants: an array and an object.
        SAFEARRAY* array = longs(2);
        VARIANT source;
        VariantInit(&source);
        source.vt = VT_ARRAY | VT_I4;
        source.parray = array;
        SAFEARRAY* state = nullptr;
        ULONG index = 0;
        LONG kind = -1;
        IUnknown* enumerator = nullptr;
        VARIANT out;
        VariantInit(&out);
        long total = 0;
        LONG more = __vbaForEachVar(&state, &index, &kind, &enumerator, &out,
                                    &source);
        while (more != 0) {
            total += out.lVal;
            more = __vbaNextEachVar(&state, &index, &kind, &enumerator, &out);
        }
        expect(kind == VT_I4 && total == 300, "array in a Variant");
        SafeArrayDestroy(array);

        Collection collection;
        source.vt = VT_DISPATCH;
        source.pdispVal = &collection;
        total = 0;
        more = __vbaForEachVar(&state, &index, &kind, &enumerator, &out,
                               &source);
        while (more != 0) {
            total += out.lVal;
            more = __vbaNextEachVar(&state, &index, &kind, &enumerator, &out);
        }
        expect(kind == 0 && total == 60, "object in a Variant");
        more = __vbaForEachVar(&state, &index, &kind, &enumerator, &out,
                               &source);
        __vbaExitEachVar(&state, &kind, &enumerator);
        expect(enumerator == nullptr, "Exit For on a Variant");
        vbrun::clear_error();
        source.vt = VT_I4;
        __vbaForEachVar(&state, &index, &kind, &enumerator, &out, &source);
        expect(vbrun::last_error() == 13, "a number is not enumerable");
        vbrun::clear_error();
    }
    if (failures == 0) {
        std::puts("foreach ok");
    }
    return failures == 0 ? 0 : 1;
}
