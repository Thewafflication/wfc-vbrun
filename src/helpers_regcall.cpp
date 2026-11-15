// Helpers the compiler calls with their first arguments in registers, and the
// Zombie interface (REQ-0100 to REQ-0104).
//
// The Visual Basic compiler calls the small helpers of the reference (the
// Integer and Byte conversions, Abs and Sgn, the free, copy and move of
// strings, objects and Variants) with the first operands in ECX and EDX. On
// x86 they are `__fastcall` here, which is that convention. On every other
// architecture only the argument list and the results matter, so they are the
// ordinary functions. Written from the disassembly of the installed MSVBVM60
// 6.00.9848.

#include "vbrun_helpers.hpp"

#include <cstdint>
#include <cstring>

#include "vbrun_regcall.hpp"

namespace {

constexpr VARTYPE kByRef = 0x4000;
constexpr VARTYPE kVariantByRef = VT_VARIANT | kByRef;

// An Integer, Long or Byte conversion that does not fit is an overflow
// (error 6).
SHORT overflow_short() {
    static_cast<void>(vbrun::raise(6));
    return 0;
}

}  // namespace

extern "C" {

// CInt(Long).
SHORT VBRUN_FASTCALL __vbaI2I4(const LONG value) {
    if (static_cast<LONG>(static_cast<SHORT>(value)) != value) {
        return overflow_short();
    }
    return static_cast<SHORT>(value);
}

// CByte(Integer): 0 to 255, compared as an unsigned number.
BYTE VBRUN_FASTCALL __vbaUI1I2(const SHORT value) {
    if (static_cast<std::uint16_t>(value) > 0xFFU) {
        static_cast<void>(overflow_short());
        return 0;
    }
    return static_cast<BYTE>(value);
}

// CByte(Long).
BYTE VBRUN_FASTCALL __vbaUI1I4(const LONG value) {
    if (static_cast<std::uint32_t>(value) > 0xFFU) {
        static_cast<void>(overflow_short());
        return 0;
    }
    return static_cast<BYTE>(value);
}

// Abs(Integer): -32768 is an overflow.
SHORT VBRUN_FASTCALL __vbaI2Abs(const SHORT value) {
    if (value >= 0) {
        return value;
    }
    const auto negated = static_cast<SHORT>(-static_cast<LONG>(value));
    if (negated < 0) {
        return overflow_short();
    }
    return negated;
}

// Abs(Long): -2147483648 is an overflow.
LONG VBRUN_FASTCALL __vbaI4Abs(const LONG value) {
    if (value >= 0) {
        return value;
    }
    if (value == INT32_MIN) {
        static_cast<void>(vbrun::raise(6));
        return value;
    }
    return -value;
}

// Sgn of an Integer, a Long and a Byte: -1, 0 or 1.
LONG VBRUN_FASTCALL __vbaI2Sgn(const SHORT value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

LONG VBRUN_FASTCALL __vbaI4Sgn(const LONG value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

LONG VBRUN_FASTCALL __vbaUI1Sgn(const BYTE value) {
    return value != 0 ? 1 : 0;
}

// A string slot: the string is copied (a new allocation of the same bytes),
// moved (the slot takes the caller's string) or freed. A copy that cannot be
// allocated is out of memory (error 7) and leaves the slot as it was.
BSTR VBRUN_FASTCALL __vbaStrCopy(BSTR* const slot, const BSTR source) {
    BSTR copy = nullptr;
    if (source != nullptr) {
        copy = SysAllocStringByteLen(reinterpret_cast<const char*>(source),
                                     SysStringByteLen(source));
        if (copy == nullptr) {
            static_cast<void>(vbrun::raise(7));
            return nullptr;
        }
    }
    if (*slot != nullptr) {
        SysFreeString(*slot);
    }
    *slot = copy;
    return copy;
}

BSTR VBRUN_FASTCALL __vbaStrMove(BSTR* const slot, const BSTR source) {
    if (*slot != nullptr) {
        SysFreeString(*slot);
    }
    *slot = source;
    return source;
}

void VBRUN_FASTCALL __vbaFreeStr(BSTR* const slot) {
    if (*slot != nullptr) {
        SysFreeString(*slot);
        *slot = nullptr;
    }
}

// An object slot: the reference is released and the slot cleared.
void VBRUN_FASTCALL __vbaFreeObj(IUnknown** const slot) {
    if (*slot != nullptr) {
        (*slot)->Release();
        *slot = nullptr;
    }
}

// A Variant slot: what the value owns is freed and the Variant becomes Empty.
// The types below String own nothing, so they are left as they are; a
// reference to another Variant is only forgotten.
void VBRUN_FASTCALL __vbaFreeVar(VARIANT* const variant) {
    const VARTYPE type = V_VT(variant);
    if (type < VT_BSTR) {
        return;
    }
    if ((type & kByRef) == 0) {
        static_cast<void>(VariantClear(variant));
    }
    std::uint32_t head = 0;
    std::memcpy(variant, &head, sizeof(head));
}

// A Variant assigned a copy of another (the old value is freed first); a
// Variant made as a copy into a slot that holds nothing yet.
VARIANT* VBRUN_FASTCALL __vbaVarCopy(VARIANT* const destination,
                                     const VARIANT* const source) {
    if (destination == source) {
        return destination;
    }
    __vbaFreeVar(destination);
    const HRESULT result = VariantCopy(destination, source);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
    }
    return destination;
}

VARIANT* VBRUN_FASTCALL __vbaVarDup(VARIANT* const destination,
                                    const VARIANT* const source) {
    VariantInit(destination);
    const HRESULT result = VariantCopy(destination, source);
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
    }
    return destination;
}

// The Variant moved into the destination (the old value is freed); the source
// becomes Empty.
VARIANT* VBRUN_FASTCALL __vbaVarZero(VARIANT* const destination,
                                     VARIANT* const source) {
    if (V_VT(destination) >= VT_BSTR) {
        __vbaFreeVar(destination);
    }
    std::memcpy(destination, source, sizeof(VARIANT));
    V_VT(source) = VT_EMPTY;
    return destination;
}

// A Variant argument passed by reference: the Variant a reference to a
// Variant points at, else the argument itself.
VARIANT* VBRUN_FASTCALL __vbaVargParmRef(VARIANT* const argument) {
    if (V_VT(argument) == kVariantByRef) {
        return V_VARIANTREF(argument);
    }
    return argument;
}

// A Variant made for a by-reference argument: a copy of a reference as it is,
// otherwise a reference to the value. The unused second parameter is EDX; the
// result slot is the first stack argument.
VARIANT* VBRUN_FASTCALL __vbaVargVar(VARIANT* const source,
                                     const std::uintptr_t /*edx*/,
                                     VARIANT* const result) {
    VARIANT value;
    std::memcpy(&value, source, sizeof(VARIANT));
    if ((V_VT(source) & kByRef) == 0) {
        V_VT(&value) = kVariantByRef;
        V_VARIANTREF(&value) = source;
    }
    std::memcpy(result, &value, sizeof(VARIANT));
    return result;
}

VARIANT* VBRUN_FASTCALL __vbaVargVarRef(VARIANT* const destination,
                                        VARIANT* const source) {
    std::memcpy(destination, source, sizeof(VARIANT));
    if ((V_VT(destination) & kByRef) == 0) {
        V_VARIANTREF(destination) = source;
        V_VT(destination) = kVariantByRef;
    }
    return destination;
}

// The Zombie object is what a destroyed instance leaves behind for the
// references still held: its reference counting goes to the object inside (at
// offset 8) and every other call fails.
struct Zombie {
    void* table;
    void* reserved;
    IUnknown* inner;
};

ULONG WINAPI Zombie_AddRef(Zombie* const self) {
    return self->inner->AddRef();
}

ULONG WINAPI Zombie_Release(Zombie* const self) {
    return self->inner->Release();
}

HRESULT WINAPI Zombie_QueryInterface(Zombie* const self,
                                     const IID* const iid, void** const out) {
    if (out == nullptr) {
        return E_INVALIDARG;
    }
    *out = nullptr;
    return self->inner->QueryInterface(*iid, out);
}

constexpr HRESULT kZombieFailure = static_cast<HRESULT>(0x80010007U);

HRESULT WINAPI Zombie_GetTypeInfoCount(Zombie*, UINT*) {
    return kZombieFailure;
}

HRESULT WINAPI Zombie_GetTypeInfo(Zombie*, UINT, LCID, ITypeInfo**) {
    return kZombieFailure;
}

HRESULT WINAPI Zombie_GetIDsOfNames(Zombie*, const IID*, OLECHAR**, UINT,
                                    LCID, DISPID*) {
    return kZombieFailure;
}

HRESULT WINAPI Zombie_Invoke(Zombie*, DISPID, const IID*, LCID, WORD,
                             DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) {
    return kZombieFailure;
}

// BASIC_CLASS is the interface a Visual Basic class instance shows outside:
// its AddRef and Release are the Zombie's (the same code in the reference), and
// QueryInterface asks the object inside without clearing the result first.
ULONG WINAPI BASIC_CLASS_AddRef(Zombie* const self) {
    return self->inner->AddRef();
}

ULONG WINAPI BASIC_CLASS_Release(Zombie* const self) {
    return self->inner->Release();
}

HRESULT WINAPI BASIC_CLASS_QueryInterface(Zombie* const self,
                                          const IID* const iid,
                                          void** const out) {
    return self->inner->QueryInterface(*iid, out);
}

// A dispinterface has one piece of type information.
HRESULT WINAPI BASIC_DISPINTERFACE_GetTICount(void* /*self*/,
                                              UINT* const count) {
    *count = 1;
    return S_OK;
}

// A Variant moved into the destination (the old value is freed). An object
// that is the source gives its default value instead, and is released.
VARIANT* VBRUN_FASTCALL __vbaVarMove(VARIANT* const destination,
                                     VARIANT* const source) {
    if (V_VT(destination) >= VT_BSTR) {
        __vbaFreeVar(destination);
    }
    if (V_VT(source) == VT_DISPATCH && V_DISPATCH(source) != nullptr) {
        IDispatch* const object = V_DISPATCH(source);
        std::uint32_t head = 0;
        std::memcpy(destination, &head, sizeof(head));
        DISPPARAMS none = {nullptr, nullptr, 0, 0};
        const HRESULT result = object->Invoke(
            DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD | DISPATCH_PROPERTYGET,
            &none, destination, nullptr, nullptr);
        if (FAILED(result)) {
            static_cast<void>(vbrun::raise_hresult(result));
        } else if (V_VT(destination) == VT_DISPATCH) {
            static_cast<void>(vbrun::raise(13));
        }
        object->Release();
        std::memcpy(source, &head, sizeof(head));
        return destination;
    }
    std::memcpy(destination, source, sizeof(VARIANT));
    std::uint32_t head = 0;
    std::memcpy(source, &head, sizeof(head));
    return destination;
}

// The Variant of a by-reference argument freed through its reference: the
// string, object or array it points at is released and the pointer cleared; a
// plain Variant is freed as it is.
void VBRUN_FASTCALL __vbaFreeVarg(VARIANT* const variant) {
    const VARTYPE type = V_VT(variant);
    if (type < VT_BSTR) {
        std::uint32_t head = 0;
        std::memcpy(variant, &head, sizeof(head));
    }
    if ((type & kByRef) == 0) {
        __vbaFreeVar(variant);
        return;
    }
    const VARTYPE base = type & static_cast<VARTYPE>(~kByRef);
    if (base == VT_BSTR) {
        BSTR* const slot = V_BSTRREF(variant);
        if (*slot != nullptr) {
            SysFreeString(*slot);
            *slot = nullptr;
        }
    } else if (base == VT_DISPATCH || base == VT_UNKNOWN) {
        IUnknown** const slot = V_UNKNOWNREF(variant);
        if (*slot != nullptr) {
            (*slot)->Release();
            *slot = nullptr;
        }
    } else if ((base & VT_ARRAY) != 0) {
        SAFEARRAY** const slot = V_ARRAYREF(variant);
        if (*slot != nullptr) {
            if ((*slot)->cLocks != 0) {
                static_cast<void>(vbrun::raise(10));
            }
            const HRESULT result = SafeArrayDestroy(*slot);
            if (FAILED(result)) {
                static_cast<void>(vbrun::raise_hresult(result));
            }
            *slot = nullptr;
        }
    }
}

// A by-reference argument as the Variant it stands for, without freeing
// anything: a plain Variant is returned as it is, a reference to a Variant
// gives the Variant behind it, and any other reference is read into the
// destination (the value is copied, not duplicated).
VARIANT* VBRUN_FASTCALL __vbaVarVargNofree(VARIANT* const destination,
                                           VARIANT* const source) {
    const VARTYPE type = V_VT(source);
    if ((type & kByRef) == 0) {
        return source;
    }
    if (type == kVariantByRef) {
        return V_VARIANTREF(source);
    }
    const VARTYPE base = type & static_cast<VARTYPE>(~kByRef);
    V_VT(destination) = base;
    const void* const target = source->byref;
    switch (base) {
        case VT_UI1:
            std::memcpy(&destination->bVal, target, 1);
            break;
        case VT_I2:
        case VT_BOOL:
            std::memcpy(&destination->iVal, target, 2);
            break;
        case VT_I4:
        case VT_R4:
        case VT_ERROR:
        case VT_BSTR:
        case VT_DISPATCH:
        case VT_UNKNOWN:
            std::memcpy(&destination->lVal, target, 4);
            break;
        case VT_R8:
        case VT_CY:
        case VT_DATE:
            std::memcpy(&destination->dblVal, target, 8);
            break;
        case VT_DECIMAL:
            std::memcpy(destination, target, sizeof(DECIMAL));
            V_VT(destination) = VT_DECIMAL;
            break;
        default:
            static_cast<void>(vbrun::raise(5));
            break;
    }
    return destination;
}

}  // extern "C"

namespace {

// Stores a Variant into the target of a by-reference Variant (the worker of
// __vbaVargVarMove and __vbaVargVarCopy): an object source gives its default
// value, the value is converted to the type the reference points at and
// written there (the old string or object in the slot is released). When the
// target is not a reference the Variant is moved or copied as it is.
VARIANT* store_through_reference(VARIANT* const target,
                                 VARIANT* const source, const bool move) {
    if ((V_VT(target) & kByRef) == 0) {
        return move ? __vbaVarMove(target, source)
                    : __vbaVarCopy(target, source);
    }
    VARIANT defaulted;
    VariantInit(&defaulted);
    VARIANT* value = source;
    if (V_VT(source) == VT_DISPATCH && V_DISPATCH(source) != nullptr) {
        IDispatch* const object = V_DISPATCH(source);
        DISPPARAMS none = {nullptr, nullptr, 0, 0};
        const HRESULT result = object->Invoke(
            DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD | DISPATCH_PROPERTYGET,
            &none, &defaulted, nullptr, nullptr);
        if (FAILED(result)) {
            static_cast<void>(vbrun::raise_hresult(result));
            return target;
        }
        if (V_VT(&defaulted) == VT_DISPATCH) {
            VariantClear(&defaulted);
            static_cast<void>(vbrun::raise(13));
            return target;
        }
        if (move) {
            object->Release();
            V_VT(source) = VT_EMPTY;
        }
        value = &defaulted;
    }
    const VARTYPE base = V_VT(target) & static_cast<VARTYPE>(~kByRef);
    if (base == VT_VARIANT) {
        __vbaVarCopy(V_VARIANTREF(target), value);
    } else {
        VARIANT converted;
        VariantInit(&converted);
        const HRESULT result = VariantChangeType(&converted, value, 0, base);
        if (FAILED(result)) {
            static_cast<void>(vbrun::raise_hresult(result));
        } else {
            void* const slot = target->byref;
            switch (base) {
                case VT_UI1:
                    std::memcpy(slot, &converted.bVal, 1);
                    break;
                case VT_I2:
                case VT_BOOL:
                    std::memcpy(slot, &converted.iVal, 2);
                    break;
                case VT_I4:
                case VT_R4:
                case VT_ERROR:
                    std::memcpy(slot, &converted.lVal, 4);
                    break;
                case VT_R8:
                case VT_CY:
                case VT_DATE:
                    std::memcpy(slot, &converted.dblVal, 8);
                    break;
                case VT_BSTR: {
                    BSTR* const string = static_cast<BSTR*>(slot);
                    if (*string != nullptr) {
                        SysFreeString(*string);
                    }
                    *string = converted.bstrVal;
                    converted.bstrVal = nullptr;
                    break;
                }
                case VT_DISPATCH:
                case VT_UNKNOWN: {
                    IUnknown** const object = static_cast<IUnknown**>(slot);
                    if (*object != nullptr) {
                        (*object)->Release();
                    }
                    *object = converted.punkVal;
                    converted.punkVal = nullptr;
                    break;
                }
                case VT_DECIMAL:
                    std::memcpy(slot, &converted, sizeof(DECIMAL));
                    break;
                default:
                    static_cast<void>(vbrun::raise(5));
                    break;
            }
        }
        VariantClear(&converted);
    }
    if (value == &defaulted) {
        VariantClear(&defaulted);
    } else if (move) {
        __vbaFreeVar(source);
    }
    return target;
}

}  // namespace

extern "C" {

// A Variant stored through a by-reference Variant, the source then freed
// (Move) or left alone (Copy). EDX is the source, ECX the by-reference target.
VARIANT* VBRUN_FASTCALL __vbaVargVarMove(VARIANT* const target,
                                         VARIANT* const source) {
    return store_through_reference(target, source, true);
}

VARIANT* VBRUN_FASTCALL __vbaVargVarCopy(VARIANT* const target,
                                         VARIANT* const source) {
    return store_through_reference(target, source, false);
}

// A by-reference Variant array as a Variant holding a new array: a copy of the
// array with every element copied.
VARIANT* VBRUN_FASTCALL __vbaAryVarVarg(VARIANT* const result,
                                        SAFEARRAY* const source) {
    V_VT(result) = static_cast<VARTYPE>(VT_ARRAY | VT_VARIANT);
    SAFEARRAY* copy = nullptr;
    if (source->rgsabound[0].cElements == 0) {
        if (FAILED(SafeArrayAllocDescriptorEx(VT_VARIANT, 1, &copy))) {
            static_cast<void>(vbrun::raise(7));
            V_ARRAY(result) = nullptr;
            return result;
        }
        copy->cbElements = sizeof(VARIANT);
        copy->fFeatures = static_cast<USHORT>(source->fFeatures & ~0x12U);
        V_ARRAY(result) = copy;
        return result;
    }
    copy = SafeArrayCreate(VT_VARIANT, 1, source->rgsabound);
    if (copy == nullptr) {
        static_cast<void>(vbrun::raise(7));
        V_ARRAY(result) = nullptr;
        return result;
    }
    VARIANT* const from = static_cast<VARIANT*>(source->pvData);
    VARIANT* const to = static_cast<VARIANT*>(copy->pvData);
    for (ULONG index = 0; index < copy->rgsabound[0].cElements; ++index) {
        __vbaVarCopy(&to[index], &from[index]);
    }
    V_ARRAY(result) = copy;
    return result;
}

// An array descriptor taken over: the slot gets a new descriptor with the
// source's bounds and data (the source keeps its descriptor, loses its data,
// and is no longer marked embedded).
SAFEARRAY** WINAPI __vbaLdZeroAry(SAFEARRAY** const slot,
                                  SAFEARRAY* const source) {
    if (*slot != nullptr) {
        SafeArrayDestroyDescriptor(*slot);
        *slot = nullptr;
    }
    HRESULT result = S_OK;
    if ((source->fFeatures & FADF_HAVEVARTYPE) != 0) {
        VARTYPE type = VT_EMPTY;
        SafeArrayGetVartype(source, &type);
        result = SafeArrayAllocDescriptorEx(type, source->cDims, slot);
    } else {
        result = SafeArrayAllocDescriptor(source->cDims, slot);
    }
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
        return slot;
    }
    SAFEARRAY* const made = *slot;
    made->cbElements = source->cbElements;
    made->fFeatures = static_cast<USHORT>(source->fFeatures & ~0x12U);
    made->cLocks = 0;
    std::memcpy(made->rgsabound, source->rgsabound,
                sizeof(SAFEARRAYBOUND) * source->cDims);
    source->fFeatures = static_cast<USHORT>(source->fFeatures & ~0x20U);
    made->pvData = source->pvData;
    source->pvData = nullptr;
    return slot;
}

}  // extern "C"

// The stack probe: EAX is the number of bytes to allocate; every page of them
// is touched in turn so the guard page moves down, the return frame is moved
// below the new space, and the space is zeroed.
#if defined(_MSC_VER) && defined(_M_IX86)

extern "C" __declspec(naked) void __vbaChkstk() {
    __asm {
        push ecx
        push edi
        push eax
        cmp eax, 0x1000
        lea ecx, [esp + 0x10]
        jae large_frame
    small_frame:
        sub ecx, eax
        mov eax, esp
        test dword ptr [ecx], eax
        lea esp, [ecx - 0x10]
        mov ecx, dword ptr [eax]
        mov dword ptr [esp], ecx
        mov ecx, dword ptr [eax + 4]
        mov dword ptr [esp + 4], ecx
        mov ecx, dword ptr [eax + 8]
        mov dword ptr [esp + 8], ecx
        mov ecx, dword ptr [eax + 0xc]
        mov dword ptr [esp + 0xc], ecx
        pop ecx
        lea edi, [esp + 0xc]
        shr ecx, 2
        mov eax, 0
        rep stosd
        pop edi
        pop ecx
        ret
    large_frame:
        sub ecx, 0x1000
        sub eax, 0x1000
        test dword ptr [ecx], eax
        cmp eax, 0x1000
        jb small_frame
        jmp large_frame
    }
}

#else

// Without a stack of the 32-bit x86 kind to probe there is nothing to do.
extern "C" void __vbaChkstk() {}

#endif
