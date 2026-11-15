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
VARIANT* VBRUN_FASTCALL __vbaVargVarRef(VARIANT* destination,
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
HRESULT WINAPI Zombie_QueryInterface(Zombie* self, const IID* iid,
                                     void** out);
HRESULT WINAPI Zombie_GetTypeInfoCount(Zombie* self, UINT* count);
HRESULT WINAPI Zombie_GetTypeInfo(Zombie* self, UINT index, LCID lcid,
                                  ITypeInfo** info);
HRESULT WINAPI Zombie_GetIDsOfNames(Zombie* self, const IID* iid,
                                    OLECHAR** names, UINT count, LCID lcid,
                                    DISPID* ids);
HRESULT WINAPI Zombie_Invoke(Zombie* self, DISPID id, const IID* iid,
                             LCID lcid, WORD flags, DISPPARAMS* params,
                             VARIANT* result, EXCEPINFO* info, UINT* arg);
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
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                             void** out) override {
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
    expect(__vbaUI1I4(200) == 200 && vbrun::last_error() == 0,
           "CByte(200&)");
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
    expect(__vbaI4Abs(-70000) == 70000 && __vbaI4Abs(5) == 5,
           "Abs of a Long");
    static_cast<void>(__vbaI4Abs(INT32_MIN));
    expect(vbrun::last_error() == 6, "Abs(-2147483648) overflows");
    vbrun::clear_error();
    expect(__vbaI2Sgn(-9) == -1 && __vbaI2Sgn(0) == 0 && __vbaI2Sgn(9) == 1,
           "Sgn of an Integer");
    expect(__vbaI4Sgn(INT32_MIN) == -1 && __vbaI4Sgn(0) == 0 &&
               __vbaI4Sgn(INT32_MAX) == 1,
           "Sgn of a Long");
    expect(__vbaUI1Sgn(0) == 0 && __vbaUI1Sgn(1) == 1 &&
               __vbaUI1Sgn(255) == 1,
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
    expect(Zombie_QueryInterface(&dead, &IID_IUnknown, nullptr) ==
               E_INVALIDARG,
           "a null result pointer");
    void* none = &dead;
    expect(Zombie_QueryInterface(&dead, &IID_IDispatch, &none) ==
                   E_NOINTERFACE &&
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
    expect(Zombie_Invoke(&dead, 0, &IID_NULL, 0, 0, nullptr, nullptr,
                         nullptr, nullptr) == failure,
           "Invoke fails");
}

}  // namespace

int main() {
    conversions();
    abs_and_sgn();
    strings_and_objects();
    variants();
    zombie();
    if (failures == 0) {
        std::printf("vbrun regcall: all checks passed\n");
        return 0;
    }
    return 1;
}
