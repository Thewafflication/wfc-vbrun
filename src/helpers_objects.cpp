// CreateObject and GetObject: an object from a class name (ProgID) or from a
// file or moniker name.
//
// Written from the disassembly of MSVBVM60. A failure to make the object is
// error 429 (ActiveX component cannot create object); a file that does not
// load is 432.

#include <objbase.h>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kCannotCreate = 429;
constexpr int kFileNotFound = 432;

// The object as a Variant: IDispatch when it has one, IUnknown otherwise.
VARIANT* give(VARIANT* const result, IUnknown* const object) {
    VariantInit(result);
    IDispatch* dispatch = nullptr;
    if (SUCCEEDED(object->QueryInterface(
            IID_IDispatch, reinterpret_cast<void**>(&dispatch))) &&
        dispatch != nullptr) {
        object->Release();
        result->vt = VT_DISPATCH;
        result->pdispVal = dispatch;
    } else {
        result->vt = VT_UNKNOWN;
        result->punkVal = object;
    }
    return result;
}

bool missing(const VARIANT* const value) {
    return value == nullptr ||
           (value->vt == VT_ERROR &&
            value->scode == static_cast<SCODE>(DISP_E_PARAMNOTFOUND));
}

// The text of an optional Variant argument (null when it has none).
BSTR text_of(const VARIANT* const value) {
    VARIANT converted;
    VariantInit(&converted);
    if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(value), 0,
                                 VT_BSTR))) {
        return nullptr;
    }
    return converted.bstrVal;
}

IUnknown* bind_name(const BSTR name) {
    IBindCtx* context = nullptr;
    if (FAILED(CreateBindCtx(0, &context))) {
        return nullptr;
    }
    IMoniker* moniker = nullptr;
    ULONG eaten = 0;
    const HRESULT parsed = MkParseDisplayName(context, name, &eaten, &moniker);
    context->Release();
    if (FAILED(parsed) || moniker == nullptr) {
        return nullptr;
    }
    IUnknown* object = nullptr;
    const HRESULT bound = BindMoniker(moniker, 0, IID_IUnknown,
                                      reinterpret_cast<void**>(&object));
    moniker->Release();
    return SUCCEEDED(bound) ? object : nullptr;
}


// What `New` of a class is given: a flags word, then the class and the
// interface to ask for (null means IUnknown).
struct ClassInfo {
    DWORD flags;
    const CLSID* clsid;
    const IID* iid;
};

// The new object of a class; failure is error 429.
IUnknown* create_from(const ClassInfo* const info) {
    if (info == nullptr || info->clsid == nullptr) {
        static_cast<void>(vbrun::raise(kCannotCreate));
        return nullptr;
    }
    IUnknown* object = nullptr;
    const HRESULT made = CoCreateInstance(
        *info->clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        info->iid == nullptr ? IID_IUnknown : *info->iid,
        reinterpret_cast<void**>(&object));
    if (FAILED(made) || object == nullptr) {
        static_cast<void>(vbrun::raise(kCannotCreate));
        return nullptr;
    }
    return object;
}

}  // namespace

extern "C" {

VARIANT* WINAPI rtcCreateObject2(VARIANT* const result, const BSTR class_name,
                                 const BSTR server) {
    VariantInit(result);
    if (class_name == nullptr || SysStringLen(class_name) == 0) {
        static_cast<void>(vbrun::raise(kCannotCreate));
        return result;
    }
    CLSID id = {};
    IUnknown* object = nullptr;
    if (SUCCEEDED(CLSIDFromProgID(class_name, &id))) {
        if (server != nullptr && SysStringLen(server) != 0) {
            COSERVERINFO remote = {};
            remote.pwszName = server;
            MULTI_QI query = {&IID_IUnknown, nullptr, S_OK};
            if (SUCCEEDED(CoCreateInstanceEx(id, nullptr, CLSCTX_REMOTE_SERVER,
                                             &remote, 1, &query)) &&
                SUCCEEDED(query.hr)) {
                object = query.pItf;
            }
        } else {
            CoCreateInstance(id, nullptr,
                             CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                             IID_IUnknown, reinterpret_cast<void**>(&object));
        }
    } else {
        object = bind_name(class_name);
    }
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kCannotCreate));
        return result;
    }
    return give(result, object);
}

// GetObject(path, class): the running object of a class, the object of a
// file or moniker, or a class loaded from a file.
VARIANT* WINAPI rtcGetObject(VARIANT* const result, const VARIANT* const path,
                             const VARIANT* const class_name) {
    VariantInit(result);
    IUnknown* object = nullptr;
    if (missing(class_name)) {
        if (missing(path)) {
            static_cast<void>(vbrun::raise(kCannotCreate));
            return result;
        }
        BSTR name = text_of(path);
        object = name == nullptr ? nullptr : bind_name(name);
        SysFreeString(name);
    } else {
        BSTR class_text = text_of(class_name);
        CLSID id = {};
        if (class_text == nullptr ||
            FAILED(CLSIDFromProgID(class_text, &id))) {
            SysFreeString(class_text);
            static_cast<void>(vbrun::raise(kCannotCreate));
            return result;
        }
        SysFreeString(class_text);
        BSTR name = missing(path) ? nullptr : text_of(path);
        if (name == nullptr || SysStringLen(name) == 0) {
            GetActiveObject(id, nullptr, &object);
        } else if (SUCCEEDED(CoCreateInstance(
                       id, nullptr,
                       CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                       IID_IUnknown, reinterpret_cast<void**>(&object)))) {
            IPersistFile* file = nullptr;
            if (SUCCEEDED(object->QueryInterface(
                    IID_IPersistFile, reinterpret_cast<void**>(&file))) &&
                file != nullptr) {
                const HRESULT loaded = file->Load(name, STGM_READ);
                file->Release();
                if (FAILED(loaded)) {
                    object->Release();
                    SysFreeString(name);
                    static_cast<void>(vbrun::raise(kFileNotFound));
                    return result;
                }
            }
        }
        SysFreeString(name);
    }
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kCannotCreate));
        return result;
    }
    return give(result, object);
}

// New of a class: the object, or error 429 when it cannot be made.
IUnknown* WINAPI __vbaNew(const void* const info) {
    return create_from(static_cast<const ClassInfo*>(info));
}

// The same, stored in *object (null when it fails).
void WINAPI __vbaNew2(const void* const info, void** const object) {
    *object = create_from(static_cast<const ClassInfo*>(info));
}

// The older form of CreateObject: it makes nothing and hands back an empty
// Variant (the program uses CreateObject2).
VARIANT* WINAPI rtcCreateObject(VARIANT* const result, const BSTR) {
    VariantInit(result);
    return result;
}

}  // extern "C"
