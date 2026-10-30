// Registration of the VBRUN compatibility library library with Windows: the
// entry points regsvr32 calls. The work is in wfc/ocx/registration.hpp.
// Internal to the WFC libraries.
//
// The class table lists the controls of the library: WFC's own ProgIDs and
// class ids, and the Microsoft ones used when WFC_REGISTER_MICROSOFT_IDS=1.
// The library does not create the controls itself yet (DllGetClassObject
// reports that the class is not available); the registration records which
// library and which version serves them.

#include "registration_info.hpp"
#include "wfc_registration.hpp"

namespace {

const wfc::registration::Library kLibrary{L"vbrun", L"" WFC_LIB_VERSION,
                                          L"" WFC_LIB_TARGET};

// This library registers no classes.

HMODULE this_module() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&this_module), &module);
    return module;
}

}  // namespace

extern "C" {

HRESULT __stdcall DllRegisterServer() {
    return wfc::registration::register_library(this_module(), kLibrary, nullptr,
                                               0U);
}

HRESULT __stdcall DllUnregisterServer() {
    return wfc::registration::unregister_library(kLibrary, nullptr, 0U,
                                                 this_module());
}

HRESULT __stdcall DllCanUnloadNow() {
    return S_OK;
}

HRESULT __stdcall DllGetClassObject(REFCLSID, REFIID, void** object) {
    if (object != nullptr) {
        *object = nullptr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

}  // extern "C"
