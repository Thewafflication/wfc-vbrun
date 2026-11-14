// The Err object: the members through the interface as compiled code calls
// them (values returned directly) and through late binding.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <cwchar>

#include "../src/vbrun_helpers.hpp"

extern "C" {
IDispatch* WINAPI rtcErrObj();
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// The interface as the Visual Basic type library lays it out.
struct IErr : IDispatch {
    virtual LONG STDMETHODCALLTYPE get_Number() = 0;
    virtual void STDMETHODCALLTYPE put_Number(LONG number) = 0;
    virtual BSTR STDMETHODCALLTYPE get_Source() = 0;
    virtual void STDMETHODCALLTYPE put_Source(BSTR text) = 0;
    virtual BSTR STDMETHODCALLTYPE get_Description() = 0;
    virtual void STDMETHODCALLTYPE put_Description(BSTR text) = 0;
    virtual BSTR STDMETHODCALLTYPE get_HelpFile() = 0;
    virtual void STDMETHODCALLTYPE put_HelpFile(BSTR text) = 0;
    virtual LONG STDMETHODCALLTYPE get_HelpContext() = 0;
    virtual void STDMETHODCALLTYPE put_HelpContext(LONG context) = 0;
    virtual void STDMETHODCALLTYPE Raise(LONG number, const VARIANT* source,
                                         const VARIANT* description,
                                         const VARIANT* help_file,
                                         const VARIANT* help_context) = 0;
    virtual void STDMETHODCALLTYPE Clear() = 0;
    virtual LONG STDMETHODCALLTYPE get_LastDllError() = 0;
};

VARIANT absent() {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_ERROR;
    value.scode = DISP_E_PARAMNOTFOUND;
    return value;
}

VARIANT text(const wchar_t* const characters) {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = SysAllocString(characters);
    return value;
}

bool same(BSTR got, const wchar_t* const wanted) {
    const bool equal = got != nullptr && std::wcscmp(got, wanted) == 0;
    SysFreeString(got);
    return equal;
}

DISPID id_of(IDispatch* const object, const wchar_t* const name) {
    DISPID id = DISPID_UNKNOWN;
    LPOLESTR names[1] = {const_cast<LPOLESTR>(name)};
    object->GetIDsOfNames(IID_NULL, names, 1, LOCALE_USER_DEFAULT, &id);
    return id;
}

}  // namespace

int main() {
    IDispatch* const object = rtcErrObj();
    expect(object != nullptr, "the Err object");
    IErr* const err = static_cast<IErr*>(object);
    vbrun::clear_error();

    // Number and the message that goes with it.
    expect(err->get_Number() == 0, "no error");
    err->put_Number(11);
    expect(err->get_Number() == 11, "Err.Number = 11");
    expect(same(err->get_Description(), L"Division by zero"),
           "the description of error 11");
    expect(same(err->get_Source(), L""), "no source");

    // Setting Description and Source.
    BSTR own = SysAllocString(L"my words");
    err->put_Description(own);
    SysFreeString(own);
    own = SysAllocString(L"MyApp");
    err->put_Source(own);
    SysFreeString(own);
    err->put_HelpContext(42);
    expect(same(err->get_Description(), L"my words"), "Err.Description");
    expect(same(err->get_Source(), L"MyApp"), "Err.Source");
    expect(err->get_HelpContext() == 42, "Err.HelpContext");

    // Raise with details.
    VARIANT source = text(L"Tester");
    VARIANT description = text(L"it broke");
    VARIANT none = absent();
    VARIANT context = {};
    VariantInit(&context);
    context.vt = VT_I4;
    context.lVal = 7;
    err->Raise(1234, &source, &description, &none, &context);
    expect(err->get_Number() == 1234 && vbrun::last_error() == 1234,
           "Err.Raise sets the number");
    expect(same(err->get_Description(), L"it broke"), "Raise description");
    expect(same(err->get_Source(), L"Tester"), "Raise source");
    expect(err->get_HelpContext() == 7, "Raise help context");
    expect(same(err->get_HelpFile(), L""), "Raise without a help file");

    // Raise of zero is error 5; a new error clears the details.
    err->Raise(0, &none, &none, &none, &none);
    expect(err->get_Number() == 5, "Err.Raise 0 is error 5");
    expect(same(err->get_Source(), L""), "a new error clears the source");

    // Clear.
    err->Clear();
    expect(err->get_Number() == 0 && vbrun::last_error() == 0,
           "Err.Clear");

    // LastDllError.
    vbrun::set_last_dll_error(87);
    expect(err->get_LastDllError() == 87, "Err.LastDllError");

    // The interfaces it answers to.
    IUnknown* unknown = nullptr;
    expect(object->QueryInterface(IID_IUnknown,
                                  reinterpret_cast<void**>(&unknown)) ==
                   S_OK &&
               unknown != nullptr,
           "QueryInterface for IUnknown");
    if (unknown != nullptr) {
        unknown->Release();
    }
    void* other = nullptr;
    expect(object->QueryInterface(IID_IStream, &other) == E_NOINTERFACE,
           "no other interface");

    // Late binding: Err.Number = 3, Err.Description, Err.Raise 9, , "d".
    const DISPID number = id_of(object, L"Number");
    const DISPID desc = id_of(object, L"description");
    const DISPID raise = id_of(object, L"Raise");
    expect(number == 0 && desc != DISPID_UNKNOWN && raise != DISPID_UNKNOWN,
           "names of members");
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = 3;
    DISPID put_id = DISPID_PROPERTYPUT;
    DISPPARAMS put = {&value, &put_id, 1, 1};
    expect(object->Invoke(number, IID_NULL, 0, DISPATCH_PROPERTYPUT, &put,
                          nullptr, nullptr, nullptr) == S_OK &&
               err->get_Number() == 3,
           "Err.Number = 3 by late binding");
    VARIANT got;
    VariantInit(&got);
    DISPPARAMS no_args = {nullptr, nullptr, 0, 0};
    expect(object->Invoke(number, IID_NULL, 0, DISPATCH_PROPERTYGET, &no_args,
                          &got, nullptr, nullptr) == S_OK &&
               got.vt == VT_I4 && got.lVal == 3,
           "Err.Number by late binding");
    VARIANT args[3];  // reverse order: Description, Source, Number
    args[0] = text(L"late");
    args[1] = none;
    VariantInit(&args[2]);
    args[2].vt = VT_I4;
    args[2].lVal = 9;
    DISPPARAMS raise_args = {args, nullptr, 3, 0};
    expect(object->Invoke(raise, IID_NULL, 0, DISPATCH_METHOD, &raise_args,
                          nullptr, nullptr, nullptr) == S_OK &&
               err->get_Number() == 9,
           "Err.Raise by late binding");
    expect(same(err->get_Description(), L"late"),
           "the description of a late Raise");
    VariantClear(&args[0]);
    VariantClear(&source);
    VariantClear(&description);

    err->Clear();
    object->Release();
    if (failures == 0) {
        std::puts("errobj ok");
    }
    return failures == 0 ? 0 : 1;
}
