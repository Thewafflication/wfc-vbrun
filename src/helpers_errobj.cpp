// The Err object: rtcErrObj hands out the one object (per process; its state
// is per thread) whose members are Number, Source, Description, HelpFile,
// HelpContext, LastDllError, Raise and Clear.
//
// The layout is that of the ErrObject interface of the Visual Basic type
// library (read from MSVBVM60): the members after IDispatch are in the order
// Number, Source, Description, HelpFile and HelpContext (each a getter then a
// setter), Raise, Clear and LastDllError. They return their value directly
// (not an HRESULT), as the Visual Basic compiler calls them.

#include <cwchar>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

// {A4C466B8-499F-101B-BB78-00AA00383CBB}
const IID kIidErrObject = {0xA4C466B8,
                           0x499F,
                           0x101B,
                           {0xBB, 0x78, 0x00, 0xAA, 0x00, 0x38, 0x3C, 0xBB}};

constexpr DISPID kNumber = 0;
constexpr DISPID kSource = 0x60030002;
constexpr DISPID kDescription = 0x60030004;
constexpr DISPID kHelpFile = 0x60030006;
constexpr DISPID kHelpContext = 0x60030008;
constexpr DISPID kRaise = 0x6003000A;
constexpr DISPID kClear = 0x6003000B;
constexpr DISPID kLastDllError = 0x6003000C;

struct NamedMember {
    const wchar_t* name;
    DISPID id;
};

const NamedMember kMembers[] = {
    {L"number", kNumber},         {L"source", kSource},
    {L"description", kDescription}, {L"helpfile", kHelpFile},
    {L"helpcontext", kHelpContext}, {L"raise", kRaise},
    {L"clear", kClear},           {L"lastdllerror", kLastDllError},
};

class ErrObject : public IDispatch {
public:
    // IUnknown. The object lives for the whole process.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,
                                             void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IDispatch) ||
            IsEqualIID(id, kIidErrObject)) {
            *object = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&references_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG left = InterlockedDecrement(&references_);
        return left < 1 ? 1U : static_cast<ULONG>(left);
    }

    // IDispatch: late binding by name.
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {
        *count = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* names,
                                            UINT count, LCID,
                                            DISPID* ids) override {
        HRESULT result = S_OK;
        for (UINT at = 0; at < count; ++at) {
            ids[at] = DISPID_UNKNOWN;
            if (at != 0) {
                result = DISP_E_UNKNOWNNAME;
                continue;
            }
            bool found = false;
            for (const NamedMember& member : kMembers) {
                if (_wcsicmp(names[at], member.name) == 0) {
                    ids[at] = member.id;
                    found = true;
                }
            }
            if (!found) {
                result = DISP_E_UNKNOWNNAME;
            }
        }
        return result;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* params, VARIANT* result,
                                     EXCEPINFO*, UINT*) override {
        const bool put = (flags & (DISPATCH_PROPERTYPUT |
                                   DISPATCH_PROPERTYPUTREF)) != 0;
        if (result != nullptr) {
            VariantInit(result);
        }
        if (id == kRaise) {
            return invoke_raise(params);
        }
        if (id == kClear) {
            vbrun::clear_error();
            return S_OK;
        }
        if (put) {
            if (params == nullptr || params->cArgs < 1) {
                return DISP_E_BADPARAMCOUNT;
            }
            return set_member(id, params->rgvarg[0]);
        }
        return get_member(id, result);
    }

    // The members, in the order of the interface after IDispatch.
    virtual LONG STDMETHODCALLTYPE get_Number() {
        return vbrun::last_error();
    }
    virtual void STDMETHODCALLTYPE put_Number(const LONG number) {
        const ErrorDetail kept = vbrun::error_detail();
        static_cast<void>(vbrun::raise(static_cast<int>(number)));
        vbrun::error_detail() = kept;
    }
    virtual BSTR STDMETHODCALLTYPE get_Source() {
        return SysAllocString(vbrun::error_detail().source.c_str());
    }
    virtual void STDMETHODCALLTYPE put_Source(const BSTR text) {
        vbrun::error_detail().source = text == nullptr ? L"" : text;
    }
    virtual BSTR STDMETHODCALLTYPE get_Description() {
        const std::wstring& own = vbrun::error_detail().description;
        if (!own.empty()) {
            return SysAllocString(own.c_str());
        }
        const wchar_t* const message =
            vbrun::error_message(vbrun::last_error());
        return SysAllocString(message == nullptr ? L"" : message);
    }
    virtual void STDMETHODCALLTYPE put_Description(const BSTR text) {
        vbrun::error_detail().description = text == nullptr ? L"" : text;
    }
    virtual BSTR STDMETHODCALLTYPE get_HelpFile() {
        return SysAllocString(vbrun::error_detail().help_file.c_str());
    }
    virtual void STDMETHODCALLTYPE put_HelpFile(const BSTR text) {
        vbrun::error_detail().help_file = text == nullptr ? L"" : text;
    }
    virtual LONG STDMETHODCALLTYPE get_HelpContext() {
        return vbrun::error_detail().help_context;
    }
    virtual void STDMETHODCALLTYPE put_HelpContext(const LONG context) {
        vbrun::error_detail().help_context = context;
    }
    // Raise(Number, [Source], [Description], [HelpFile], [HelpContext]): the
    // error is raised with those details; a Number of zero is error 5.
    virtual void STDMETHODCALLTYPE Raise(const LONG number,
                                         const VARIANT* const source,
                                         const VARIANT* const description,
                                         const VARIANT* const help_file,
                                         const VARIANT* const help_context) {
        if (number == 0) {
            static_cast<void>(vbrun::raise(5));
            return;
        }
        static_cast<void>(vbrun::raise(static_cast<int>(number)));
        vbrun::ErrorDetail& detail = vbrun::error_detail();
        if (present(source)) {
            detail.source = text_of(source);
        }
        if (present(description)) {
            detail.description = text_of(description);
        }
        if (present(help_file)) {
            detail.help_file = text_of(help_file);
        }
        if (present(help_context)) {
            detail.help_context = number_of(help_context);
        }
    }
    virtual void STDMETHODCALLTYPE Clear() {
        vbrun::clear_error();
    }
    virtual LONG STDMETHODCALLTYPE get_LastDllError() {
        return static_cast<LONG>(vbrun::last_dll_error());
    }

private:
    using ErrorDetail = vbrun::ErrorDetail;

    static bool present(const VARIANT* const value) {
        return value != nullptr &&
               !(value->vt == VT_ERROR &&
                 value->scode == static_cast<SCODE>(DISP_E_PARAMNOTFOUND)) &&
               value->vt != VT_EMPTY;
    }

    static std::wstring text_of(const VARIANT* const value) {
        VARIANT converted;
        VariantInit(&converted);
        std::wstring text;
        if (SUCCEEDED(VariantChangeType(&converted,
                                        const_cast<VARIANT*>(value), 0,
                                        VT_BSTR))) {
            text.assign(converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                        SysStringLen(converted.bstrVal));
        }
        VariantClear(&converted);
        return text;
    }

    static LONG number_of(const VARIANT* const value) {
        VARIANT converted;
        VariantInit(&converted);
        LONG number = 0;
        if (SUCCEEDED(VariantChangeType(&converted,
                                        const_cast<VARIANT*>(value), 0,
                                        VT_I4))) {
            number = converted.lVal;
        }
        return number;
    }

    HRESULT invoke_raise(DISPPARAMS* const params) {
        const UINT count = params == nullptr ? 0 : params->cArgs;
        if (count < 1 || count > 5) {
            return DISP_E_BADPARAMCOUNT;
        }
        // The arguments are in reverse order.
        VARIANT absent;
        VariantInit(&absent);
        absent.vt = VT_ERROR;
        absent.scode = DISP_E_PARAMNOTFOUND;
        const VARIANT* given[5] = {&absent, &absent, &absent, &absent,
                                   &absent};
        for (UINT at = 0; at < count; ++at) {
            given[at] = &params->rgvarg[count - 1 - at];
        }
        if (!present(given[0])) {
            return DISP_E_PARAMNOTFOUND;
        }
        Raise(number_of(given[0]), given[1], given[2], given[3], given[4]);
        return S_OK;
    }

    HRESULT get_member(const DISPID id, VARIANT* const result) {
        if (result == nullptr) {
            return S_OK;
        }
        switch (id) {
            case kNumber:
                result->vt = VT_I4;
                result->lVal = get_Number();
                return S_OK;
            case kSource:
                result->vt = VT_BSTR;
                result->bstrVal = get_Source();
                return S_OK;
            case kDescription:
                result->vt = VT_BSTR;
                result->bstrVal = get_Description();
                return S_OK;
            case kHelpFile:
                result->vt = VT_BSTR;
                result->bstrVal = get_HelpFile();
                return S_OK;
            case kHelpContext:
                result->vt = VT_I4;
                result->lVal = get_HelpContext();
                return S_OK;
            case kLastDllError:
                result->vt = VT_I4;
                result->lVal = get_LastDllError();
                return S_OK;
            default:
                return DISP_E_MEMBERNOTFOUND;
        }
    }

    void text_put(void (STDMETHODCALLTYPE ErrObject::*setter)(BSTR),
                  const VARIANT& value) {
        const BSTR text = SysAllocString(text_of(&value).c_str());
        (this->*setter)(text);
        SysFreeString(text);
    }

    HRESULT set_member(const DISPID id, const VARIANT& value) {
        switch (id) {
            case kNumber:
                put_Number(number_of(&value));
                return S_OK;
            case kSource:
                text_put(&ErrObject::put_Source, value);
                return S_OK;
            case kDescription:
                text_put(&ErrObject::put_Description, value);
                return S_OK;
            case kHelpFile:
                text_put(&ErrObject::put_HelpFile, value);
                return S_OK;
            case kHelpContext:
                put_HelpContext(number_of(&value));
                return S_OK;
            default:
                return DISP_E_MEMBERNOTFOUND;
        }
    }

    LONG references_ = 1;
};

ErrObject g_err_object;

}  // namespace

extern "C" {

// The Err object, with a reference added for the caller.
IDispatch* WINAPI rtcErrObj() {
    IDispatch* const object = &g_err_object;
    object->AddRef();
    return object;
}

}  // extern "C"
