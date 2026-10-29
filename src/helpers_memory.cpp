// REQ-0103: the member and memory helpers that move values between the
// fields and locals of a compiled VB6 program (GetMem*, PutMem*, SetMem*,
// VarPtr).
//
// Every helper returns an HRESULT (zero for success) and leaves a failure as
// a Visual Basic error of the thread (vbrun::raise), as the reference does.

#include <cstdint>
#include <cstring>

#include "vbrun_helpers.hpp"

extern "C" {

HRESULT WINAPI GetMem1(const std::uint8_t* from, std::uint8_t* to) {
    *to = *from;
    return S_OK;
}

HRESULT WINAPI GetMem2(const std::uint16_t* from, std::uint16_t* to) {
    *to = *from;
    return S_OK;
}

HRESULT WINAPI GetMem4(const std::uint32_t* from, std::uint32_t* to) {
    *to = *from;
    return S_OK;
}

HRESULT WINAPI GetMem8(const std::uint32_t* from, std::uint32_t* to) {
    const std::uint32_t low = from[0];
    const std::uint32_t high = from[1];
    to[0] = low;
    to[1] = high;
    return S_OK;
}

HRESULT WINAPI PutMem1(std::uint8_t* to, const std::uint8_t value) {
    *to = value;
    return S_OK;
}

HRESULT WINAPI PutMem2(std::uint16_t* to, const std::uint16_t value) {
    *to = value;
    return S_OK;
}

HRESULT WINAPI PutMem4(std::uint32_t* to, const std::uint32_t value) {
    *to = value;
    return S_OK;
}

// An eight-byte value is passed as two doublewords, the low one first.
HRESULT WINAPI PutMem8(std::uint32_t* to, const std::uint32_t low,
                       const std::uint32_t high) {
    to[0] = low;
    to[1] = high;
    return S_OK;
}

// VarPtr(x): the address of the argument, which is passed by reference.
LONG_PTR WINAPI VarPtr(void* address) {
    return reinterpret_cast<LONG_PTR>(address);
}

// A String is copied by its length in bytes (an odd byte count survives).
HRESULT WINAPI GetMemStr(BSTR* from, BSTR* to) {
    const BSTR source = *from;
    if (source == nullptr) {
        *to = nullptr;
        return S_OK;
    }
    const BSTR copy = SysAllocStringByteLen(
        reinterpret_cast<const char*>(source), SysStringByteLen(source));
    *to = copy;
    if (copy == nullptr) {
        static_cast<void>(vbrun::raise(7));
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

// An empty source gives the target an allocated empty String (not a null
// pointer), as the reference does.
HRESULT WINAPI PutMemStr(BSTR* to, const BSTR value) {
    const BSTR copy =
        value != nullptr
            ? SysAllocStringByteLen(reinterpret_cast<const char*>(value),
                                    SysStringByteLen(value))
            : SysAllocStringByteLen(nullptr, 0);
    if (copy == nullptr) {
        static_cast<void>(vbrun::raise(7));
        return E_OUTOFMEMORY;
    }
    if (*to != nullptr) {
        SysFreeString(*to);
    }
    *to = copy;
    return S_OK;
}

// An object reference is copied and counted.
HRESULT WINAPI GetMemObj(IUnknown** from, IUnknown** to) {
    IUnknown* const object = *from;
    *to = object;
    if (object != nullptr) {
        object->AddRef();
    }
    return S_OK;
}

HRESULT WINAPI SetMemObj(IUnknown** to, IUnknown* value) {
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

// A Variant is copied (VariantCopy); a failure is a Visual Basic error.
HRESULT WINAPI GetMemVar(VARIANT* from, VARIANT* to) {
    const HRESULT result = VariantCopy(to, from);
    if (FAILED(result)) {
        return vbrun::raise_hresult(result);
    }
    return S_OK;
}

}  // extern "C"
