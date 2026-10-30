/* Black-box check of a WFC library DLL, written in C so it uses the DLL the way
 * any other program does. This file is identical in every WFC library
 * (wfc-vbrun, wfc-mscomctl, wfc-comdlg32, wfc-mswinsck); a WFC test fails when
 * the copies differ. Windows only; internal to the WFC libraries.
 *
 * Usage of the program built from it: <test> <dll> <version> <target>
 *
 * It loads the DLL, looks for the four COM entry points, compares the version
 * resource with the version and target the library was built from, and runs a
 * registration round trip in the registry of the current user (DllRegisterServer,
 * then DllUnregisterServer), once under WFC's own ProgIDs and once more under
 * the Microsoft ones. Exit code 77 means the registry already holds the library
 * or the Microsoft class ids, so the test left it alone.
 */
#ifndef WFC_DLL_CHECK_H
#define WFC_DLL_CHECK_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct wfc_check_class {
    const wchar_t* progid;     /* WFC's own ProgID */
    const wchar_t* clsid;      /* WFC's own class id */
    const wchar_t* ms_progid;  /* the Microsoft ProgID, or NULL */
    const wchar_t* ms_clsid;   /* the Microsoft class id, or NULL */
};

typedef HRESULT(__stdcall* wfc_check_entry)(void);

static wfc_check_entry wfc_check_unregister;
static const wchar_t* wfc_check_library;

static void wfc_check_fail(const char* what, const wchar_t* detail) {
    fprintf(stderr, "FAIL: %s %ls\n", what, detail != NULL ? detail : L"");
    if (wfc_check_unregister != NULL) {
        wfc_check_unregister();
    }
    exit(1);
}

static wchar_t* wfc_check_widen(const char* text) {
    const int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t* wide = (wchar_t*)malloc((size_t)count * sizeof(wchar_t));
    if (wide == NULL) {
        exit(1);
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, count);
    return wide;
}

/* The default value (name NULL) or a named value of an HKCU key. */
static int wfc_check_read(const wchar_t* path, const wchar_t* name,
                          wchar_t* text, DWORD capacity) {
    HKEY key = NULL;
    DWORD size = (capacity - 1U) * (DWORD)sizeof(wchar_t);
    DWORD type = 0;
    LONG status;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return 0;
    }
    status = RegQueryValueExW(key, name, NULL, &type, (BYTE*)text, &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return 0;
    }
    text[size / sizeof(wchar_t)] = L'\0';
    return 1;
}

static int wfc_check_exists(const wchar_t* path) {
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return 0;
    }
    RegCloseKey(key);
    return 1;
}

static void wfc_check_path(wchar_t* out, const wchar_t* format,
                           const wchar_t* first, const wchar_t* second) {
    swprintf(out, 512, format, first, second != NULL ? second : L"");
}

static void wfc_check_expect(const wchar_t* path, const wchar_t* name,
                             const wchar_t* expected) {
    wchar_t text[512];
    if (!wfc_check_read(path, name, text, 512U)) {
        wfc_check_fail("missing registry value", path);
    }
    if (wcscmp(text, expected) != 0) {
        wfc_check_fail("wrong registry value at", path);
    }
}

/* "major.minor.patch" into three numbers. */
static int wfc_check_parse_version(const wchar_t* text, unsigned* major,
                                   unsigned* minor, unsigned* patch) {
    wchar_t* end = NULL;
    *major = (unsigned)wcstoul(text, &end, 10);
    if (*end != L'.') {
        return 0;
    }
    *minor = (unsigned)wcstoul(end + 1, &end, 10);
    if (*end != L'.') {
        return 0;
    }
    *patch = (unsigned)wcstoul(end + 1, &end, 10);
    return *end == L'\0';
}

/* The version resource names the version and target the library was built
 * from. */
static void wfc_check_version(const wchar_t* dll, const wchar_t* version,
                              const wchar_t* target) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(dll, &handle);
    void* data;
    VS_FIXEDFILEINFO* fixed = NULL;
    wchar_t* comments = NULL;
    UINT length = 0;
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    wchar_t expected[512];
    if (size == 0U) {
        wfc_check_fail("no version resource in", dll);
    }
    data = malloc(size);
    if (data == NULL || !GetFileVersionInfoW(dll, 0, size, data)) {
        wfc_check_fail("cannot read the version resource of", dll);
    }
    if (!VerQueryValueW(data, L"\\", (void**)&fixed, &length) || fixed == NULL) {
        wfc_check_fail("no fixed version in", dll);
    }
    if (!wfc_check_parse_version(version, &major, &minor, &patch) ||
        HIWORD(fixed->dwFileVersionMS) != major ||
        LOWORD(fixed->dwFileVersionMS) != minor ||
        HIWORD(fixed->dwFileVersionLS) != patch) {
        wfc_check_fail("the file version is not", version);
    }
    if (!VerQueryValueW(data, L"\\StringFileInfo\\040904B0\\Comments",
                        (void**)&comments, &length) ||
        comments == NULL) {
        wfc_check_fail("no Comments string in", dll);
    }
    swprintf(expected, 512, L"Targets %ls", target);
    if (wcscmp(comments, expected) != 0) {
        wfc_check_fail("the Comments string is not", expected);
    }
    free(data);
}

static void wfc_check_exports(HMODULE module, wfc_check_entry* reg,
                              wfc_check_entry* unreg) {
    wfc_check_entry can_unload =
        (wfc_check_entry)(void*)GetProcAddress(module, "DllCanUnloadNow");
    HRESULT(__stdcall * get_class)
    (const GUID*, const GUID*, void**) =
        (HRESULT(__stdcall*)(const GUID*, const GUID*, void**))(void*)
            GetProcAddress(module, "DllGetClassObject");
    const GUID none = {0U, 0U, 0U, {0, 0, 0, 0, 0, 0, 0, 0}};
    void* object = (void*)&none;
    *reg = (wfc_check_entry)(void*)GetProcAddress(module, "DllRegisterServer");
    *unreg =
        (wfc_check_entry)(void*)GetProcAddress(module, "DllUnregisterServer");
    if (*reg == NULL || *unreg == NULL || can_unload == NULL ||
        get_class == NULL) {
        wfc_check_fail("a COM entry point is missing in", wfc_check_library);
    }
    if (can_unload() != S_OK) {
        wfc_check_fail("DllCanUnloadNow is not S_OK in", wfc_check_library);
    }
    /* The controls are implemented by the WFC runtime for now. */
    if (get_class(&none, &none, &object) != CLASS_E_CLASSNOTAVAILABLE ||
        object != NULL) {
        wfc_check_fail("DllGetClassObject answered in", wfc_check_library);
    }
}

static void wfc_check_registered(const wchar_t* dll_name, const wchar_t* version,
                                 const wchar_t* target,
                                 const struct wfc_check_class* classes,
                                 size_t count, int microsoft) {
    wchar_t library[512];
    wchar_t path[512];
    wchar_t server[512];
    wchar_t tail[512];
    size_t i;
    wfc_check_path(library, L"Software\\WFC\\Libraries\\%ls%ls",
                   wfc_check_library, NULL);
    wfc_check_expect(library, NULL, version);
    wfc_check_expect(library, L"Target", target);
    wfc_check_path(path, L"%ls\\Server%ls", library, NULL);
    if (!wfc_check_read(path, NULL, server, 512U)) {
        wfc_check_fail("no Server value at", path);
    }
    swprintf(tail, 512, L"\\%ls", dll_name);
    if (wcslen(server) < wcslen(tail) ||
        _wcsicmp(server + wcslen(server) - wcslen(tail), tail) != 0) {
        wfc_check_fail("the server is not the library:", server);
    }
    for (i = 0U; i < count; ++i) {
        const wchar_t* progid = microsoft ? classes[i].ms_progid
                                          : classes[i].progid;
        const wchar_t* clsid = microsoft ? classes[i].ms_clsid : classes[i].clsid;
        if (progid == NULL || clsid == NULL) {
            continue;
        }
        wfc_check_path(path, L"Software\\Classes\\%ls\\CLSID%ls", progid, NULL);
        wfc_check_expect(path, NULL, clsid);
        wfc_check_path(path, L"Software\\Classes\\CLSID\\%ls\\InprocServer32%ls",
                       clsid, NULL);
        wfc_check_expect(path, NULL, server);
    }
}

static void wfc_check_removed(const struct wfc_check_class* classes, size_t count,
                              int microsoft) {
    wchar_t path[512];
    size_t i;
    wfc_check_path(path, L"Software\\WFC\\Libraries\\%ls%ls",
                   wfc_check_library, NULL);
    if (wfc_check_exists(path)) {
        wfc_check_fail("the unregistration left", path);
    }
    for (i = 0U; i < count; ++i) {
        const wchar_t* progid = microsoft ? classes[i].ms_progid
                                          : classes[i].progid;
        const wchar_t* clsid = microsoft ? classes[i].ms_clsid : classes[i].clsid;
        if (progid == NULL || clsid == NULL) {
            continue;
        }
        wfc_check_path(path, L"Software\\Classes\\%ls%ls", progid, NULL);
        if (wfc_check_exists(path)) {
            wfc_check_fail("the unregistration left", path);
        }
        wfc_check_path(path, L"Software\\Classes\\CLSID\\%ls%ls", clsid, NULL);
        if (wfc_check_exists(path)) {
            wfc_check_fail("the unregistration left", path);
        }
    }
}

static int wfc_check_microsoft_free(const struct wfc_check_class* classes,
                                    size_t count) {
    wchar_t path[512];
    size_t i;
    for (i = 0U; i < count; ++i) {
        if (classes[i].ms_clsid == NULL) {
            continue;
        }
        wfc_check_path(path, L"Software\\Classes\\CLSID\\%ls%ls",
                       classes[i].ms_clsid, NULL);
        if (wfc_check_exists(path)) {
            return 0;
        }
    }
    return 1;
}

/* `library` is the short name (mscomctl); argv holds the DLL, version, target. */
static int wfc_check_dll(int argc, char** argv, const wchar_t* library,
                         const struct wfc_check_class* classes, size_t count) {
    wchar_t* dll;
    wchar_t* version;
    wchar_t* target;
    wchar_t dll_name[MAX_PATH];
    wchar_t key[512];
    wchar_t* slash;
    HMODULE module;
    wfc_check_entry reg = NULL;
    wfc_check_entry unreg = NULL;
    if (argc != 4) {
        fprintf(stderr, "usage: %s <dll> <version> <target>\n", argv[0]);
        return 2;
    }
    wfc_check_library = library;
    /* A missing dependency is an exit code, not a dialog. */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    dll = wfc_check_widen(argv[1]);
    version = wfc_check_widen(argv[2]);
    target = wfc_check_widen(argv[3]);
    module = LoadLibraryW(dll);
    if (module == NULL) {
        wfc_check_fail("cannot load", dll);
    }
    wfc_check_exports(module, &reg, &unreg);
    wfc_check_version(dll, version, target);

    slash = wcsrchr(dll, L'\\');
    if (slash == NULL) {
        slash = wcsrchr(dll, L'/');
    }
    lstrcpynW(dll_name, slash != NULL ? slash + 1 : dll, MAX_PATH);

    wfc_check_path(key, L"Software\\WFC\\Libraries\\%ls%ls", library, NULL);
    if (wfc_check_exists(key)) {
        printf("%ls is already registered for this user: skipped\n", library);
        return 77;
    }
    SetEnvironmentVariableW(L"WFC_REGISTER_SCOPE", L"user");

    SetEnvironmentVariableW(L"WFC_REGISTER_MICROSOFT_IDS", L"0");
    wfc_check_unregister = unreg;
    if (reg() != S_OK) {
        wfc_check_fail("DllRegisterServer failed for", dll);
    }
    wfc_check_registered(dll_name, version, target, classes, count, 0);
    if (unreg() != S_OK) {
        wfc_check_fail("DllUnregisterServer failed for", dll);
    }
    wfc_check_unregister = NULL;
    wfc_check_removed(classes, count, 0);

    if (wfc_check_microsoft_free(classes, count)) {
        SetEnvironmentVariableW(L"WFC_REGISTER_MICROSOFT_IDS", L"1");
        wfc_check_unregister = unreg;
        if (reg() != S_OK) {
            wfc_check_fail("DllRegisterServer (Microsoft ids) failed for", dll);
        }
        wfc_check_registered(dll_name, version, target, classes, count, 1);
        if (unreg() != S_OK) {
            wfc_check_fail("DllUnregisterServer (Microsoft ids) failed for",
                           dll);
        }
        wfc_check_unregister = NULL;
        wfc_check_removed(classes, count, 1);
    } else {
        printf("a Microsoft %ls class id is in HKCU: that part is skipped\n",
               library);
    }
    printf("%ls: exports, version %ls and registration are right\n", library,
           version);
    free(dll);
    free(version);
    free(target);
    return 0;
}

#endif /* WFC_DLL_CHECK_H */
