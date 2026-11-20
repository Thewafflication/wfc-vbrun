// REQ-0005: the engine entry points of the runtime: Declare'd calls
// (DllFunctionCall), End and Stop, Resume and the procedure exception
// handler, the program and ActiveX DLL entries (ThunRTMain, UserDllMain,
// VBDll*), the execution contexts and projects (Eb*), the type-information
// provider calls (Tip*) and the debugger and help hooks. Written from the
// disassembly of the installed MSVBVM60 6.00.9848 and from Visual Basic 6
// programs built with VB6.EXE /make (error numbers and texts of a failed
// Declare, the message of Stop, the layout of the project header).
//
// The project header the compiler emits ("VB5!"): Sub Main at +0x2c, the
// project data at +0x30, the COM registration data at +0x54, and the offsets
// (from the header) of the exe name (+0x58), the title (+0x5c), the help file
// (+0x60) and the project name (+0x64). Its addresses are 32-bit, as in the
// x86 programs that carry it.
//
// Not reproduced: the object engine (creating instances of the classes of a
// compiled project, its forms, the predeclared instances) and the p-code
// engine. The calls that need them answer CLASS_E_CLASSNOTAVAILABLE (or the
// documented failure of the call); ProcCallEngine and MethCallEngine, which
// a p-code procedure jumps into, stop the program with a diagnostic.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "vbrun_helpers.hpp"

extern "C" void __cdecl __vbaFileCloseAll();

namespace {

constexpr int kFileNotFound = 53;
constexpr int kPathNotFound = 76;
constexpr int kDllLoadError = 48;
constexpr int kOutOfMemory = 7;
constexpr int kBadOrdinal = 452;
constexpr int kNoEntryPoint = 453;
constexpr int kResumeWithoutError = 20;
constexpr int kOutOfStack = 28;
constexpr int kOverflow = 6;
constexpr int kDivisionByZero = 11;
constexpr unsigned kUnsupportedExitCode = 0x5642U;  // 'VB'
constexpr HRESULT kTypeElementNotFound = static_cast<HRESULT>(0x8002802BU);
constexpr HRESULT kBadIndex = static_cast<HRESULT>(0x8002000BU);
constexpr HRESULT kClassNotRegistered = static_cast<HRESULT>(0x80040201U);

// --- the project header --------------------------------------------------

std::uint32_t header_dword(const void* const header, const std::size_t at) {
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const char*>(header) + at, sizeof value);
    return value;
}

const char* header_address(const void* const header, const std::size_t at) {
    return reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(header_dword(header, at)));
}

bool valid_header(const void* const header) {
    return header != nullptr && std::memcmp(header, "VB5!", 4) == 0;
}

// A string of the header (its offset from the header at `at`).
std::string header_text(const void* const header, const std::size_t at) {
    if (!valid_header(header)) {
        return {};
    }
    return std::string(static_cast<const char*>(header) +
                       header_dword(header, at));
}

thread_local const void* g_project = nullptr;

std::wstring widen(const std::string& text) {
    const int length = MultiByteToWideChar(
        CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length > 0 ? length : 0), L'\0');
    if (length > 0) {
        MultiByteToWideChar(CP_ACP, 0, text.data(),
                            static_cast<int>(text.size()), wide.data(), length);
    }
    return wide;
}

// App.Title: the title of the running project, else the program's name.
std::wstring application_title() {
    std::string title = header_text(g_project, 0x5c);
    if (!title.empty()) {
        return widen(title);
    }
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring name(path);
    const std::size_t slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        name.erase(0, slash + 1);
    }
    const std::size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        name.erase(dot);
    }
    return name;
}

// The error of a Declare'd call with the text Visual Basic gives it.
void raise_with_text(const int number, const std::string& text) {
    static_cast<void>(vbrun::raise(number));
    vbrun::error_detail().description = widen(text);
}

[[noreturn]] void end_program() {
    __vbaFileCloseAll();
    ExitProcess(0);
}

[[noreturn]] void unsupported(const char* const what) {
    char message[200];
    const int length = std::snprintf(
        message, sizeof message,
        "wfcvbrun: %s is not supported (the program is stopped)\n", what);
    if (length > 0) {
        DWORD written = 0;
        const HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
        if (error != nullptr && error != INVALID_HANDLE_VALUE) {
            static_cast<void>(WriteFile(
                error, message, static_cast<DWORD>(length), &written, nullptr));
        }
        OutputDebugStringA(message);
    }
    ExitProcess(kUnsupportedExitCode);
}

// --- COM registration of an ActiveX DLL ------------------------------------
//
// The registration data (+0x54 of the header; every offset below is from its
// start): the first class (+0), the project name (+4), the help directory
// (+8), the description (+0xc), the type library's id (+0x10), its locale
// (+0x20) and version (+0x26, +0x28). A class: the next one (+0, 0 for none),
// its name (+4), description (+8), instancing (+0xc: 3 to 6 are creatable),
// its CLSID (+0x14).

struct ClassEntry {
    std::string name;
    CLSID clsid;
    std::uint32_t instancing;
};

struct Registration {
    std::string project;
    GUID library{};
    LCID locale = 0;
    WORD major = 0;
    WORD minor = 0;
    std::vector<ClassEntry> classes;
};

bool read_registration(const void* const header, Registration& out) {
    if (!valid_header(header)) {
        return false;
    }
    const char* const data = header_address(header, 0x54);
    if (data == nullptr) {
        return true;
    }
    out.project = data + header_dword(data, 4);
    std::memcpy(&out.library, data + 0x10, sizeof(GUID));
    out.locale = header_dword(data, 0x20);
    std::memcpy(&out.major, data + 0x26, sizeof(WORD));
    std::memcpy(&out.minor, data + 0x28, sizeof(WORD));
    std::uint32_t at = header_dword(data, 0);
    for (int guard = 0; at != 0 && guard < 4096; ++guard) {
        const char* const entry = data + at;
        ClassEntry item;
        item.name = data + header_dword(entry, 4);
        item.instancing = header_dword(entry, 0xc);
        std::memcpy(&item.clsid, entry + 0x14, sizeof(CLSID));
        out.classes.push_back(item);
        at = header_dword(entry, 0);
    }
    return true;
}

bool creatable(const ClassEntry& entry) {
    return entry.instancing >= 3 && entry.instancing <= 6;
}

std::wstring guid_text(const GUID& guid) {
    wchar_t text[40] = {};
    StringFromGUID2(guid, text, 40);
    return text;
}

bool set_value(const std::wstring& key, const wchar_t* const name,
               const std::wstring& value) {
    HKEY handle = nullptr;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, key.c_str(), 0, nullptr, 0,
                        KEY_WRITE, nullptr, &handle,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegSetValueExW(
        handle, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(handle);
    return result == ERROR_SUCCESS;
}

HRESULT register_classes(const Registration& data, const std::wstring& module) {
    wchar_t version[24] = {};
    std::swprintf(version, 24, L"%u.%u", static_cast<unsigned>(data.major),
                  static_cast<unsigned>(data.minor));
    for (const ClassEntry& entry : data.classes) {
        if (!creatable(entry)) {
            continue;
        }
        const std::wstring progid = widen(data.project + "." + entry.name);
        const std::wstring clsid = guid_text(entry.clsid);
        const std::wstring key = L"CLSID\\" + clsid;
        const bool done =
            set_value(progid, nullptr, progid) &&
            set_value(progid + L"\\Clsid", nullptr, clsid) &&
            set_value(key, nullptr, progid) &&
            set_value(key + L"\\InprocServer32", nullptr, module) &&
            set_value(key + L"\\InprocServer32", L"ThreadingModel",
                      L"Apartment") &&
            set_value(key + L"\\ProgID", nullptr, progid) &&
            set_value(key + L"\\VERSION", nullptr, version) &&
            set_value(key + L"\\TypeLib", nullptr, guid_text(data.library)) &&
            set_value(key + L"\\Programmable", nullptr, L"");
        if (!done) {
            return kClassNotRegistered;
        }
    }
    ITypeLib* library = nullptr;
    if (SUCCEEDED(LoadTypeLibEx(module.c_str(), REGKIND_REGISTER, &library))) {
        library->Release();
    }
    return S_OK;
}

HRESULT unregister_classes(const Registration& data) {
    for (const ClassEntry& entry : data.classes) {
        if (!creatable(entry)) {
            continue;
        }
        const std::wstring progid = widen(data.project + "." + entry.name);
        RegDeleteTreeW(HKEY_CLASSES_ROOT, progid.c_str());
        RegDeleteTreeW(HKEY_CLASSES_ROOT,
                       (L"CLSID\\" + guid_text(entry.clsid)).c_str());
    }
    static_cast<void>(UnRegisterTypeLib(data.library, data.major, data.minor,
                                        data.locale, SYS_WIN32));
    return S_OK;
}

std::wstring module_path(HMODULE* const module) {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        module != nullptr ? *module : nullptr, path, MAX_PATH);
    return std::wstring(path, length);
}

// --- execution contexts ------------------------------------------------------

struct Context {
    void* worker = nullptr;
    LONG objects = 0;
};

thread_local LONG g_contexts = 0;

}  // namespace

extern "C" {

// --- Declare
// ------------------------------------------------------------------

// The address of a Declare'd procedure, from its template: the library name
// (+0), the procedure name (+1 slot), the ordinal (UInt16 at +2 slots) and
// flags (+2 slots + 2; 2: by ordinal), and a cache (+3 slots) whose second
// slot keeps the library and third the address. A library that does not load
// is error 53 ("File not found: <library>"), 76 or 48; a missing procedure
// is 453 (452 for an ordinal), "Can't find DLL entry point <name> in
// <library>".
struct DllCache {
    void* reserved;
    HMODULE module;
    FARPROC address;
};

struct DllTemplate {
    const char* library;
    const char* name;
    WORD ordinal;
    WORD flags;
    DllCache* cache;
};

FARPROC WINAPI DllFunctionCall(const DllTemplate* const declared) {
    DllCache* const cache = declared->cache;
    const UINT mode =
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    if (reinterpret_cast<std::uintptr_t>(cache->module) < 0x20) {
        cache->module = LoadLibraryA(declared->library);
        if (reinterpret_cast<std::uintptr_t>(cache->module) < 0x20) {
            const DWORD code = GetLastError();
            SetErrorMode(mode);
            cache->module = nullptr;
            const std::string library = declared->library;
            if (code == 0 || code == ERROR_NOT_ENOUGH_MEMORY) {
                static_cast<void>(vbrun::raise(kOutOfMemory));
            } else if (code == ERROR_FILE_NOT_FOUND ||
                       code == ERROR_MOD_NOT_FOUND ||
                       code == ERROR_PROC_NOT_FOUND) {
                raise_with_text(kFileNotFound, "File not found: " + library);
            } else if (code == ERROR_PATH_NOT_FOUND) {
                raise_with_text(kPathNotFound, "Path not found: " + library);
            } else {
                static_cast<void>(vbrun::raise(kDllLoadError));
            }
            return nullptr;
        }
    }
    SetErrorMode(mode);
    const bool by_ordinal = (declared->flags & 2) != 0;
    const FARPROC address = GetProcAddress(
        cache->module,
        by_ordinal ? MAKEINTRESOURCEA(declared->ordinal) : declared->name);
    if (address == nullptr) {
        const std::string entry = by_ordinal ? std::to_string(declared->ordinal)
                                             : std::string(declared->name);
        raise_with_text(
            by_ordinal ? kBadOrdinal : kNoEntryPoint,
            "Can't find DLL entry point " + entry + " in " + declared->library);
        return nullptr;
    }
    cache->address = address;
    return address;
}

// --- End and Stop
// -------------------------------------------------------------

// End: the open files are closed and the program ends (exit code 0).
void __vbaEnd() {
    end_program();
}

// Stop in a compiled program: a message box "Stop statement encountered"
// with the program's title, then End.
void WINAPI __vbaStopExe(void* /*reserved*/) {
    MessageBoxW(nullptr, L"Stop statement encountered",
                application_title().c_str(), MB_OK | MB_ICONEXCLAMATION);
    end_program();
}

// --- the program and its project
// ----------------------------------------------

// The entry of a compiled program: the project header is checked, COM is
// started and Sub Main runs; the program then ends as End ends it. A program
// that starts with a form needs the forms engine, which is not reproduced.
void WINAPI ThunRTMain(const void* const header) {
    if (!valid_header(header)) {
        unsupported("a program without a Visual Basic project header");
    }
    g_project = header;
    static_cast<void>(OleInitialize(nullptr));
    const char* const sub_main = header_address(header, 0x2c);
    if (sub_main == nullptr) {
        unsupported("a program that starts with a form");
    }
    reinterpret_cast<void(WINAPI*)()>(const_cast<char*>(sub_main))();
    end_program();
}

// A p-code procedure enters the p-code engine, which is not reproduced.
void ProcCallEngine() {
    unsupported("p-code (ProcCallEngine)");
}

void MethCallEngine() {
    unsupported("p-code (MethCallEngine)");
}

// The DllMain of an ActiveX DLL: on attach the module handle is stored in
// the DLL's slot. Always TRUE.
BOOL WINAPI UserDllMain(HMODULE* const slot, void* /*reserved*/,
                        const HINSTANCE instance, const DWORD reason,
                        void* /*reserved2*/) {
    if (reason == DLL_PROCESS_ATTACH && slot != nullptr) {
        *slot = instance;
    }
    return TRUE;
}

// DllRegisterServer of an ActiveX DLL: each creatable class of the project
// gets its ProgID (Project.Class) and its CLSID key (InprocServer32 the
// DLL, apartment threaded, ProgID, VERSION, TypeLib, Programmable); the type
// library in the DLL is registered when there is one. A header that is not a
// project's is E_UNEXPECTED.
HRESULT WINAPI VBDllRegisterServer(HMODULE* const module, void* /*reserved*/,
                                   const void* const header) {
    Registration data;
    if (!read_registration(header, data)) {
        return E_UNEXPECTED;
    }
    const std::wstring path = module_path(module);
    if (path.empty()) {
        return E_FAIL;
    }
    return register_classes(data, path);
}

HRESULT WINAPI VBDllUnRegisterServer(HMODULE* /*module*/, void* /*reserved*/,
                                     const void* const header) {
    Registration data;
    if (!read_registration(header, data)) {
        return E_UNEXPECTED;
    }
    return unregister_classes(data);
}

// DllGetClassObject: a class of the project needs the object engine; every
// request is CLASS_E_CLASSNOTAVAILABLE (E_UNEXPECTED for a bad header).
HRESULT WINAPI VBDllGetClassObject(HMODULE* /*module*/, void* /*reserved*/,
                                   const void* const header,
                                   const CLSID* /*clsid*/, const IID* /*iid*/,
                                   void** const out) {
    if (out != nullptr) {
        *out = nullptr;
    }
    if (!valid_header(header)) {
        return E_UNEXPECTED;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

// DllCanUnloadNow: no object of the DLL can be alive.
HRESULT WINAPI VBDllCanUnloadNow(const void* /*header*/) {
    return S_OK;
}

// --- contexts and projects
// ----------------------------------------------------

HRESULT WINAPI EbCreateContext(void** const out) {
    Context* const context = new (std::nothrow) Context();
    if (context == nullptr) {
        return E_OUTOFMEMORY;
    }
    ++g_contexts;
    *out = context;
    return S_OK;
}

void WINAPI EbDestroyContext(void* const context) {
    if (context != nullptr) {
        --g_contexts;
        delete static_cast<Context*>(context);
    }
}

// The objects and the connections (contexts but the first) of the calling
// thread; a null pointer answers 5.
HRESULT WINAPI EbGetObjConnectionCounts(LONG* const connections,
                                        LONG* const objects) {
    if (connections == nullptr || objects == nullptr) {
        return 5;
    }
    *objects = 0;
    *connections = g_contexts - 1;
    return S_OK;
}

// The project running on the calling thread (its header), or null.
void WINAPI EbGetHandleOfExecutingProject(const void** const out) {
    *out = g_project;
}

BOOL WINAPI EbIsProjectOnStack(const void* const project) {
    return project != nullptr && project == g_project;
}

// Reset of a project: its error is cleared and its files closed; it no longer
// runs on this thread.
static HRESULT reset_project(const void* const project) {
    vbrun::clear_error();
    __vbaFileCloseAll();
    if (project == g_project) {
        g_project = nullptr;
    }
    return S_OK;
}

HRESULT WINAPI EbResetProject(const void* const project) {
    return reset_project(project);
}

HRESULT WINAPI EbResetProjectNormal(const void* const project) {
    return reset_project(project);
}

// The message of an error (+1 slot: a BSTR the caller frees) and its help
// context (+2 slots: 1,000,000 plus the number).
struct EbErrorInfo {
    void* reserved;
    BSTR description;
    LONG help_context;
};

HRESULT WINAPI EbGetErrorInfo(const LONG number, EbErrorInfo* const info) {
    const wchar_t* const message = vbrun::error_message(number);
    info->description = message != nullptr ? SysAllocString(message) : nullptr;
    if (message != nullptr && info->description == nullptr) {
        return E_OUTOFMEMORY;
    }
    info->help_context = number + 1000000;
    return S_OK;
}

// The VBA objects of the IDE are not part of a WFC program.
HRESULT WINAPI EbGetVBAObject(const IID* /*iid*/, void* const outer,
                              void* const reserved, void** const out) {
    if (out == nullptr) {
        return E_POINTER;
    }
    *out = nullptr;
    if (outer != nullptr || reserved != nullptr) {
        return E_INVALIDARG;
    }
    return E_NOINTERFACE;
}

// The runtime is this library: nothing to load.
HRESULT __cdecl EbLoadRunTime(void* /*project*/, void* /*context*/) {
    return S_OK;
}

// --- the type-information provider
// ------------------------------------------------

// A method of an object called through its table as the FUNCDESC describes
// it (oVft, calling convention, parameter and result types; a Variant
// argument array with the last argument first is not used: the arguments are
// in the order of the declaration). Optional parameters are E_INVALIDARG.
HRESULT WINAPI TipInvokeMethod2(void* const object, ITypeInfo* const info,
                                const FUNCDESC* const function,
                                VARIANTARG* const arguments,
                                VARIANT* const result) {
    if (function->cParamsOpt != 0) {
        return E_INVALIDARG;
    }
    const UINT count = static_cast<UINT>(function->cParams);
    std::vector<VARTYPE> types(count);
    std::vector<VARIANTARG*> values(count);
    auto resolve = [info](const TYPEDESC& type) {
        if (type.vt != VT_USERDEFINED || info == nullptr) {
            return type.vt;
        }
        ITypeInfo* other = nullptr;
        VARTYPE answer = VT_I4;
        if (SUCCEEDED(info->GetRefTypeInfo(type.hreftype, &other))) {
            TYPEATTR* attributes = nullptr;
            if (SUCCEEDED(other->GetTypeAttr(&attributes))) {
                if (attributes->typekind == TKIND_ALIAS) {
                    answer = attributes->tdescAlias.vt;
                } else if (attributes->typekind == TKIND_DISPATCH) {
                    answer = VT_DISPATCH;
                } else if (attributes->typekind == TKIND_INTERFACE ||
                           attributes->typekind == TKIND_COCLASS) {
                    answer = VT_UNKNOWN;
                }
                other->ReleaseTypeAttr(attributes);
            }
            other->Release();
        }
        return answer;
    };
    for (UINT i = 0; i < count; ++i) {
        types[i] = resolve(function->lprgelemdescParam[i].tdesc);
        values[i] = &arguments[i];
    }
    if (result != nullptr) {
        V_VT(result) = VT_EMPTY;
    }
    VARIANT ignored;
    VariantInit(&ignored);
    return DispCallFunc(
        object, static_cast<ULONG_PTR>(function->oVft), function->callconv,
        resolve(function->elemdescFunc.tdesc), count,
        count > 0 ? types.data() : nullptr, count > 0 ? values.data() : nullptr,
        result != nullptr ? result : &ignored);
}

HRESULT WINAPI TipInvokeMethod(void* const object,
                               const FUNCDESC* const function,
                               VARIANTARG* const arguments,
                               VARIANT* const result) {
    return TipInvokeMethod2(object, nullptr, function, arguments, result);
}

// Options of the provider: option 0 is the only one (another is 5).
HRESULT WINAPI TipSetOption(const LONG option, LONG /*value*/) {
    return option == 0 ? S_OK : 5;
}

// Instances of a project's classes and their predeclared instances belong to
// the object engine.
struct TipCreateRequest {
    void* reserved;
    const void* project;
    const CLSID* clsid;
    void* outer;
    const IID* iid;
    void** out;
};

HRESULT WINAPI TipCreateInstanceEx(TipCreateRequest* const request) {
    if (request->out != nullptr) {
        *request->out = nullptr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI TipCreateInstanceProject2(const void* /*project*/,
                                         const CLSID* /*clsid*/,
                                         IUnknown* /*outer*/,
                                         const IID* /*iid*/, void** const out) {
    if (out != nullptr) {
        *out = nullptr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI TipGetAddressOfPredeclaredInstance(const void* /*project*/,
                                                  WORD /*index*/,
                                                  void** const out) {
    if (out != nullptr) {
        *out = nullptr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

// An instance is unloaded by letting it go.
HRESULT WINAPI TipUnloadInstance(IUnknown* const instance) {
    if (instance != nullptr) {
        instance->Release();
    }
    return S_OK;
}

// A project that runs on this thread cannot be unloaded (E_FAIL); another is
// reset.
HRESULT WINAPI TipUnloadProject(const void* const project) {
    if (EbIsProjectOnStack(project)) {
        return E_FAIL;
    }
    vbrun::clear_error();
    return S_OK;
}

// --- debugger and help hooks
// ----------------------------------------------------

// The expression service of the debugger (version 4.0 only) is not part of
// a WFC program: no object.
void* WINAPI CreateIExprSrvObj(void* /*reserved*/, WORD /*major*/,
                               WORD /*minor*/) {
    return nullptr;
}

// The help-string entry of the VBA type library: contexts 900,000 to
// 965,536 are the library's; WFC carries none of their strings.
HRESULT WINAPI DLLGetDocumentation(ITypeLib* /*library*/, ITypeInfo* /*info*/,
                                   LCID /*locale*/, const DWORD context,
                                   BSTR* const text) {
    if (text == nullptr) {
        return E_POINTER;
    }
    *text = nullptr;
    if (context < 900000 || context > 965536) {
        return kBadIndex;
    }
    return kTypeElementNotFound;
}

}  // extern "C"
