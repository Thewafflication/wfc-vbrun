// CreateObject and GetObject.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdio>

#include "../src/vbrun_helpers.hpp"

extern "C" {
VARIANT* WINAPI rtcCreateObject2(VARIANT* result, BSTR class_name,
                                 BSTR server);
VARIANT* WINAPI rtcGetObject(VARIANT* result, const VARIANT* path,
                             const VARIANT* class_name);
IUnknown* WINAPI __vbaNew(const void* info);
VARIANT* WINAPI rtcCreateObject(VARIANT* result, BSTR class_name);
void WINAPI __vbaNew2(const void* info, void** object);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    CoInitialize(nullptr);
    VARIANT result;
    VariantInit(&result);

    {
        struct Info {
            DWORD flags;
            const CLSID* clsid;
            const IID* iid;
        };
        const CLSID std_font = {0x0BE35203,
                                0x8F91,
                                0x11CE,
                                {0x9D, 0xE3, 0x00, 0xAA, 0x00, 0x4B, 0xB8,
                                 0x51}};
        const Info font = {0, &std_font, nullptr};
        IUnknown* made = __vbaNew(&font);
        expect(made != nullptr, "New of a class");
        if (made != nullptr) {
            made->Release();
        }
        void* stored = nullptr;
        __vbaNew2(&font, &stored);
        expect(stored != nullptr, "New of a class into a variable");
        if (stored != nullptr) {
            static_cast<IUnknown*>(stored)->Release();
        }
        const CLSID nothing = {};
        const Info missing = {0, &nothing, nullptr};
        vbrun::clear_error();
        stored = &stored;
        __vbaNew2(&missing, &stored);
        expect(stored == nullptr && vbrun::last_error() == 429,
               "New of a missing class is error 429");
        vbrun::clear_error();
    }
    {
        VARIANT old;
        old.vt = VT_I4;
        old.lVal = 1;
        expect(rtcCreateObject(&old, nullptr) == &old && old.vt == VT_EMPTY,
               "the older CreateObject gives an empty Variant");
    }
    BSTR none = SysAllocString(L"No.Such.Class.Wfc");
    rtcCreateObject2(&result, none, nullptr);
    expect(result.vt == VT_EMPTY && vbrun::last_error() == 429,
           "unknown class is error 429");
    vbrun::clear_error();
    BSTR empty = SysAllocString(L"");
    rtcCreateObject2(&result, empty, nullptr);
    expect(vbrun::last_error() == 429, "empty class name is error 429");
    vbrun::clear_error();

    CLSID id = {};
    if (SUCCEEDED(CLSIDFromProgID(L"Scripting.Dictionary", &id))) {
        BSTR name = SysAllocString(L"Scripting.Dictionary");
        rtcCreateObject2(&result, name, nullptr);
        expect(result.vt == VT_DISPATCH && result.pdispVal != nullptr &&
                   vbrun::last_error() == 0,
               "a Dictionary");
        VariantClear(&result);
        SysFreeString(name);
    }

    VARIANT absent;
    VariantInit(&absent);
    absent.vt = VT_ERROR;
    absent.scode = DISP_E_PARAMNOTFOUND;
    rtcGetObject(&result, &absent, &absent);
    expect(vbrun::last_error() == 429, "nothing to get is error 429");
    vbrun::clear_error();
    VARIANT file;
    VariantInit(&file);
    file.vt = VT_BSTR;
    file.bstrVal = SysAllocString(L"C:\\no\\such\\wfc\\file.nothing");
    rtcGetObject(&result, &file, &absent);
    expect(result.vt == VT_EMPTY && vbrun::last_error() != 0,
           "a file that is not there");
    vbrun::clear_error();
    VariantClear(&file);

    SysFreeString(none);
    SysFreeString(empty);
    CoUninitialize();
    if (failures == 0) {
        std::puts("objects ok");
    }
    return failures == 0 ? 0 : 1;
}
