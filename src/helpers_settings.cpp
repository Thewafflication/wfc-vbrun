// SaveSetting, GetSetting, GetAllSettings and DeleteSetting: the program
// settings kept in the registry of the user, below
// HKEY_CURRENT_USER\Software\VB and VBA Program Settings\<app>\<section>.
// Every setting is a string value named by its key.
//
// Written from the disassembly of MSVBVM60. An empty or missing name is
// error 5; so is any failure of the registry.

#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidCall = 5;
constexpr int kOutOfMemory = 7;
constexpr size_t kMaxPath = 0x104;
constexpr const wchar_t* kRoot = L"Software\\VB and VBA Program Settings\\";

std::wstring text_of(const BSTR text) {
    return text == nullptr ? std::wstring()
                           : std::wstring(text, SysStringLen(text));
}

bool named(const BSTR text) {
    return text != nullptr && SysStringLen(text) != 0;
}

// The key of an application, or of one of its sections.
bool key_path(const BSTR app, const BSTR section, std::wstring& path) {
    path = std::wstring(kRoot) + text_of(app);
    if (section != nullptr) {
        path += L"\\" + text_of(section);
    }
    if (path.size() + 1 >= kMaxPath) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return false;
    }
    return true;
}

// A missing optional Variant argument.
bool is_missing(const VARIANT& value) {
    return value.vt == VT_ERROR &&
           value.scode == static_cast<SCODE>(DISP_E_PARAMNOTFOUND);
}

// The name an optional Variant argument holds; false when it is not usable.
bool name_of(const VARIANT& value, BSTR& name) {
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status = VariantChangeType(&converted, &value, 0, VT_BSTR);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    name = converted.bstrVal;
    if (!named(name)) {
        SysFreeString(name);
        name = nullptr;
        static_cast<void>(vbrun::raise(kInvalidCall));
        return false;
    }
    return true;
}

}  // namespace

extern "C" {

void WINAPI rtcSaveSetting(const BSTR app, const BSTR section, const BSTR key,
                           const BSTR value) {
    if (!named(app) || !named(section) || !named(key)) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return;
    }
    std::wstring path;
    if (!key_path(app, section, path)) {
        return;
    }
    HKEY handle = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &handle,
                        nullptr) != ERROR_SUCCESS) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return;
    }
    const std::wstring data = text_of(value);
    const LONG status = RegSetValueExW(
        handle, text_of(key).c_str(), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(data.c_str()),
        static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
    const LONG closed = RegCloseKey(handle);
    if (status != ERROR_SUCCESS || closed != ERROR_SUCCESS) {
        static_cast<void>(vbrun::raise(kInvalidCall));
    }
}

BSTR WINAPI rtcGetSetting(const BSTR app, const BSTR section, const BSTR key,
                          VARIANT fallback) {
    if (!named(app) || !named(section) || !named(key)) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return nullptr;
    }
    std::wstring path;
    if (!key_path(app, section, path)) {
        return nullptr;
    }
    HKEY handle = nullptr;
    DWORD type = 0;
    DWORD bytes = 0;
    const std::wstring name = text_of(key);
    bool found =
        RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE,
                      &handle) == ERROR_SUCCESS;
    if (found) {
        found = RegQueryValueExW(handle, name.c_str(), nullptr, &type,
                                 nullptr, &bytes) == ERROR_SUCCESS;
    }
    if (!found) {
        if (handle != nullptr) {
            RegCloseKey(handle);
        }
        if (is_missing(fallback)) {
            return SysAllocString(L"");
        }
        VARIANT converted;
        VariantInit(&converted);
        const HRESULT status =
            VariantChangeType(&converted, &fallback, 0, VT_BSTR);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return nullptr;
        }
        return converted.bstrVal;
    }
    if (type != REG_SZ) {
        RegCloseKey(handle);
        static_cast<void>(vbrun::raise(kInvalidCall));
        return nullptr;
    }
    std::wstring data(bytes / sizeof(wchar_t) + 1, L'\0');
    DWORD size = static_cast<DWORD>(data.size() * sizeof(wchar_t));
    const LONG status = RegQueryValueExW(
        handle, name.c_str(), nullptr, &type,
        reinterpret_cast<BYTE*>(data.data()), &size);
    RegCloseKey(handle);
    if (status != ERROR_SUCCESS) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return nullptr;
    }
    const size_t length = size / sizeof(wchar_t);
    const size_t used = length > 0 && data[length - 1] == L'\0' ? length - 1
                                                                 : length;
    BSTR result = SysAllocStringLen(data.c_str(), static_cast<UINT>(used));
    if (result == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfMemory));
    }
    return result;
}

void WINAPI rtcDeleteSetting(const BSTR app, VARIANT section, VARIANT key) {
    if (!named(app)) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return;
    }
    const bool has_section = !is_missing(section);
    const bool has_key = !is_missing(key);
    if (has_key && !has_section) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return;
    }
    BSTR section_name = nullptr;
    BSTR key_name = nullptr;
    if ((has_section && !name_of(section, section_name)) ||
        (has_key && !name_of(key, key_name))) {
        SysFreeString(section_name);
        SysFreeString(key_name);
        return;
    }
    std::wstring path;
    if (key_path(app, section_name, path)) {
        if (has_key) {
            HKEY handle = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0,
                              KEY_SET_VALUE, &handle) != ERROR_SUCCESS) {
                static_cast<void>(vbrun::raise(kInvalidCall));
            } else {
                const LONG status =
                    RegDeleteValueW(handle, text_of(key_name).c_str());
                RegCloseKey(handle);
                if (status != ERROR_SUCCESS) {
                    static_cast<void>(vbrun::raise(kInvalidCall));
                }
            }
        } else if (RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()) !=
                   ERROR_SUCCESS) {
            static_cast<void>(vbrun::raise(kInvalidCall));
        } else {
            RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
        }
    }
    SysFreeString(section_name);
    SysFreeString(key_name);
}

// The keys and settings of a section: an array of strings with a row for
// every setting (the key, then the value). A section without settings gives
// an Empty Variant.
VARIANT* WINAPI rtcGetAllSettings(VARIANT* const result, const BSTR app,
                                  const BSTR section) {
    VariantClear(result);
    if (!named(app) || !named(section)) {
        static_cast<void>(vbrun::raise(kInvalidCall));
        return result;
    }
    std::wstring path;
    if (!key_path(app, section, path)) {
        return result;
    }
    HKEY handle = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ,
                      &handle) != ERROR_SUCCESS) {
        return result;
    }
    DWORD count = 0;
    DWORD longest_name = 0;
    DWORD longest_data = 0;
    if (RegQueryInfoKeyW(handle, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr, &count, &longest_name, &longest_data,
                         nullptr, nullptr) != ERROR_SUCCESS) {
        RegCloseKey(handle);
        static_cast<void>(vbrun::raise(kInvalidCall));
        return result;
    }
    if (count == 0) {
        RegCloseKey(handle);
        return result;
    }
    SAFEARRAYBOUND bounds[2] = {{count, 0}, {2, 0}};
    SAFEARRAY* const rows = SafeArrayCreate(VT_BSTR, 2, bounds);
    if (rows == nullptr) {
        RegCloseKey(handle);
        static_cast<void>(vbrun::raise(kOutOfMemory));
        return result;
    }
    bool failed = false;
    std::wstring name(longest_name + 1, L'\0');
    std::wstring data(longest_data / sizeof(wchar_t) + 2, L'\0');
    for (DWORD row = 0; row < count && !failed; ++row) {
        DWORD name_size = static_cast<DWORD>(name.size());
        DWORD data_size = static_cast<DWORD>(data.size() * sizeof(wchar_t));
        DWORD type = 0;
        const LONG status = RegEnumValueW(
            handle, row, name.data(), &name_size, nullptr, &type,
            reinterpret_cast<BYTE*>(data.data()), &data_size);
        if (status != ERROR_SUCCESS || type != REG_SZ) {
            failed = true;
            break;
        }
        const size_t length = data_size / sizeof(wchar_t);
        const size_t used =
            length > 0 && data[length - 1] == L'\0' ? length - 1 : length;
        BSTR key_text = SysAllocStringLen(name.c_str(), name_size);
        BSTR value_text = SysAllocStringLen(data.c_str(),
                                            static_cast<UINT>(used));
        LONG at[2] = {static_cast<LONG>(row), 0};
        failed = FAILED(SafeArrayPutElement(rows, at, key_text));
        at[1] = 1;
        failed = failed || FAILED(SafeArrayPutElement(rows, at, value_text));
        SysFreeString(key_text);
        SysFreeString(value_text);
    }
    RegCloseKey(handle);
    if (failed) {
        SafeArrayDestroy(rows);
        static_cast<void>(vbrun::raise(kInvalidCall));
        return result;
    }
    result->vt = VT_ARRAY | VT_BSTR;
    result->parray = rows;
    return result;
}

}  // extern "C"
