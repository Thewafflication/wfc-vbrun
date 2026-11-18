// The helpers called with their operands in registers (the Integer, Byte, Abs
// and Sgn helpers, the string, object and Variant free, copy and move
// helpers) and the Zombie interface, called as compiled code calls them.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>

#include "../src/vbrun_helpers.hpp"
#include "../src/vbrun_regcall.hpp"

extern "C" {
SHORT VBRUN_FASTCALL __vbaI2I4(LONG value);
BYTE VBRUN_FASTCALL __vbaUI1I2(SHORT value);
BYTE VBRUN_FASTCALL __vbaUI1I4(LONG value);
SHORT VBRUN_FASTCALL __vbaI2Abs(SHORT value);
LONG VBRUN_FASTCALL __vbaI4Abs(LONG value);
LONG VBRUN_FASTCALL __vbaI2Sgn(SHORT value);
LONG VBRUN_FASTCALL __vbaI4Sgn(LONG value);
LONG VBRUN_FASTCALL __vbaUI1Sgn(BYTE value);
BSTR VBRUN_FASTCALL __vbaStrCopy(BSTR* slot, BSTR source);
BSTR VBRUN_FASTCALL __vbaStrMove(BSTR* slot, BSTR source);
void VBRUN_FASTCALL __vbaFreeStr(BSTR* slot);
void VBRUN_FASTCALL __vbaFreeObj(IUnknown** slot);
void VBRUN_FASTCALL __vbaFreeVar(VARIANT* variant);
VARIANT* VBRUN_FASTCALL __vbaVarCopy(VARIANT* destination,
                                     const VARIANT* source);
VARIANT* VBRUN_FASTCALL __vbaVarDup(VARIANT* destination,
                                    const VARIANT* source);
VARIANT* VBRUN_FASTCALL __vbaVarZero(VARIANT* destination, VARIANT* source);
VARIANT* VBRUN_FASTCALL __vbaVargParmRef(VARIANT* argument);
VARIANT* VBRUN_FASTCALL __vbaVargVar(VARIANT* source, std::uintptr_t unused,
                                     VARIANT* result);
VARIANT* VBRUN_FASTCALL __vbaVargVarRef(VARIANT* destination, VARIANT* source);
VARIANT* VBRUN_FASTCALL __vbaVarMove(VARIANT* destination, VARIANT* source);
void VBRUN_FASTCALL __vbaFreeVarg(VARIANT* variant);
VARIANT* VBRUN_FASTCALL __vbaVargVarMove(VARIANT* target, VARIANT* source);
VARIANT* VBRUN_FASTCALL __vbaVargVarCopy(VARIANT* target, VARIANT* source);
VARIANT* VBRUN_FASTCALL __vbaAryVarVarg(VARIANT* result, SAFEARRAY* source);
SAFEARRAY** WINAPI __vbaLdZeroAry(SAFEARRAY** slot, SAFEARRAY* source);
VARIANT* VBRUN_FASTCALL __vbaVarVargNofree(VARIANT* destination,
                                           VARIANT* source);
}

struct Zombie {
    void* table;
    void* reserved;
    IUnknown* inner;
};

extern "C" {
ULONG WINAPI Zombie_AddRef(Zombie* self);
ULONG WINAPI Zombie_Release(Zombie* self);
HRESULT WINAPI Zombie_QueryInterface(Zombie* self, const IID* iid, void** out);
HRESULT WINAPI Zombie_GetTypeInfoCount(Zombie* self, UINT* count);
HRESULT WINAPI Zombie_GetTypeInfo(Zombie* self, UINT index, LCID lcid,
                                  ITypeInfo** info);
HRESULT WINAPI Zombie_GetIDsOfNames(Zombie* self, const IID* iid,
                                    OLECHAR** names, UINT count, LCID lcid,
                                    DISPID* ids);
ULONG WINAPI BASIC_CLASS_AddRef(Zombie* self);
ULONG WINAPI BASIC_CLASS_Release(Zombie* self);
HRESULT WINAPI BASIC_CLASS_QueryInterface(Zombie* self, const IID* iid,
                                          void** out);
HRESULT WINAPI BASIC_DISPINTERFACE_GetTICount(void* self, UINT* count);
HRESULT WINAPI BASIC_CLASS_GetIDsOfNames(Zombie* self, const IID* iid,
                                         OLECHAR** names, UINT count,
                                         LCID locale, DISPID* ids);
HRESULT WINAPI BASIC_CLASS_Invoke(Zombie* self, DISPID member, const IID* iid,
                                  LCID locale, WORD flags, DISPPARAMS* params,
                                  VARIANT* result, EXCEPINFO* excep,
                                  UINT* argument_error);
HRESULT WINAPI BASIC_DISPINTERFACE_GetTypeInfo(Zombie* self, UINT index,
                                               LCID locale, ITypeInfo** info);
HRESULT WINAPI Zombie_Invoke(Zombie* self, DISPID id, const IID* iid, LCID lcid,
                             WORD flags, DISPPARAMS* params, VARIANT* result,
                             EXCEPINFO* info, UINT* arg);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// An object that counts its references and answers one interface.
class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (iid == IID_IUnknown) {
            *out = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++count; }
    ULONG STDMETHODCALLTYPE Release() override { return --count; }
    ULONG count = 1;
};

void conversions() {
    vbrun::clear_error();
    expect(__vbaI2I4(32767) == 32767 && vbrun::last_error() == 0,
           "CInt(32767)");
    expect(__vbaI2I4(-32768) == -32768, "CInt(-32768)");
    expect(__vbaI2I4(32768) == 0 && vbrun::last_error() == 6,
           "CInt(32768) overflows");
    vbrun::clear_error();
    expect(__vbaUI1I2(255) == 255 && vbrun::last_error() == 0, "CByte(255)");
    expect(__vbaUI1I2(256) == 0 && vbrun::last_error() == 6,
           "CByte(256) overflows");
    vbrun::clear_error();
    expect(__vbaUI1I2(-1) == 0 && vbrun::last_error() == 6,
           "CByte(-1) overflows");
    vbrun::clear_error();
    expect(__vbaUI1I4(200) == 200 && vbrun::last_error() == 0, "CByte(200&)");
    expect(__vbaUI1I4(65536) == 0 && vbrun::last_error() == 6,
           "CByte(65536) overflows");
    vbrun::clear_error();
    expect(__vbaUI1I4(-5) == 0 && vbrun::last_error() == 6,
           "CByte(-5&) overflows");
    vbrun::clear_error();
}

void abs_and_sgn() {
    vbrun::clear_error();
    expect(__vbaI2Abs(-7) == 7 && __vbaI2Abs(7) == 7 && __vbaI2Abs(0) == 0,
           "Abs of an Integer");
    expect(__vbaI2Abs(-32767) == 32767 && vbrun::last_error() == 0,
           "Abs(-32767)");
    static_cast<void>(__vbaI2Abs(-32768));
    expect(vbrun::last_error() == 6, "Abs(-32768) overflows");
    vbrun::clear_error();
    expect(__vbaI4Abs(-70000) == 70000 && __vbaI4Abs(5) == 5, "Abs of a Long");
    static_cast<void>(__vbaI4Abs(INT32_MIN));
    expect(vbrun::last_error() == 6, "Abs(-2147483648) overflows");
    vbrun::clear_error();
    expect(__vbaI2Sgn(-9) == -1 && __vbaI2Sgn(0) == 0 && __vbaI2Sgn(9) == 1,
           "Sgn of an Integer");
    expect(__vbaI4Sgn(INT32_MIN) == -1 && __vbaI4Sgn(0) == 0 &&
               __vbaI4Sgn(INT32_MAX) == 1,
           "Sgn of a Long");
    expect(__vbaUI1Sgn(0) == 0 && __vbaUI1Sgn(1) == 1 && __vbaUI1Sgn(255) == 1,
           "Sgn of a Byte");
}

void strings_and_objects() {
    BSTR slot = nullptr;
    BSTR source = SysAllocString(L"hello");
    BSTR copy = __vbaStrCopy(&slot, source);
    expect(copy == slot && copy != source, "a copy is a new string");
    expect(copy != nullptr && std::wcscmp(copy, L"hello") == 0,
           "the copy holds the text");
    BSTR second = __vbaStrCopy(&slot, nullptr);
    expect(second == nullptr && slot == nullptr, "a copy of Null empties");
    slot = SysAllocString(L"old");
    const BSTR moved = __vbaStrMove(&slot, source);
    expect(moved == source && slot == source, "a move keeps the string");
    __vbaFreeStr(&slot);
    expect(slot == nullptr, "FreeStr clears the slot");
    __vbaFreeStr(&slot);
    expect(slot == nullptr, "FreeStr of an empty slot");

    Counted object;
    object.count = 2;
    IUnknown* held = &object;
    __vbaFreeObj(&held);
    expect(held == nullptr && object.count == 1, "FreeObj releases");
    __vbaFreeObj(&held);
    expect(object.count == 1, "FreeObj of an empty slot");
}

void variants() {
    VARIANT text;
    VariantInit(&text);
    V_VT(&text) = VT_BSTR;
    V_BSTR(&text) = SysAllocString(L"abc");
    VARIANT other;
    VariantInit(&other);
    V_VT(&other) = VT_BSTR;
    V_BSTR(&other) = SysAllocString(L"old");

    VARIANT* const copied = __vbaVarCopy(&other, &text);
    expect(copied == &other && V_VT(&other) == VT_BSTR &&
               V_BSTR(&other) != V_BSTR(&text) &&
               std::wcscmp(V_BSTR(&other), L"abc") == 0,
           "VarCopy duplicates the string");
    expect(__vbaVarCopy(&text, &text) == &text &&
               std::wcscmp(V_BSTR(&text), L"abc") == 0,
           "VarCopy onto itself");

    VARIANT duplicate;
    std::memset(&duplicate, 0xCC, sizeof(duplicate));
    __vbaVarDup(&duplicate, &text);
    expect(V_VT(&duplicate) == VT_BSTR &&
               std::wcscmp(V_BSTR(&duplicate), L"abc") == 0,
           "VarDup builds a copy");

    VARIANT moved;
    VariantInit(&moved);
    V_VT(&moved) = VT_I4;
    V_I4(&moved) = 5;
    __vbaVarZero(&moved, &duplicate);
    expect(V_VT(&moved) == VT_BSTR && V_VT(&duplicate) == VT_EMPTY,
           "VarZero moves the value");
    __vbaFreeVar(&moved);
    expect(V_VT(&moved) == VT_EMPTY, "FreeVar empties");

    VARIANT number;
    VariantInit(&number);
    V_VT(&number) = VT_I2;
    V_I2(&number) = 9;
    __vbaFreeVar(&number);
    expect(V_VT(&number) == VT_I2, "FreeVar leaves an Integer");
    __vbaFreeVar(&other);
    __vbaFreeVar(&text);

    // By-reference Variant arguments.
    VARIANT value;
    VariantInit(&value);
    V_VT(&value) = VT_I4;
    V_I4(&value) = 77;
    VARIANT reference;
    __vbaVargVar(&value, 0, &reference);
    expect(V_VT(&reference) == (VT_VARIANT | VT_BYREF) &&
               V_VARIANTREF(&reference) == &value,
           "a value is passed by reference");
    expect(__vbaVargParmRef(&reference) == &value,
           "the Variant behind a reference");
    expect(__vbaVargParmRef(&value) == &value, "a value is its own");
    VARIANT again;
    __vbaVargVar(&reference, 0, &again);
    expect(V_VT(&again) == (VT_VARIANT | VT_BYREF) &&
               V_VARIANTREF(&again) == &value,
           "a reference stays as it is");
    VARIANT third;
    __vbaVargVarRef(&third, &value);
    expect(V_VT(&third) == (VT_VARIANT | VT_BYREF) &&
               V_VARIANTREF(&third) == &value,
           "VargVarRef makes a reference");
}

// An object whose default property is the number 41.
class WithDefault final : public IDispatch {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override {
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
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS*,
                                     VARIANT* result, EXCEPINFO*,
                                     UINT*) override {
        if (id != DISPID_VALUE || result == nullptr) {
            return DISP_E_MEMBERNOTFOUND;
        }
        V_VT(result) = VT_I4;
        V_I4(result) = 41;
        return S_OK;
    }
    ULONG count = 1;
};

void more_variants() {
    // VarMove moves a string and empties the source.
    VARIANT target;
    VariantInit(&target);
    V_VT(&target) = VT_BSTR;
    V_BSTR(&target) = SysAllocString(L"old");
    VARIANT from;
    VariantInit(&from);
    V_VT(&from) = VT_BSTR;
    V_BSTR(&from) = SysAllocString(L"new");
    const BSTR held = V_BSTR(&from);
    expect(__vbaVarMove(&target, &from) == &target &&
               V_VT(&target) == VT_BSTR && V_BSTR(&target) == held &&
               V_VT(&from) == VT_EMPTY,
           "VarMove moves the string");
    __vbaFreeVar(&target);

    // An object that is moved gives its default value and is released.
    WithDefault object;
    object.count = 2;
    VARIANT holder;
    VariantInit(&holder);
    V_VT(&holder) = VT_DISPATCH;
    V_DISPATCH(&holder) = &object;
    VARIANT result;
    VariantInit(&result);
    __vbaVarMove(&result, &holder);
    expect(V_VT(&result) == VT_I4 && V_I4(&result) == 41 &&
               V_VT(&holder) == VT_EMPTY && object.count == 1,
           "VarMove of an object gives the default value");

    // FreeVarg frees through a reference.
    BSTR text = SysAllocString(L"x");
    VARIANT by_text;
    VariantInit(&by_text);
    V_VT(&by_text) = VT_BSTR | VT_BYREF;
    V_BSTRREF(&by_text) = &text;
    __vbaFreeVarg(&by_text);
    expect(text == nullptr, "FreeVarg frees a string through its reference");
    Counted counted;
    counted.count = 2;
    IUnknown* pointer = &counted;
    VARIANT by_object;
    VariantInit(&by_object);
    V_VT(&by_object) = VT_UNKNOWN | VT_BYREF;
    V_UNKNOWNREF(&by_object) = &pointer;
    __vbaFreeVarg(&by_object);
    expect(pointer == nullptr && counted.count == 1,
           "FreeVarg releases an object through its reference");
    VARIANT own;
    VariantInit(&own);
    V_VT(&own) = VT_BSTR;
    V_BSTR(&own) = SysAllocString(L"own");
    __vbaFreeVarg(&own);
    expect(V_VT(&own) == VT_EMPTY, "FreeVarg of a plain Variant");

    // VarVargNofree reads through a reference without copying what it owns.
    VARIANT plain;
    VariantInit(&plain);
    V_VT(&plain) = VT_I4;
    VARIANT scratch;
    VariantInit(&scratch);
    expect(__vbaVarVargNofree(&scratch, &plain) == &plain,
           "a plain Variant is returned as it is");
    LONG number = 123;
    VARIANT by_number;
    VariantInit(&by_number);
    V_VT(&by_number) = VT_I4 | VT_BYREF;
    V_I4REF(&by_number) = &number;
    expect(__vbaVarVargNofree(&scratch, &by_number) == &scratch &&
               V_VT(&scratch) == VT_I4 && V_I4(&scratch) == 123,
           "a reference to a Long is read");
    double real = 2.5;
    V_VT(&by_number) = VT_R8 | VT_BYREF;
    V_R8REF(&by_number) = &real;
    __vbaVarVargNofree(&scratch, &by_number);
    expect(V_VT(&scratch) == VT_R8 && V_R8(&scratch) == 2.5,
           "a reference to a Double is read");
    VARIANT inner;
    VariantInit(&inner);
    VARIANT outer;
    VariantInit(&outer);
    V_VT(&outer) = VT_VARIANT | VT_BYREF;
    V_VARIANTREF(&outer) = &inner;
    expect(__vbaVarVargNofree(&scratch, &outer) == &inner,
           "a reference to a Variant gives that Variant");
}

// An object with one named member (Value, DISPID 3) that answers 77.
class Named final : public IDispatch {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (iid == IID_IUnknown || iid == IID_IDispatch) {
            *out = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++count; }
    ULONG STDMETHODCALLTYPE Release() override { return --count; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n) override {
        *n = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID,
                                          ITypeInfo** info) override {
        *info = nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* names, UINT, LCID,
                                            DISPID* ids) override {
        ids[0] = names[0][0] == L'V' ? 3 : DISPID_UNKNOWN;
        return ids[0] == 3 ? S_OK : DISP_E_UNKNOWNNAME;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS*,
                                     VARIANT* result, EXCEPINFO*,
                                     UINT*) override {
        if (id != 3) {
            return DISP_E_MEMBERNOTFOUND;
        }
        V_VT(result) = VT_I4;
        V_I4(result) = 77;
        return S_OK;
    }
    ULONG count = 1;
};

void basic_class() {
    Counted inner;
    Zombie object{nullptr, nullptr, &inner};
    expect(
        BASIC_CLASS_AddRef(&object) == 2 && BASIC_CLASS_Release(&object) == 1,
        "BASIC_CLASS reference counting");
    void* out = nullptr;
    expect(BASIC_CLASS_QueryInterface(&object, &IID_IUnknown, &out) == S_OK &&
               out == static_cast<IUnknown*>(&inner),
           "BASIC_CLASS QueryInterface");
    inner.count = 1;
    UINT count = 0;
    expect(
        BASIC_DISPINTERFACE_GetTICount(nullptr, &count) == S_OK && count == 1,
        "a dispinterface has one type information");
    // The dispatch methods are the object's own; another interface than
    // IID_NULL is DISP_E_UNKNOWNINTERFACE.
    Named named;
    Zombie dispatch{nullptr, nullptr, &named};
    OLECHAR value[] = L"Value";
    OLECHAR* names[] = {value};
    DISPID id = 0;
    expect(BASIC_CLASS_GetIDsOfNames(&dispatch, &IID_NULL, names, 1, 0, &id) ==
                   S_OK &&
               id == 3 && named.count == 1,
           "BASIC_CLASS_GetIDsOfNames asks the object");
    expect(BASIC_CLASS_GetIDsOfNames(&dispatch, &IID_IUnknown, names, 1, 0,
                                     &id) == DISP_E_UNKNOWNINTERFACE,
           "GetIDsOfNames for an interface is DISP_E_UNKNOWNINTERFACE");
    DISPPARAMS none = {nullptr, nullptr, 0, 0};
    VARIANT result;
    VariantInit(&result);
    expect(BASIC_CLASS_Invoke(&dispatch, 3, &IID_NULL, 0, DISPATCH_PROPERTYGET,
                              &none, &result, nullptr, nullptr) == S_OK &&
               V_VT(&result) == VT_I4 && V_I4(&result) == 77,
           "BASIC_CLASS_Invoke calls the object");
    expect(BASIC_CLASS_Invoke(&object, 3, &IID_NULL, 0, DISPATCH_PROPERTYGET,
                              &none, &result, nullptr,
                              nullptr) == DISP_E_MEMBERNOTFOUND,
           "an object without IDispatch has no members");
    ITypeInfo* info = reinterpret_cast<ITypeInfo*>(&named);
    expect(BASIC_DISPINTERFACE_GetTypeInfo(&dispatch, 1, 0, &info) ==
               static_cast<HRESULT>(0x8002802BU),
           "a dispinterface has no type information 1");
    expect(BASIC_DISPINTERFACE_GetTypeInfo(&dispatch, 0, 0, &info) == S_OK &&
               info == nullptr,
           "type information 0 is the object's");
}

void zombie() {
    Counted inner;
    Zombie dead{nullptr, nullptr, &inner};
    expect(Zombie_AddRef(&dead) == 2 && inner.count == 2, "Zombie AddRef");
    expect(Zombie_Release(&dead) == 1 && inner.count == 1, "Zombie Release");
    void* out = &dead;
    expect(Zombie_QueryInterface(&dead, &IID_IUnknown, &out) == S_OK &&
               out == static_cast<IUnknown*>(&inner),
           "Zombie QueryInterface");
    inner.count = 1;
    expect(Zombie_QueryInterface(&dead, &IID_IUnknown, nullptr) == E_INVALIDARG,
           "a null result pointer");
    void* none = &dead;
    expect(
        Zombie_QueryInterface(&dead, &IID_IDispatch, &none) == E_NOINTERFACE &&
            none == nullptr,
        "an unknown interface");
    const auto failure = static_cast<HRESULT>(0x80010007U);
    UINT count = 7;
    expect(Zombie_GetTypeInfoCount(&dead, &count) == failure,
           "GetTypeInfoCount fails");
    expect(Zombie_GetTypeInfo(&dead, 0, 0, nullptr) == failure,
           "GetTypeInfo fails");
    expect(Zombie_GetIDsOfNames(&dead, &IID_NULL, nullptr, 0, 0, nullptr) ==
               failure,
           "GetIDsOfNames fails");
    expect(Zombie_Invoke(&dead, 0, &IID_NULL, 0, 0, nullptr, nullptr, nullptr,
                         nullptr) == failure,
           "Invoke fails");
}

void through_references() {
    // A value is converted to the type the reference points at.
    long number = 0;
    VARIANT by_long;
    VariantInit(&by_long);
    V_VT(&by_long) = VT_I4 | VT_BYREF;
    V_I4REF(&by_long) = &number;
    VARIANT source;
    VariantInit(&source);
    V_VT(&source) = VT_R8;
    V_R8(&source) = 7.4;
    expect(__vbaVargVarCopy(&by_long, &source) == &by_long && number == 7 &&
               V_VT(&source) == VT_R8,
           "VargVarCopy converts and leaves the source");

    // A string moves in and the source is freed; the old string is freed.
    BSTR text = SysAllocString(L"old");
    VARIANT by_text;
    VariantInit(&by_text);
    V_VT(&by_text) = VT_BSTR | VT_BYREF;
    V_BSTRREF(&by_text) = &text;
    VARIANT from;
    VariantInit(&from);
    V_VT(&from) = VT_BSTR;
    V_BSTR(&from) = SysAllocString(L"new");
    __vbaVargVarMove(&by_text, &from);
    expect(text != nullptr && std::wcscmp(text, L"new") == 0 &&
               V_VT(&from) == VT_EMPTY,
           "VargVarMove stores the string and frees the source");
    SysFreeString(text);

    // Into a Variant reference the Variant is copied.
    VARIANT inner;
    VariantInit(&inner);
    VARIANT by_variant;
    VariantInit(&by_variant);
    V_VT(&by_variant) = VT_VARIANT | VT_BYREF;
    V_VARIANTREF(&by_variant) = &inner;
    __vbaVargVarCopy(&by_variant, &source);
    expect(V_VT(&inner) == VT_R8 && V_R8(&inner) == 7.4,
           "VargVarCopy into a Variant reference");

    // The target that is not a reference is a plain move.
    VARIANT plain;
    VariantInit(&plain);
    VARIANT value;
    VariantInit(&value);
    V_VT(&value) = VT_I2;
    V_I2(&value) = 5;
    __vbaVargVarMove(&plain, &value);
    expect(
        V_VT(&plain) == VT_I2 && V_I2(&plain) == 5 && V_VT(&value) == VT_EMPTY,
        "VargVarMove into a plain Variant");

    // AryVarVarg copies an array of Variants.
    SAFEARRAYBOUND bound = {3, 0};
    SAFEARRAY* array = SafeArrayCreate(VT_VARIANT, 1, &bound);
    VARIANT* elements = static_cast<VARIANT*>(array->pvData);
    for (int index = 0; index < 3; ++index) {
        V_VT(&elements[index]) = VT_I4;
        V_I4(&elements[index]) = index + 10;
    }
    VARIANT copied;
    VariantInit(&copied);
    __vbaAryVarVarg(&copied, array);
    VARIANT* made = static_cast<VARIANT*>(V_ARRAY(&copied)->pvData);
    expect(V_VT(&copied) == (VT_ARRAY | VT_VARIANT) &&
               V_ARRAY(&copied) != array && V_I4(&made[2]) == 12 &&
               V_ARRAY(&copied)->rgsabound[0].cElements == 3,
           "AryVarVarg copies the array");
    VariantClear(&copied);

    // LdZeroAry takes the data of the source descriptor over.
    SAFEARRAY* slot = nullptr;
    void* data = array->pvData;
    __vbaLdZeroAry(&slot, array);
    expect(slot != nullptr && slot->pvData == data &&
               array->pvData == nullptr &&
               slot->cbElements == sizeof(VARIANT) &&
               slot->rgsabound[0].cElements == 3,
           "LdZeroAry moves the data to a new descriptor");
    SafeArrayDestroy(slot);
    SafeArrayDestroy(array);
}

}  // namespace

int main() {
    conversions();
    abs_and_sgn();
    strings_and_objects();
    variants();
    more_variants();
    basic_class();
    zombie();
    through_references();
    if (failures == 0) {
        std::printf("vbrun regcall: all checks passed\n");
        return 0;
    }
    return 1;
}
