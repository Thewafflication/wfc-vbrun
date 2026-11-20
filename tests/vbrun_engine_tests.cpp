// REQ-0005: the engine entry points: DllFunctionCall (error numbers and texts
// measured with a Visual Basic 6 program), End, Stop, ThunRTMain and the
// p-code entries (run in child processes, since they end the program), the
// ActiveX DLL registration (under a private HKEY_CLASSES_ROOT), contexts,
// projects, the type-information provider calls and the help hooks.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
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
struct EbErrorInfo {
    void* reserved;
    BSTR description;
    LONG help_context;
};
FARPROC WINAPI DllFunctionCall(const DllTemplate* declared);
void __vbaEnd();
void WINAPI __vbaStopExe(void* reserved);
void WINAPI ThunRTMain(const void* header);
void ProcCallEngine();
void MethCallEngine();
BOOL WINAPI UserDllMain(HMODULE* slot, void* reserved, HINSTANCE instance,
                        DWORD reason, void* reserved2);
HRESULT WINAPI VBDllRegisterServer(HMODULE* module, void* reserved,
                                   const void* header);
HRESULT WINAPI VBDllUnRegisterServer(HMODULE* module, void* reserved,
                                     const void* header);
HRESULT WINAPI VBDllGetClassObject(HMODULE* module, void* reserved,
                                   const void* header, const CLSID* clsid,
                                   const IID* iid, void** out);
HRESULT WINAPI VBDllCanUnloadNow(const void* header);
HRESULT WINAPI EbCreateContext(void** out);
void WINAPI EbDestroyContext(void* context);
HRESULT WINAPI EbGetObjConnectionCounts(LONG* connections, LONG* objects);
void WINAPI EbGetHandleOfExecutingProject(const void** out);
BOOL WINAPI EbIsProjectOnStack(const void* project);
HRESULT WINAPI EbResetProject(const void* project);
HRESULT WINAPI EbResetProjectNormal(const void* project);
HRESULT WINAPI EbGetErrorInfo(LONG number, EbErrorInfo* info);
HRESULT WINAPI EbGetVBAObject(const IID* iid, void* outer, void* reserved,
                              void** out);
HRESULT __cdecl EbLoadRunTime(void* project, void* context);
HRESULT WINAPI TipInvokeMethod(void* object, const FUNCDESC* function,
                               VARIANTARG* arguments, VARIANT* result);
HRESULT WINAPI TipInvokeMethod2(void* object, ITypeInfo* info,
                                const FUNCDESC* function, VARIANTARG* arguments,
                                VARIANT* result);
HRESULT WINAPI TipSetOption(LONG option, LONG value);
HRESULT WINAPI TipCreateInstanceEx(void* request);
HRESULT WINAPI TipCreateInstanceProject2(const void* project,
                                         const CLSID* clsid, IUnknown* outer,
                                         const IID* iid, void** out);
HRESULT WINAPI TipGetAddressOfPredeclaredInstance(const void* project,
                                                  WORD index, void** out);
HRESULT WINAPI TipUnloadInstance(IUnknown* instance);
HRESULT WINAPI TipUnloadProject(const void* project);
void* WINAPI CreateIExprSrvObj(void* reserved, WORD major, WORD minor);
HRESULT WINAPI DLLGetDocumentation(ITypeLib* library, ITypeInfo* info,
                                   LCID locale, DWORD context, BSTR* text);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::wstring description() {
    return vbrun::error_detail().description;
}

// Wine (the Winelib build) has no desktop for a message box and keeps its
// own registry rules; those checks run on Windows.
bool under_wine() {
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll != nullptr &&
           GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

// The Sub Main stub of the ThunRTMain check is x86 or x64 code.
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || \
    defined(__x86_64__)
constexpr bool kStubbable = true;
#else
constexpr bool kStubbable = false;
#endif

void declares() {
    DllCache cache = {};
    DllTemplate tick = {"kernel32", "GetTickCount", 0, 0, &cache};
    const FARPROC address = DllFunctionCall(&tick);
    expect(address != nullptr &&
               address == GetProcAddress(GetModuleHandleW(L"kernel32"),
                                         "GetTickCount") &&
               cache.address == address && cache.module != nullptr,
           "DllFunctionCall finds the procedure and fills the cache");

    // Measured: "nosuch 53 File not found: nosuchlib123.dll|0".
    DllCache none = {};
    DllTemplate missing = {"nosuchlib123.dll", "F", 0, 0, &none};
    vbrun::clear_error();
    expect(DllFunctionCall(&missing) == nullptr && vbrun::last_error() == 53 &&
               description() == L"File not found: nosuchlib123.dll",
           "a missing library is error 53 with its name");
    // "noentry 453 Can't find DLL entry point NoSuchEntry123 in kernel32".
    DllCache kernel = {};
    DllTemplate entry = {"kernel32", "NoSuchEntry123", 0, 0, &kernel};
    vbrun::clear_error();
    expect(DllFunctionCall(&entry) == nullptr && vbrun::last_error() == 453 &&
               description() ==
                   L"Can't find DLL entry point NoSuchEntry123 in kernel32",
           "a missing procedure is error 453");
    // "byord 452 Can't find DLL entry point 9999 in kernel32".
    DllTemplate ordinal = {"kernel32", nullptr, 9999, 2, &kernel};
    vbrun::clear_error();
    expect(DllFunctionCall(&ordinal) == nullptr && vbrun::last_error() == 452 &&
               description() == L"Can't find DLL entry point 9999 in kernel32",
           "a missing ordinal is error 452");
    vbrun::clear_error();
}

void contexts() {
    EbErrorInfo info = {};
    expect(EbGetErrorInfo(5, &info) == S_OK && info.description != nullptr &&
               std::wstring(info.description) ==
                   L"Invalid procedure call or argument" &&
               info.help_context == 1000005,
           "EbGetErrorInfo gives the message and the help context");
    SysFreeString(info.description);

    void* first = nullptr;
    void* second = nullptr;
    LONG connections = -9;
    LONG objects = -9;
    expect(EbCreateContext(&first) == S_OK && first != nullptr &&
               EbCreateContext(&second) == S_OK,
           "EbCreateContext makes contexts");
    expect(EbGetObjConnectionCounts(&connections, &objects) == S_OK &&
               connections == 1 && objects == 0,
           "the connections are the contexts but the first");
    expect(EbGetObjConnectionCounts(nullptr, &objects) == 5,
           "a null pointer answers 5");
    EbDestroyContext(second);
    EbDestroyContext(first);

    const void* project = &first;
    EbGetHandleOfExecutingProject(&project);
    expect(project == nullptr && !EbIsProjectOnStack(&first),
           "no project runs on this thread");
    static_cast<void>(vbrun::raise(5));
    expect(EbResetProject(&first) == S_OK && vbrun::last_error() == 0 &&
               EbResetProjectNormal(&first) == S_OK,
           "a reset clears the error");
    void* object = &first;
    expect(EbGetVBAObject(&IID_IUnknown, nullptr, nullptr, &object) ==
                   E_NOINTERFACE &&
               object == nullptr &&
               EbGetVBAObject(&IID_IUnknown, &first, nullptr, &object) ==
                   E_INVALIDARG &&
               EbGetVBAObject(&IID_IUnknown, nullptr, nullptr, nullptr) ==
                   E_POINTER,
           "EbGetVBAObject has no VBA objects");
    expect(EbLoadRunTime(nullptr, nullptr) == S_OK, "EbLoadRunTime");
    expect(CreateIExprSrvObj(nullptr, 4, 0) == nullptr,
           "no expression service");
    BSTR text = SysAllocString(L"x");
    expect(DLLGetDocumentation(nullptr, nullptr, 0, 900001, &text) ==
                   static_cast<HRESULT>(0x8002802BU) &&
               text == nullptr &&
               DLLGetDocumentation(nullptr, nullptr, 0, 5, &text) ==
                   static_cast<HRESULT>(0x8002000BU) &&
               DLLGetDocumentation(nullptr, nullptr, 0, 900001, nullptr) ==
                   E_POINTER,
           "DLLGetDocumentation answers by context");
}

// A COM-style object whose third method adds its two arguments.
struct Adder {
    struct Table {
        void* methods[4];
    };
    const Table* table;
    LONG base;
};

LONG WINAPI add(Adder* const self, const LONG a, const LONG b) {
    return self->base + a + b;
}

void provider() {
    static const Adder::Table table = {
        {nullptr, nullptr, nullptr, reinterpret_cast<void*>(&add)}};
    Adder adder = {&table, 100};
    ELEMDESC parameters[2] = {};
    parameters[0].tdesc.vt = VT_I4;
    parameters[1].tdesc.vt = VT_I4;
    FUNCDESC function = {};
    function.lprgelemdescParam = parameters;
    function.cParams = 2;
    function.oVft = static_cast<SHORT>(3 * sizeof(void*));
    function.callconv = CC_STDCALL;
    function.elemdescFunc.tdesc.vt = VT_I4;
    VARIANTARG arguments[2];
    VariantInit(&arguments[0]);
    VariantInit(&arguments[1]);
    V_VT(&arguments[0]) = VT_I4;
    V_I4(&arguments[0]) = 20;
    V_VT(&arguments[1]) = VT_I4;
    V_I4(&arguments[1]) = 3;
    VARIANT result;
    VariantInit(&result);
    expect(TipInvokeMethod(&adder, &function, arguments, &result) == S_OK &&
               V_VT(&result) == VT_I4 && V_I4(&result) == 123,
           "TipInvokeMethod calls the method of the table");
    function.cParamsOpt = 1;
    expect(TipInvokeMethod2(&adder, nullptr, &function, arguments, &result) ==
               E_INVALIDARG,
           "optional parameters are E_INVALIDARG");
    expect(TipSetOption(0, 1) == S_OK && TipSetOption(1, 1) == 5,
           "TipSetOption knows option 0");
    void* out = &adder;
    void* request[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, &out};
    expect(TipCreateInstanceEx(request) == CLASS_E_CLASSNOTAVAILABLE &&
               out == nullptr,
           "TipCreateInstanceEx needs the object engine");
    out = &adder;
    expect(TipCreateInstanceProject2(nullptr, nullptr, nullptr, nullptr,
                                     &out) == CLASS_E_CLASSNOTAVAILABLE &&
               out == nullptr &&
               TipGetAddressOfPredeclaredInstance(nullptr, 0, &out) ==
                   CLASS_E_CLASSNOTAVAILABLE,
           "instances need the object engine");
    expect(
        TipUnloadInstance(nullptr) == S_OK && TipUnloadProject(&adder) == S_OK,
        "unloading");
}

// Memory below 4 GB, where the 32-bit addresses of a project header reach.
unsigned char* low_memory() {
    for (std::uintptr_t at = 0x10000000; at < 0x7f000000; at += 0x01000000) {
        void* const block =
            VirtualAlloc(reinterpret_cast<void*>(at), 0x10000,
                         MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (block != nullptr) {
            return static_cast<unsigned char*>(block);
        }
    }
    return nullptr;
}

void put32(unsigned char* const at, const std::uint32_t value) {
    std::memcpy(at, &value, 4);
}

std::uint32_t low_address(const void* const p) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

// A project header with the registration data of one MultiUse class
// (Project1.Class1) and one private class.
unsigned char* make_header(unsigned char* const memory) {
    unsigned char* const header = memory;
    std::memcpy(header, "VB5!", 4);
    unsigned char* const data = memory + 0x400;
    put32(header + 0x54, low_address(data));
    put32(header + 0x5c, 0x68);
    std::memcpy(header + 0x68, "Probe Title", 12);
    put32(data + 0, 0x40);
    put32(data + 4, 0x30);
    std::memcpy(data + 0x30, "Project1", 9);
    const GUID library = {0x5a0bd1e1,
                          0x1234,
                          0x4abc,
                          {0x90, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}};
    std::memcpy(data + 0x10, &library, sizeof library);
    const WORD one = 1;
    std::memcpy(data + 0x26, &one, 2);
    unsigned char* const first = data + 0x40;
    put32(first + 0, 0x100);
    put32(first + 4, 0x90);
    std::memcpy(data + 0x90, "Class1", 7);
    put32(first + 0xc, 5);
    const CLSID clsid = {0x5a0bd1e2,
                         0x1234,
                         0x4abc,
                         {0x90, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x88}};
    std::memcpy(first + 0x14, &clsid, sizeof clsid);
    unsigned char* const second = data + 0x100;
    put32(second + 4, 0x98);
    std::memcpy(data + 0x98, "Hidden", 7);
    put32(second + 0xc, 1);
    return header;
}

bool key_exists(const wchar_t* const key) {
    HKEY handle = nullptr;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, key, 0, KEY_READ, &handle) !=
        ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(handle);
    return true;
}

void registration(unsigned char* const memory) {
    const unsigned char* const header = make_header(memory);
    const wchar_t* const root = L"Software\\WFC\\vbrun-engine-tests";
    HKEY classes = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER, root, 0, nullptr, 0, KEY_ALL_ACCESS,
                    nullptr, &classes, nullptr);
    RegOverridePredefKey(HKEY_CLASSES_ROOT, classes);
    HMODULE module = GetModuleHandleW(nullptr);
    expect(VBDllRegisterServer(&module, nullptr, header) == S_OK,
           "VBDllRegisterServer succeeds");
    expect(key_exists(L"Project1.Class1\\Clsid") &&
               key_exists(L"CLSID\\{5A0BD1E2-1234-4ABC-9011-223344556688}"
                          L"\\InprocServer32") &&
               !key_exists(L"Project1.Hidden"),
           "a creatable class is registered, a private one is not");
    expect(VBDllUnRegisterServer(&module, nullptr, header) == S_OK &&
               !key_exists(L"Project1.Class1"),
           "VBDllUnRegisterServer removes the class");
    const char bad[8] = "VB4!";
    expect(VBDllRegisterServer(&module, nullptr, bad) == E_UNEXPECTED,
           "a header that is not a project's is E_UNEXPECTED");
    RegOverridePredefKey(HKEY_CLASSES_ROOT, nullptr);
    RegCloseKey(classes);
    RegDeleteTreeW(HKEY_CURRENT_USER, root);

    void* out = &module;
    expect(
        VBDllGetClassObject(&module, nullptr, header, &IID_IUnknown,
                            &IID_IUnknown, &out) == CLASS_E_CLASSNOTAVAILABLE &&
            out == nullptr &&
            VBDllGetClassObject(&module, nullptr, bad, &IID_IUnknown,
                                &IID_IUnknown, &out) == E_UNEXPECTED,
        "VBDllGetClassObject has no class factory");
    expect(VBDllCanUnloadNow(header) == S_OK, "VBDllCanUnloadNow");
    HMODULE slot = nullptr;
    expect(UserDllMain(&slot, nullptr, reinterpret_cast<HINSTANCE>(module),
                       DLL_PROCESS_ATTACH, nullptr) == TRUE &&
               slot == module,
           "UserDllMain keeps the module on attach");
}

// --- the child processes
// ------------------------------------------------------

void WINAPI sub_main() {
    const HANDLE file = CreateFileW(L"engine-submain.txt", GENERIC_WRITE, 0,
                                    nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
}

int child(const std::string& what, unsigned char* const memory) {
    if (what == "end") {
        __vbaEnd();
    } else if (what == "stop") {
        __vbaStopExe(nullptr);
    } else if (what == "pcode") {
        ProcCallEngine();
    } else if (what == "thunrt") {
        unsigned char* const header = make_header(memory);
#if defined(_M_IX86) || defined(__i386__)
        put32(header + 0x2c, low_address(reinterpret_cast<void*>(&sub_main)));
#else
        // mov rax, sub_main; jmp rax
        unsigned char* const stub = memory + 0x800;
        const std::uint64_t target =
            reinterpret_cast<std::uintptr_t>(&sub_main);
        stub[0] = 0x48;
        stub[1] = 0xb8;
        std::memcpy(stub + 2, &target, 8);
        stub[10] = 0xff;
        stub[11] = 0xe0;
        FlushInstructionCache(GetCurrentProcess(), stub, 12);
        put32(header + 0x2c, low_address(stub));
#endif
        ThunRTMain(header);
    }
    return 99;
}

struct Child {
    PROCESS_INFORMATION process = {};
};

bool start(const wchar_t* const argument, Child& out) {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring command = L"\"" + std::wstring(path) + L"\" " + argument;
    STARTUPINFOW startup = {};
    startup.cb = sizeof startup;
    return CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0,
                          nullptr, nullptr, &startup, &out.process) != 0;
}

DWORD finish(Child& child_process, const DWORD wait) {
    DWORD code = 0xffffffff;
    if (WaitForSingleObject(child_process.process.hProcess, wait) !=
        WAIT_OBJECT_0) {
        TerminateProcess(child_process.process.hProcess, 98);
        WaitForSingleObject(child_process.process.hProcess, 5000);
    }
    GetExitCodeProcess(child_process.process.hProcess, &code);
    CloseHandle(child_process.process.hThread);
    CloseHandle(child_process.process.hProcess);
    return code;
}

struct Found {
    DWORD process;
    HWND window;
    std::wstring text;
};

BOOL CALLBACK find_text(HWND child_window, LPARAM data) {
    Found* const found = reinterpret_cast<Found*>(data);
    wchar_t text[256] = {};
    GetWindowTextW(child_window, text, 256);
    if (std::wstring(text) == L"Stop statement encountered") {
        found->text = text;
    }
    return TRUE;
}

BOOL CALLBACK find_box(HWND window, LPARAM data) {
    Found* const found = reinterpret_cast<Found*>(data);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == found->process && IsWindowVisible(window)) {
        EnumChildWindows(window, find_text, data);
        if (!found->text.empty()) {
            found->window = window;
            return FALSE;
        }
    }
    return TRUE;
}

void programs() {
    Child end;
    expect(start(L"end", end) && finish(end, 20000) == 0,
           "End ends the program with exit code 0");
    Child pcode;
    expect(start(L"pcode", pcode) && finish(pcode, 20000) == 0x5642,
           "p-code stops the program with a diagnostic");
    if constexpr (kStubbable) {
        DeleteFileW(L"engine-submain.txt");
        Child thunrt;
        const bool started = start(L"thunrt", thunrt);
        expect(started && finish(thunrt, 20000) == 0 &&
                   GetFileAttributesW(L"engine-submain.txt") !=
                       INVALID_FILE_ATTRIBUTES,
               "ThunRTMain runs Sub Main and ends the program");
        DeleteFileW(L"engine-submain.txt");
    }
    if (under_wine()) {
        return;
    }

    // Stop shows its message box; the test answers it.
    Child stop;
    if (!start(L"stop", stop)) {
        expect(false, "the Stop child starts");
        return;
    }
    Found found = {stop.process.dwProcessId, nullptr, {}};
    for (int i = 0; i < 200 && found.window == nullptr; ++i) {
        Sleep(50);
        EnumWindows(find_box, reinterpret_cast<LPARAM>(&found));
    }
    expect(found.window != nullptr,
           "Stop shows \"Stop statement encountered\"");
    wchar_t title[256] = {};
    if (found.window != nullptr) {
        GetWindowTextW(found.window, title, 256);
        for (int i = 0; i < 100 && WaitForSingleObject(stop.process.hProcess,
                                                       100) == WAIT_TIMEOUT;
             ++i) {
            PostMessageW(found.window, WM_CLOSE, 0, 0);
        }
    }
    expect(std::wstring(title) == L"vbrun_engine_tests",
           "the message box has the program's title");
    expect(finish(stop, 20000) == 0, "Stop then ends the program");
}

}  // namespace

int main(int argc, char** argv) {
    unsigned char* const memory = low_memory();
    if (argc > 1) {
        return child(argv[1], memory);
    }
    declares();
    contexts();
    provider();
    if (memory == nullptr) {
        expect(false, "memory below 4 GB for a project header");
    } else if (!under_wine()) {
        registration(memory);
    }
    programs();
    if (failures == 0) {
        std::printf("vbrun engine: all checks passed\n");
        return 0;
    }
    return 1;
}
