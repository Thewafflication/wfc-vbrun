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

}  // extern "C"
