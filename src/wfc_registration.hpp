// Registers a WFC library with Windows (DllRegisterServer and
// DllUnregisterServer). This file is identical in every WFC library (wfc-vbrun,
// wfc-mscomctl, wfc-comdlg32, wfc-mswinsck); a WFC test fails when the copies
// differ. Windows only; internal to the WFC libraries.
//
// What a registration writes:
//   Software\WFC\Libraries\<name>   Version, Target, Path of the library
//   <progid> and CLSID\<clsid>      for every class of the library, under
//                                   WFC's own ProgIDs (WFC.<Library>.<Class>)
//                                   and CLSIDs, with the DLL as the in-process
//                                   server
// With WFC_REGISTER_MICROSOFT_IDS=1 the classes are registered under the
// Microsoft ProgIDs and CLSIDs as well (so a program that asks for
// MSComctlLib.ListView gets this library); the entry that was there before is
// saved and restored by the unregistration.
//
// WFC_REGISTER_SCOPE selects the registry: `machine` (HKEY_LOCAL_MACHINE),
// `user` (HKEY_CURRENT_USER), or unset: the machine, and the user when the
// machine is not writable.

#ifndef WFC_REGISTRATION_HPP
#define WFC_REGISTRATION_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <olectl.h>

#include <cstddef>
#include <cwchar>
#include <string>

namespace wfc::registration {

struct Library {
    const wchar_t* name;     // "mscomctl"
    const wchar_t* version;  // "0.6.1"
    const wchar_t* target;   // "MSCOMCTL.OCX 6.1.98"
};

struct ComClass {
    const wchar_t* name;        // "ListView"
    const wchar_t* wfc_clsid;   // WFC's own class id
    const wchar_t* wfc_progid;  // "WFC.MSComctlLib.ListView"
    const wchar_t* ms_clsid;    // the Microsoft class id, or nullptr
    const wchar_t* ms_progid;   // "MSComctlLib.ListView.2", or nullptr
};

namespace detail {

inline std::wstring environment(const wchar_t* name) {
    wchar_t buffer[64]{};
    const DWORD length = GetEnvironmentVariableW(name, buffer, 64);
    return length > 0 && length < 64 ? std::wstring(buffer, length)
                                     : std::wstring{};
}

inline bool set_text(HKEY key, const wchar_t* name, const std::wstring& text) {
    return RegSetValueExW(key, name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(text.c_str()),
                          static_cast<DWORD>((text.size() + 1U) *
                                             sizeof(wchar_t))) == ERROR_SUCCESS;
}

inline bool get_text(HKEY root, const std::wstring& path, const wchar_t* name,
                     std::wstring& text) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buffer[1024]{};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    const bool found = RegQueryValueExW(key, name, nullptr, &type,
                                        reinterpret_cast<BYTE*>(buffer),
                                        &size) == ERROR_SUCCESS &&
                       type == REG_SZ;
    RegCloseKey(key);
    if (found) {
        text.assign(buffer);
    }
    return found;
}

// Creates `path` below `root` and sets its default value (when `text` is not
// null) and optional named value.
inline bool write_key(HKEY root, const std::wstring& path,
                      const std::wstring* text,
                      const wchar_t* extra_name = nullptr,
                      const std::wstring* extra_text = nullptr) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = true;
    if (text != nullptr) {
        ok = set_text(key, nullptr, *text);
    }
    if (ok && extra_name != nullptr && extra_text != nullptr) {
        ok = set_text(key, extra_name, *extra_text);
    }
    RegCloseKey(key);
    return ok;
}

inline void remove_tree(HKEY root, const std::wstring& path) {
    RegDeleteTreeW(root, path.c_str());
}

inline std::wstring module_path(HMODULE module) {
    wchar_t buffer[MAX_PATH * 2]{};
    const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH * 2U);
    return std::wstring(buffer, length);
}

// Writes or removes the entries of one class below `classes` ("<hive>\Software\
// Classes").
inline bool write_class(HKEY hive, const std::wstring& progid,
                        const std::wstring& clsid, const std::wstring& title,
                        const std::wstring& path) {
    const std::wstring base = L"Software\\Classes\\";
    const std::wstring class_key = base + L"CLSID\\" + clsid;
    const std::wstring apartment = L"Apartment";
    return write_key(hive, base + progid, &title) &&
           write_key(hive, base + progid + L"\\CLSID", &clsid) &&
           write_key(hive, class_key, &title) &&
           write_key(hive, class_key + L"\\ProgID", &progid) &&
           write_key(hive, class_key + L"\\InprocServer32", &path,
                     L"ThreadingModel", &apartment) &&
           write_key(hive, class_key + L"\\VersionIndependentProgID", &progid);
}

inline void remove_class(HKEY hive, const std::wstring& progid,
                         const std::wstring& clsid) {
    const std::wstring base = L"Software\\Classes\\";
    remove_tree(hive, base + progid);
    remove_tree(hive, base + L"CLSID\\" + clsid);
}

}  // namespace detail

// The registry the registration goes to: true for the machine.
inline bool choose_machine_scope() {
    const std::wstring scope = detail::environment(L"WFC_REGISTER_SCOPE");
    if (scope == L"user") {
        return false;
    }
    if (scope == L"machine") {
        return true;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\WFC", 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return false;
}

inline bool microsoft_ids() {
    return detail::environment(L"WFC_REGISTER_MICROSOFT_IDS") == L"1";
}

inline HRESULT register_library(HMODULE module, const Library& library,
                                const ComClass* classes, std::size_t count) {
    const HKEY hive =
        choose_machine_scope() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const std::wstring path = detail::module_path(module);
    const std::wstring name(library.name);
    const std::wstring key = L"Software\\WFC\\Libraries\\" + name;
    const std::wstring version(library.version);
    const std::wstring target(library.target);
    if (!detail::write_key(hive, key, &version, L"Target", &target) ||
        !detail::write_key(hive, key + L"\\Server", &path)) {
        return SELFREG_E_CLASS;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const ComClass& entry = classes[i];
        const std::wstring title = L"WFC " + std::wstring(entry.name);
        if (!detail::write_class(hive, entry.wfc_progid, entry.wfc_clsid, title,
                                 path)) {
            return SELFREG_E_CLASS;
        }
        if (!microsoft_ids() || entry.ms_clsid == nullptr) {
            continue;
        }
        // The Microsoft class id is served by this library: keep what served
        // it before (once) and put this library in its place.
        const std::wstring previous_key =
            key + L"\\Previous\\" + entry.ms_clsid;
        const std::wstring server_key =
            std::wstring(L"Software\\Classes\\CLSID\\") + entry.ms_clsid +
            L"\\InprocServer32";
        std::wstring old_server;
        std::wstring recorded;
        const bool had_server =
            detail::get_text(hive, server_key, nullptr, old_server);
        if (!had_server) {
            // The Microsoft control is not installed: register the class whole.
            if (!detail::get_text(hive, previous_key, nullptr, recorded)) {
                const std::wstring none = L"-";
                detail::write_key(hive, previous_key, &none);
            }
            if (!detail::write_class(hive, entry.ms_progid, entry.ms_clsid,
                                     title, path)) {
                return SELFREG_E_CLASS;
            }
        } else if (old_server != path) {
            std::wstring old_threading;
            detail::get_text(hive, server_key, L"ThreadingModel",
                             old_threading);
            if (!detail::get_text(hive, previous_key, nullptr, recorded)) {
                detail::write_key(hive, previous_key, &old_server,
                                  L"ThreadingModel", &old_threading);
            }
            const std::wstring apartment = L"Apartment";
            if (!detail::write_key(hive, server_key, &path, L"ThreadingModel",
                                   &apartment)) {
                return SELFREG_E_CLASS;
            }
        }
    }
    return S_OK;
}

inline HRESULT unregister_library(const Library& library,
                                  const ComClass* classes, std::size_t count,
                                  HMODULE module) {
    const HKEY hive =
        choose_machine_scope() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    const std::wstring path = detail::module_path(module);
    const std::wstring key =
        L"Software\\WFC\\Libraries\\" + std::wstring(library.name);
    for (std::size_t i = 0; i < count; ++i) {
        const ComClass& entry = classes[i];
        detail::remove_class(hive, entry.wfc_progid, entry.wfc_clsid);
        if (entry.ms_clsid == nullptr) {
            continue;
        }
        // The Microsoft class is ours only while it names this library.
        std::wstring server;
        const std::wstring server_key =
            std::wstring(L"Software\\Classes\\CLSID\\") + entry.ms_clsid +
            L"\\InprocServer32";
        if (!detail::get_text(hive, server_key, nullptr, server) ||
            server != path) {
            continue;
        }
        const std::wstring previous_key =
            key + L"\\Previous\\" + entry.ms_clsid;
        std::wstring old_server;
        if (!detail::get_text(hive, previous_key, nullptr, old_server)) {
            continue;
        }
        if (old_server == L"-") {
            detail::remove_class(hive, entry.ms_progid, entry.ms_clsid);
            continue;
        }
        // Put the server that was there before back.
        std::wstring old_threading;
        detail::get_text(hive, previous_key, L"ThreadingModel", old_threading);
        detail::write_key(hive, server_key, &old_server, L"ThreadingModel",
                          &old_threading);
    }
    detail::remove_tree(hive, key);
    // The parents go with the last library (the calls fail while they hold
    // anything else).
    RegDeleteKeyW(hive, L"Software\\WFC\\Libraries");
    RegDeleteKeyW(hive, L"Software\\WFC");
    return S_OK;
}

}  // namespace wfc::registration

#endif  // WFC_REGISTRATION_HPP
