// Behavior tests of the VBRUN helpers (REQ-0100 to REQ-0104). Usage:
//   vbrun_helper_tests <wfcvbrun.dll> <family>
// Each family calls exports of the library by name with the argument lists of
// the reference runtime and checks the results and the effects the reference
// has (probed on MSVBVM60 6.00.9848 with scripts/Probe-Vbrun.ps1).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <string>
#include <vector>

namespace {

HMODULE g_library = nullptr;
int g_failures = 0;
int g_checks = 0;

void check(const bool condition, const char* const what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}

template <typename Function>
Function export_of(const char* const name) {
    const FARPROC proc = GetProcAddress(g_library, name);
    if (proc == nullptr) {
        std::fprintf(stderr, "FAIL: %s is not exported\n", name);
        ++g_failures;
    }
    return reinterpret_cast<Function>(proc);
}

// ----- REQ-0103 member and memory helpers ---------------------------------

// A counted object for the object helpers.
struct Counted final : IUnknown {
    long references = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override {
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(++references);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return static_cast<ULONG>(--references);
    }
};

void memory_family() {
    using Get1 = HRESULT(WINAPI*)(const std::uint8_t*, std::uint8_t*);
    using Get2 = HRESULT(WINAPI*)(const std::uint16_t*, std::uint16_t*);
    using Get4 = HRESULT(WINAPI*)(const std::uint32_t*, std::uint32_t*);
    using Put1 = HRESULT(WINAPI*)(std::uint8_t*, std::uint8_t);
    using Put2 = HRESULT(WINAPI*)(std::uint16_t*, std::uint16_t);
    using Put4 = HRESULT(WINAPI*)(std::uint32_t*, std::uint32_t);
    using Put8 = HRESULT(WINAPI*)(std::uint32_t*, std::uint32_t, std::uint32_t);
    const auto get1 = export_of<Get1>("GetMem1");
    const auto get2 = export_of<Get2>("GetMem2");
    const auto get4 = export_of<Get4>("GetMem4");
    const auto get8 = export_of<Get4>("GetMem8");
    const auto put1 = export_of<Put1>("PutMem1");
    const auto put2 = export_of<Put2>("PutMem2");
    const auto put4 = export_of<Put4>("PutMem4");
    const auto put8 = export_of<Put8>("PutMem8");
    const auto var_ptr = export_of<LONG_PTR(WINAPI*)(void*)>("VarPtr");
    if (!get1 || !get2 || !get4 || !get8 || !put1 || !put2 || !put4 || !put8 ||
        !var_ptr) {
        return;
    }
    std::uint8_t byte_in = 0xA5;
    std::uint8_t byte_out = 0;
    check(get1(&byte_in, &byte_out) == S_OK && byte_out == 0xA5, "GetMem1");
    std::uint16_t word_in = 0xBEEF;
    std::uint16_t word_out = 0;
    check(get2(&word_in, &word_out) == S_OK && word_out == 0xBEEF, "GetMem2");
    std::uint32_t long_in = 0xDEADBEEFU;
    std::uint32_t long_out = 0;
    check(get4(&long_in, &long_out) == S_OK && long_out == 0xDEADBEEFU,
          "GetMem4");
    const std::uint32_t wide_in[2] = {0x01234567U, 0x89ABCDEFU};
    std::uint32_t wide_out[2] = {0, 0};
    check(get8(wide_in, wide_out) == S_OK && wide_out[0] == 0x01234567U &&
              wide_out[1] == 0x89ABCDEFU,
          "GetMem8");

    std::uint8_t put_byte = 0;
    check(put1(&put_byte, 0x7F) == S_OK && put_byte == 0x7F, "PutMem1");
    std::uint16_t put_word = 0;
    check(put2(&put_word, 0x1234) == S_OK && put_word == 0x1234, "PutMem2");
    std::uint32_t put_long = 0;
    check(put4(&put_long, 0xCAFEF00DU) == S_OK && put_long == 0xCAFEF00DU,
          "PutMem4");
    std::uint32_t put_wide[2] = {0, 0};
    check(put8(put_wide, 0x11223344U, 0x55667788U) == S_OK &&
              put_wide[0] == 0x11223344U && put_wide[1] == 0x55667788U,
          "PutMem8");
    check(var_ptr(&put_long) == reinterpret_cast<LONG_PTR>(&put_long),
          "VarPtr");

    // Strings are copied by byte length (an odd length is kept).
    using GetStr = HRESULT(WINAPI*)(BSTR*, BSTR*);
    using PutStr = HRESULT(WINAPI*)(BSTR*, BSTR);
    const auto get_str = export_of<GetStr>("GetMemStr");
    const auto put_str = export_of<PutStr>("PutMemStr");
    if (get_str && put_str) {
        BSTR source = SysAllocString(L"hello");
        BSTR copy = nullptr;
        check(get_str(&source, &copy) == S_OK && copy != nullptr &&
                  copy != source && SysStringLen(copy) == 5U &&
                  std::wmemcmp(copy, L"hello", 5) == 0,
              "GetMemStr copies a String");
        SysFreeString(copy);
        SysFreeString(source);
        BSTR odd = SysAllocStringByteLen("abc", 3);
        BSTR odd_copy = nullptr;
        check(get_str(&odd, &odd_copy) == S_OK && odd_copy != nullptr &&
                  SysStringByteLen(odd_copy) == 3U &&
                  std::memcmp(odd_copy, "abc", 3) == 0,
              "GetMemStr keeps an odd byte count");
        SysFreeString(odd);
        SysFreeString(odd_copy);
        BSTR none = nullptr;
        BSTR got = SysAllocString(L"x");
        check(get_str(&none, &got) == S_OK && got == nullptr,
              "GetMemStr of an empty String gives an empty String");

        BSTR target = SysAllocString(L"old");
        BSTR value = SysAllocString(L"new value");
        check(put_str(&target, value) == S_OK && target != nullptr &&
                  target != value && SysStringLen(target) == 9U &&
                  std::wmemcmp(target, L"new value", 9) == 0,
              "PutMemStr replaces a String with a copy");
        check(put_str(&target, nullptr) == S_OK && target != nullptr &&
                  SysStringByteLen(target) == 0U,
              "PutMemStr of an empty String gives an allocated empty String");
        SysFreeString(target);
        SysFreeString(value);
    }

    // Objects are counted.
    using GetObj = HRESULT(WINAPI*)(IUnknown**, IUnknown**);
    using SetObj = HRESULT(WINAPI*)(IUnknown**, IUnknown*);
    const auto get_obj = export_of<GetObj>("GetMemObj");
    const auto set_obj = export_of<SetObj>("SetMemObj");
    if (get_obj && set_obj) {
        Counted first;
        Counted second;
        IUnknown* held = &first;
        IUnknown* taken = nullptr;
        check(get_obj(&held, &taken) == S_OK && taken == &first &&
                  first.references == 2,
              "GetMemObj counts the copy");
        check(set_obj(&held, &second) == S_OK && held == &second &&
                  second.references == 2 && first.references == 1,
              "SetMemObj counts the new object and lets the old one go");
        check(set_obj(&held, nullptr) == S_OK && held == nullptr &&
                  second.references == 1,
              "SetMemObj of Nothing");
        IUnknown* empty = nullptr;
        IUnknown* sink = &first;
        check(get_obj(&empty, &sink) == S_OK && sink == nullptr,
              "GetMemObj of Nothing");
    }

    // A Variant is copied.
    using GetVar = HRESULT(WINAPI*)(VARIANT*, VARIANT*);
    const auto get_var = export_of<GetVar>("GetMemVar");
    if (get_var) {
        VARIANT from;
        VariantInit(&from);
        from.vt = VT_BSTR;
        from.bstrVal = SysAllocString(L"variant text");
        VARIANT to;
        VariantInit(&to);
        check(get_var(&from, &to) == S_OK && to.vt == VT_BSTR &&
                  to.bstrVal != from.bstrVal &&
                  std::wcscmp(to.bstrVal, L"variant text") == 0,
              "GetMemVar copies a Variant");
        VariantClear(&from);
        VariantClear(&to);
    }
}

// ----- Call cases ----------------------------------------------------------
//
// tests/cases/<family>.cases lists calls (`export arg... => result`);
// <family>.expected holds what the reference runtime answered, one line per
// case (scripts/Probe-VbrunCases.ps1). Arguments: s"text" (a String), s0
// (Nothing), i123, x7F. Results: s (a String), i (a Long), h (an HRESULT).

std::wstring read_case_text(const std::string& text) {
    std::wstring result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '\\') {
            result.push_back(
                static_cast<wchar_t>(static_cast<unsigned char>(c)));
            continue;
        }
        ++i;
        switch (text[i]) {
            case 'n':
                result.push_back(L'\n');
                break;
            case 'r':
                result.push_back(L'\r');
                break;
            case 't':
                result.push_back(L'\t');
                break;
            case 'u':
                result.push_back(static_cast<wchar_t>(
                    std::stoul(text.substr(i + 1U, 4U), nullptr, 16)));
                i += 4U;
                break;
            default:
                result.push_back(static_cast<wchar_t>(text[i]));
                break;
        }
    }
    return result;
}

std::string write_case_text(const wchar_t* const text,
                            const std::size_t length) {
    std::string result;
    for (std::size_t i = 0; i < length; ++i) {
        const auto code = static_cast<unsigned>(text[i]);
        if (text[i] == L'\\') {
            result += "\\\\";
        } else if (text[i] == L'"') {
            result += "\\\"";
        } else if (code < 32U || code > 126U) {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04X", code);
            result += buffer;
        } else {
            result.push_back(static_cast<char>(code));
        }
    }
    return result;
}

using Word = std::uintptr_t;

// A Variant of the case grammar (see Probe-VbrunCases.ps1).
VARIANT* make_variant(const int vt, const std::string& number,
                      const std::wstring& text) {
    auto* v = static_cast<VARIANT*>(CoTaskMemAlloc(sizeof(VARIANT)));
    std::memset(v, 0, sizeof(VARIANT));
    v->vt = static_cast<VARTYPE>(vt);
    switch (vt) {
        case 2:
        case 11:
            v->iVal = static_cast<SHORT>(std::stol(number));
            break;
        case 3:
        case 10:
            v->lVal = number.rfind('x', 0) == 0
                          ? static_cast<LONG>(
                                std::stoul(number.substr(1), nullptr, 16))
                          : static_cast<LONG>(std::stol(number));
            break;
        case 17:
            v->bVal = static_cast<BYTE>(std::stoul(number));
            break;
        case 4:
            if (number.rfind('x', 0) == 0) {  // the bits of the Single
                const auto bits = static_cast<std::uint32_t>(
                    std::stoul(number.substr(1), nullptr, 16));
                std::memcpy(&v->fltVal, &bits, sizeof(bits));
            } else {
                v->fltVal = std::stof(number);
            }
            break;
        case 5:
        case 7:
            if (number.rfind('x', 0) == 0) {  // the bits of the Double
                const std::uint64_t bits =
                    std::stoull(number.substr(1), nullptr, 16);
                std::memcpy(&v->dblVal, &bits, sizeof(bits));
            } else {
                v->dblVal = std::stod(number);
            }
            break;
        case 6:
            v->cyVal.int64 = std::stoll(number);
            break;
        case 8:
            v->bstrVal =
                SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
            break;
        case 8200: {  // an array of Strings: the elements, split at '|'
            std::vector<std::wstring> items;
            if (std::stol(number) > 0) {
                std::size_t from = 0;
                for (;;) {
                    const std::size_t bar = text.find(L'|', from);
                    items.push_back(text.substr(
                        from, bar == std::wstring::npos ? bar : bar - from));
                    if (bar == std::wstring::npos) {
                        break;
                    }
                    from = bar + 1U;
                }
            }
            SAFEARRAYBOUND bound = {static_cast<ULONG>(items.size()), 0};
            SAFEARRAY* const array = SafeArrayCreate(VT_BSTR, 1, &bound);
            for (LONG at = 0; at < static_cast<LONG>(items.size()); ++at) {
                BSTR item = SysAllocStringLen(
                    items[static_cast<std::size_t>(at)].data(),
                    static_cast<UINT>(
                        items[static_cast<std::size_t>(at)].size()));
                SafeArrayPutElement(array, &at, item);
                SysFreeString(item);
            }
            v->parray = array;
            break;
        }
        default:
            break;
    }
    return v;
}

std::string format_variant(const VARIANT* const v);

std::string hex_text(const char* const format, const unsigned long long value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}

std::string format_variant(const VARIANT* const v) {
    const int vt = v->vt;
    switch (vt) {
        case 0:
        case 1:
            return "v" + std::to_string(vt);
        case 2:
        case 11:
            return "v" + std::to_string(vt) + ":" + std::to_string(v->iVal);
        case 3:
            return "v3:" + std::to_string(v->lVal);
        case 10:
            return "v10:" +
                   hex_text("x%08llX", static_cast<unsigned long>(v->lVal));
        case 17:
            return "v17:" + std::to_string(v->bVal);
        case 4: {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &v->fltVal, sizeof(bits));
            return "v4:" + hex_text("x%08llX", bits);
        }
        case 5:
        case 7: {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &v->dblVal, sizeof(bits));
            return "v" + std::to_string(vt) + ":" + hex_text("x%016llX", bits);
        }
        case 6:
            return "v6:" + std::to_string(v->cyVal.int64);
        case 8:
            if (v->bstrVal == nullptr) {
                return "v8:0";
            }
            return "v8\"" +
                   write_case_text(v->bstrVal,
                                   SysStringByteLen(v->bstrVal) / 2U) +
                   "\"";
        case 8200: {  // an array of Strings
            if (v->parray == nullptr) {
                return "v8200:0\"\"";
            }
            LONG low = 0;
            LONG high = -1;
            SafeArrayGetLBound(v->parray, 1, &low);
            SafeArrayGetUBound(v->parray, 1, &high);
            std::string text;
            for (LONG at = low; at <= high; ++at) {
                BSTR item = nullptr;
                SafeArrayGetElement(v->parray, &at, &item);
                if (at != low) {
                    text += "|";
                }
                if (item != nullptr) {
                    text += write_case_text(item, SysStringByteLen(item) / 2U);
                    SysFreeString(item);
                }
            }
            return "v8200:" + std::to_string(high - low + 1) + "\"" + text +
                   "\"";
        }
        default:
            return "v" + std::to_string(vt) + ":?";
    }
}

Word call_with(const FARPROC proc, const std::vector<Word>& a) {
    switch (a.size()) {
        case 0:
            return reinterpret_cast<Word(WINAPI*)()>(proc)();
        case 1:
            return reinterpret_cast<Word(WINAPI*)(Word)>(proc)(a[0]);
        case 2:
            return reinterpret_cast<Word(WINAPI*)(Word, Word)>(proc)(a[0],
                                                                     a[1]);
        case 3:
            return reinterpret_cast<Word(WINAPI*)(Word, Word, Word)>(proc)(
                a[0], a[1], a[2]);
        case 4:
            return reinterpret_cast<Word(WINAPI*)(Word, Word, Word, Word)>(
                proc)(a[0], a[1], a[2], a[3]);
        case 5:
            return reinterpret_cast<Word(WINAPI*)(
                Word, Word, Word, Word, Word)>(proc)(a[0], a[1], a[2], a[3],
                                                     a[4]);
        default:
            return reinterpret_cast<Word(WINAPI*)(
                Word, Word, Word, Word, Word, Word)>(proc)(a[0], a[1], a[2],
                                                           a[3], a[4], a[5]);
    }
}

// ----- file cases ----------------------------------------------------------
// The cases of the file exports start from a scratch folder that a `#fixture`
// line rebuilds (the same files, times and attributes the Visual Basic 6
// harness started from); the results `n` (the state of the folder), `r` and
// `R` (a BSTR and a Variant string with the scratch folder shown as <R>).

std::wstring g_root;

void wipe_folder(const std::wstring& folder) {
    WIN32_FIND_DATAW data;
    const HANDLE found = FindFirstFileW((folder + L"\\*").c_str(), &data);
    if (found == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        const std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }
        const std::wstring path = folder + L"\\" + name;
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
            wipe_folder(path);
            RemoveDirectoryW(path.c_str());
        } else {
            DeleteFileW(path.c_str());
        }
    } while (FindNextFileW(found, &data));
    FindClose(found);
}

void put_file(const wchar_t* const name, const char* const text) {
    std::ofstream out(name, std::ios::binary);
    out << text;
}

// Sets the time of the last write to a local time.
void stamp(const wchar_t* const name, const WORD year, const WORD month,
           const WORD day, const WORD hour, const WORD minute,
           const WORD second) {
    SYSTEMTIME at = {year, month, 0, day, hour, minute, second, 0};
    FILETIME local;
    FILETIME utc;
    SystemTimeToFileTime(&at, &local);
    LocalFileTimeToFileTime(&local, &utc);
    const HANDLE file = CreateFileW(
        name, FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        SetFileTime(file, nullptr, nullptr, &utc);
        CloseHandle(file);
    }
}

void build_fixture() {
    if (g_root.empty()) {
        wchar_t temp[MAX_PATH];
        const DWORD length = GetTempPathW(MAX_PATH, temp);
        std::wstring base(temp, length);
        while (!base.empty() && base.back() == L'\\') {
            base.pop_back();
        }
        g_root =
            base + L"\\wfcvbrunfx" + std::to_wstring(GetCurrentProcessId());
    }
    SetCurrentDirectoryW(g_root.substr(0, g_root.rfind(L'\\')).c_str());
    wipe_folder(g_root);
    CreateDirectoryW(g_root.c_str(), nullptr);
    SetCurrentDirectoryW(g_root.c_str());
    put_file(L"a.txt", "hello");
    put_file(L"b.txt", "abcdefghij");
    put_file(L"c.txt", "ccc");
    put_file(L"empty.txt", "");
    put_file(L"ro.txt", "abc");
    put_file(L"hid.txt", "x");
    CreateDirectoryW(L"sub", nullptr);
    put_file(L"sub\\in.txt", "in");
    CreateDirectoryW(L"emptydir", nullptr);
    stamp(L"a.txt", 2000, 1, 1, 13, 45, 10);
    stamp(L"c.txt", 2000, 1, 1, 13, 45, 11);
    for (const wchar_t* const name :
         {L"b.txt", L"empty.txt", L"ro.txt", L"hid.txt", L"sub\\in.txt", L"sub",
          L"emptydir"}) {
        stamp(name, 2001, 2, 3, 4, 5, 6);
    }
    SetFileAttributesW(L"ro.txt", FILE_ATTRIBUTE_READONLY);
    SetFileAttributesW(L"hid.txt", FILE_ATTRIBUTE_HIDDEN);
}

std::string narrow(const std::wstring& text) {
    std::string result;
    for (const wchar_t c : text) {
        result += static_cast<char>(c);
    }
    return result;
}

std::wstring upper(std::wstring text) {
    for (wchar_t& c : text) {
        c = static_cast<wchar_t>(std::towupper(c));
    }
    return text;
}

// The files and folders under `folder` ("" or ends with a separator), by
// name: name:attributes:size, a folder with its contents in braces.
std::string folder_tree(const std::wstring& folder) {
    std::vector<WIN32_FIND_DATAW> entries;
    WIN32_FIND_DATAW data;
    const HANDLE found = FindFirstFileW((folder + L"*").c_str(), &data);
    if (found != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = data.cFileName;
            if (name != L"." && name != L"..") {
                entries.push_back(data);
            }
        } while (FindNextFileW(found, &data));
        FindClose(found);
    }
    std::sort(entries.begin(), entries.end(),
              [](const WIN32_FIND_DATAW& a, const WIN32_FIND_DATAW& b) {
                  return upper(a.cFileName) < upper(b.cFileName);
              });
    std::string text;
    for (const WIN32_FIND_DATAW& entry : entries) {
        const std::wstring name = entry.cFileName;
        const std::string narrow_name = narrow(name);
        const unsigned attributes = entry.dwFileAttributes & 0x37U;
        text += narrow_name + ":" + std::to_string(attributes) + ":";
        if ((attributes & 0x10U) != 0U) {
            text += "0{" + folder_tree(folder + name + L"\\") + "};";
        } else {
            text += std::to_string(entry.nFileSizeLow) + ";";
        }
    }
    return text;
}

std::string folder_state() {
    wchar_t buffer[MAX_PATH * 2];
    const std::wstring current(buffer,
                               GetCurrentDirectoryW(MAX_PATH * 2, buffer));
    std::wstring where;
    if (upper(current) == upper(g_root)) {
        where = L".";
    } else if (upper(current).rfind(upper(g_root) + L"\\", 0) == 0) {
        where = current.substr(g_root.size() + 1U);
    } else {
        where = L"?" + current;
    }
    return "cwd=" + narrow(where) + " " + folder_tree(L"");
}

// The text with the scratch folder shown as <R>.
std::wstring hide_root(const std::wstring& text) {
    std::wstring result;
    const std::wstring key = upper(g_root);
    const std::wstring big = upper(text);
    std::size_t at = 0;
    while (at < text.size()) {
        if (!key.empty() && big.compare(at, key.size(), key) == 0) {
            result += L"<R>";
            at += key.size();
        } else {
            result += text[at++];
        }
    }
    return result;
}

// Runs one case line and returns the result text.
std::string run_case(const std::string& line) {
    const auto arrow = line.find(" => ");
    const std::string head = line.substr(0, arrow);
    const std::string kind = line.substr(arrow + 4U);
    const auto space = head.find(' ');
    const std::string name = head.substr(0, space);
    std::string rest =
        space == std::string::npos ? std::string() : head.substr(space + 1U);
    std::vector<Word> arguments;
    std::vector<BSTR> allocated;
    std::vector<VARIANT*> variants;
    std::vector<VARIANT*> outputs;
    double number_argument = 0.0;  // d1.5: a Double passed by value
    bool has_double = false;
    std::vector<double> doubles;  // every Double argument, in order
    std::vector<float> singles;   // f1.5: a Single passed by value
    std::vector<long long> cys;   // y123456: a Currency by value
    while (!rest.empty()) {
        while (!rest.empty() && rest.front() == ' ') {
            rest.erase(0, 1);
        }
        if (rest.empty()) {
            break;
        }
        if (rest.rfind("s0", 0) == 0) {
            arguments.push_back(0);
            rest.erase(0, 2);
        } else if (rest.rfind("s\"", 0) == 0) {
            std::size_t end = 2;
            while (end < rest.size() && rest[end] != '"') {
                end += rest[end] == '\\' ? 2U : 1U;
            }
            const std::wstring text = read_case_text(rest.substr(2, end - 2));
            BSTR b =
                SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
            allocated.push_back(b);
            arguments.push_back(reinterpret_cast<Word>(b));
            rest.erase(0, end + 1U);
        } else if (rest.front() == 'o') {
            VARIANT* v = make_variant(0, "", L"");
            variants.push_back(v);
            outputs.push_back(v);
            arguments.push_back(reinterpret_cast<Word>(v));
            rest.erase(0, 1);
        } else if (rest.front() == 'v') {
            std::size_t used = 0;
            const int vt = std::stoi(rest.substr(1), &used);
            std::size_t at = 1U + used;
            std::string number;
            std::wstring text;
            if (vt == 8200) {  // v8200:<count>"a|b|c"
                const auto quote = rest.find('"', at);
                number = rest.substr(at + 1U, quote - at - 1U);
                std::size_t end = quote + 1U;
                while (end < rest.size() && rest[end] != '"') {
                    end += rest[end] == '\\' ? 2U : 1U;
                }
                text =
                    read_case_text(rest.substr(quote + 1U, end - quote - 1U));
                at = end + 1U;
            } else if (at < rest.size() && rest[at] == ':') {
                const auto end = rest.find(' ', at);
                number = rest.substr(at + 1U, end - at - 1U);
                at = end == std::string::npos ? rest.size() : end;
            } else if (at < rest.size() && rest[at] == '"') {
                std::size_t end = at + 1U;
                while (end < rest.size() && rest[end] != '"') {
                    end += rest[end] == '\\' ? 2U : 1U;
                }
                text = read_case_text(rest.substr(at + 1U, end - at - 1U));
                at = end + 1U;
            }
            VARIANT* v = make_variant(vt, number, text);
            variants.push_back(v);
            arguments.push_back(reinterpret_cast<Word>(v));
            rest.erase(0, at);
        } else if (rest.front() == 'i') {
            std::size_t used = 0;
            const long value = std::stol(rest.substr(1), &used);
            arguments.push_back(
                static_cast<Word>(static_cast<std::intptr_t>(value)));
            rest.erase(0, used + 1U);
        } else if (rest.front() == 'd') {
            std::size_t used = 0;
            number_argument = std::stod(rest.substr(1), &used);
            doubles.push_back(number_argument);
            has_double = true;
            rest.erase(0, used + 1U);
        } else if (rest.rfind("a:", 0) == 0) {
            // a double array by reference: a:1,2,3
            const auto end = rest.find(' ');
            const std::string list = rest.substr(2U, end - 2U);
            std::vector<double> numbers;
            std::size_t from = 0;
            while (from <= list.size()) {
                const auto comma = list.find(',', from);
                numbers.push_back(std::stod(list.substr(
                    from, comma == std::string::npos ? comma : comma - from)));
                if (comma == std::string::npos) {
                    break;
                }
                from = comma + 1U;
            }
            SAFEARRAYBOUND bound = {static_cast<ULONG>(numbers.size()), 0};
            SAFEARRAY* const array = SafeArrayCreate(VT_R8, 1, &bound);
            for (LONG at = 0; at < static_cast<LONG>(numbers.size()); ++at) {
                SafeArrayPutElement(array, &at,
                                    &numbers[static_cast<std::size_t>(at)]);
            }
            auto* cell = new SAFEARRAY*(array);
            arguments.push_back(reinterpret_cast<Word>(cell));
            rest.erase(0, end == std::string::npos ? rest.size() : end);
        } else if (rest.rfind("p[", 0) == 0) {
            // a Variant array by reference: p[v8"a" v3:7]
            std::size_t close = 2U;
            bool quoted = false;
            while (close < rest.size() && (quoted || rest[close] != ']')) {
                if (rest[close] == '\\' && quoted) {
                    ++close;
                } else if (rest[close] == '"') {
                    quoted = !quoted;
                }
                ++close;
            }
            std::string inner = rest.substr(2U, close - 2U);
            std::vector<VARIANT*> members;
            while (!inner.empty()) {
                if (inner.front() == ' ') {
                    inner.erase(0, 1);
                    continue;
                }
                std::size_t used = 0;
                const int vt = std::stoi(inner.substr(1), &used);
                std::size_t at = 1U + used;
                std::string number;
                std::wstring text;
                if (at < inner.size() && inner[at] == ':') {
                    const auto stop = inner.find(' ', at);
                    number = inner.substr(at + 1U, stop - at - 1U);
                    at = stop == std::string::npos ? inner.size() : stop;
                } else if (at < inner.size() && inner[at] == '"') {
                    std::size_t end = at + 1U;
                    while (end < inner.size() && inner[end] != '"') {
                        end += inner[end] == '\\' ? 2U : 1U;
                    }
                    text = read_case_text(inner.substr(at + 1U, end - at - 1U));
                    at = end + 1U;
                }
                members.push_back(make_variant(vt, number, text));
                inner.erase(0, at);
            }
            SAFEARRAYBOUND bound = {static_cast<ULONG>(members.size()), 0};
            SAFEARRAY* const array = SafeArrayCreate(VT_VARIANT, 1, &bound);
            for (LONG at = 0; at < static_cast<LONG>(members.size()); ++at) {
                SafeArrayPutElement(array, &at,
                                    members[static_cast<std::size_t>(at)]);
            }
            auto* cell = new SAFEARRAY*(array);
            arguments.push_back(reinterpret_cast<Word>(cell));
            rest.erase(0, close + 1U);
        } else if (rest.front() == 'f') {
            std::size_t used = 0;
            singles.push_back(std::stof(rest.substr(1), &used));
            rest.erase(0, used + 1U);
        } else if (rest.front() == 'y') {
            std::size_t used = 0;  // y123456: a Currency by value (scaled)
            cys.push_back(std::stoll(rest.substr(1), &used));
            rest.erase(0, used + 1U);
        } else if (rest.front() == 'x') {
            std::size_t used = 0;
            const unsigned long value = std::stoul(rest.substr(1), &used, 16);
            arguments.push_back(static_cast<Word>(
                static_cast<std::intptr_t>(static_cast<long>(value))));
            rest.erase(0, used + 1U);
        } else {
            return "bad case";
        }
    }
    const FARPROC proc = GetProcAddress(g_library, name.c_str());
    if (proc == nullptr) {
        return "not exported";
    }
    if (kind == "c") {  // a Currency result (the scaled integer)
        long long answer = 0;
        if (cys.size() == 2) {
            answer = reinterpret_cast<long long(WINAPI*)(long long, long long)>(
                proc)(cys[0], cys[1]);
        } else if (cys.size() == 1 && arguments.size() == 1) {
            answer = reinterpret_cast<long long(WINAPI*)(Word, long long)>(
                proc)(arguments[0], cys[0]);
        } else if (cys.size() == 1) {
            answer =
                reinterpret_cast<long long(WINAPI*)(long long)>(proc)(cys[0]);
        } else {
            answer = reinterpret_cast<long long(WINAPI*)(Word)>(proc)(
                arguments.empty() ? 0 : arguments[0]);
        }
        for (BSTR b : allocated) {
            SysFreeString(b);
        }
        return "c" + std::to_string(answer);
    }
    if (kind == "f") {  // a Single result, shown as the bits of the Double
        float single = 0.0F;
        if (cys.size() == 1) {
            single = reinterpret_cast<float(WINAPI*)(long long)>(proc)(cys[0]);
        } else {
            single = reinterpret_cast<float(WINAPI*)(Word)>(proc)(
                arguments.empty() ? 0 : arguments[0]);
        }
        const double widened = single;
        std::uint64_t single_bits = 0;
        std::memcpy(&single_bits, &widened, sizeof(single_bits));
        for (BSTR b : allocated) {
            SysFreeString(b);
        }
        return "d:" + hex_text("x%016llX", single_bits);
    }
    if (kind == "D" && cys.size() == 1) {
        const double answer =
            reinterpret_cast<double(WINAPI*)(long long)>(proc)(cys[0]);
        std::uint64_t bits = 0;
        std::memcpy(&bits, &answer, sizeof(bits));
        return "d:" + hex_text("x%016llX", bits);
    }
    if (kind == "D") {  // a Double result
        // The Doubles come first, then the other arguments (Variants).
        const std::string shape = std::string(doubles.size(), 'd') +
                                  std::string(arguments.size(), 'w');
        double answer = 0.0;
        if (shape == "ddd") {
            answer = reinterpret_cast<double(WINAPI*)(double, double, double)>(
                proc)(doubles[0], doubles[1], doubles[2]);
        } else if (shape == "dddd") {
            answer = reinterpret_cast<double(WINAPI*)(
                double, double, double, double)>(proc)(doubles[0], doubles[1],
                                                       doubles[2], doubles[3]);
        } else if (shape == "ddddw") {
            answer = reinterpret_cast<double(WINAPI*)(double, double, double,
                                                      double, Word)>(proc)(
                doubles[0], doubles[1], doubles[2], doubles[3], arguments[0]);
        } else if (shape == "dddww") {
            answer = reinterpret_cast<double(WINAPI*)(double, double, double,
                                                      Word, Word)>(proc)(
                doubles[0], doubles[1], doubles[2], arguments[0], arguments[1]);
        } else if (shape == "ddddww") {
            answer = reinterpret_cast<double(WINAPI*)(
                double, double, double, double, Word, Word)>(proc)(
                doubles[0], doubles[1], doubles[2], doubles[3], arguments[0],
                arguments[1]);
        } else if (shape == "dddwww") {
            answer = reinterpret_cast<double(WINAPI*)(double, double, double,
                                                      Word, Word, Word)>(proc)(
                doubles[0], doubles[1], doubles[2], arguments[0], arguments[1],
                arguments[2]);
        } else if (name == "rtcNPV") {
            answer = reinterpret_cast<double(WINAPI*)(double, Word)>(proc)(
                doubles[0], arguments[0]);
        } else if (name == "rtcMIRR") {
            answer = reinterpret_cast<double(WINAPI*)(Word, double, double)>(
                proc)(arguments[0], doubles[0], doubles[1]);
        } else if (name == "rtcIRR") {
            answer = reinterpret_cast<double(WINAPI*)(Word, Word)>(proc)(
                arguments[0], arguments[1]);
        } else {
            answer = has_double ? reinterpret_cast<double(WINAPI*)(double)>(
                                      proc)(number_argument)
                                : reinterpret_cast<double(WINAPI*)(Word)>(proc)(
                                      arguments.empty() ? 0 : arguments[0]);
        }
        std::uint64_t bits = 0;
        std::memcpy(&bits, &answer, sizeof(bits));
        for (BSTR b : allocated) {
            SysFreeString(b);
        }
        return "d:" + hex_text("x%016llX", bits);
    }
    Word raw = 0;
    if (singles.size() == 1 && arguments.size() == 2) {
        // Choose(result, index as a Single, array)
        raw = reinterpret_cast<Word(WINAPI*)(Word, float, Word)>(proc)(
            arguments[0], singles[0], arguments[1]);
    } else if (cys.size() == 1 && arguments.empty()) {
        raw = reinterpret_cast<Word(WINAPI*)(long long)>(proc)(cys[0]);
    } else if (doubles.size() == 1 && arguments.empty()) {
        raw = reinterpret_cast<Word(WINAPI*)(double)>(proc)(doubles[0]);
    } else if (singles.size() == 1 && arguments.empty()) {
        raw = reinterpret_cast<Word(WINAPI*)(float)>(proc)(singles[0]);
    } else if (doubles.size() == 1 && arguments.size() == 3) {
        // A Double between the arguments: DateAdd(result, interval, number,
        // date).
        raw = reinterpret_cast<Word(WINAPI*)(Word, Word, double, Word)>(proc)(
            arguments[0], arguments[1], doubles[0], arguments[2]);
    } else {
        raw = call_with(proc, arguments);
    }
    std::string result;
    if (kind == "-") {  // the result is not checked
        result = "-";
    } else if (kind == "s") {
        const auto text = reinterpret_cast<BSTR>(raw);
        if (text == nullptr) {
            result = "s0";
        } else {
            const UINT bytes = SysStringByteLen(text);
            result = "s\"" + write_case_text(text, bytes / 2U) + "\"" +
                     ((bytes % 2U) != 0U ? "+1" : "");
            SysFreeString(text);
        }
    } else if (kind == "n") {
        result = folder_state();
    } else if (kind == "r") {
        const auto text = reinterpret_cast<BSTR>(raw);
        const std::wstring shown =
            hide_root(text == nullptr ? std::wstring()
                                      : std::wstring(text, SysStringLen(text)));
        result = "s\"" + write_case_text(shown.data(), shown.size()) + "\"";
        SysFreeString(text);
    } else if (kind == "R") {
        const VARIANT* const out = outputs.front();
        const std::wstring shown = hide_root(
            out->vt == VT_BSTR && out->bstrVal != nullptr
                ? std::wstring(out->bstrVal, SysStringLen(out->bstrVal))
                : std::wstring());
        result = "vt" + std::to_string(out->vt) + " s\"" +
                 write_case_text(shown.data(), shown.size()) + "\"";
    } else if (kind == "i") {
        result = "i" + std::to_string(static_cast<std::int32_t>(raw));
    } else if (kind == "w") {
        result = "w" + std::to_string(static_cast<std::int16_t>(raw));
    } else if (kind == "V") {
        result = format_variant(reinterpret_cast<const VARIANT*>(raw));
    } else if (kind == "O") {
        result = format_variant(outputs.front());
    } else {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "x%08X",
                      static_cast<unsigned>(raw));
        result = buffer;
    }
    for (BSTR b : allocated) {
        SysFreeString(b);
    }
    for (VARIANT* v : variants) {
        VariantClear(v);
        CoTaskMemFree(v);
    }
    return result;
}

void cases_family(const char* const cases, const char* const expected) {
    std::ifstream case_stream(cases);
    std::ifstream expected_stream(expected);
    std::string line;
    std::string want;
    int number = 0;
    while (std::getline(case_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "#fixture") {
            build_fixture();
            continue;
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        ++number;
        if (!std::getline(expected_stream, want)) {
            check(false, "the expected file is shorter than the cases");
            return;
        }
        if (!want.empty() && want.back() == '\r') {
            want.pop_back();
        }
        const std::string got = run_case(line);
        if (got != want) {
            std::fprintf(stderr, "case %d: %s\n  expected %s\n  got      %s\n",
                         number, line.c_str(), want.c_str(), got.c_str());
        }
        check(got == want, line.c_str());
    }
}

}  // namespace

int main(const int argument_count, const char* const arguments[]) {
    if (argument_count != 3 && argument_count != 5) {
        std::fprintf(stderr,
                     "usage: vbrun_helper_tests <dll> <family> "
                     "[<cases> <expected>]\n");
        return 2;
    }
    g_library = LoadLibraryA(arguments[1]);
    if (g_library == nullptr) {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 2;
    }
    const std::string family = arguments[2];
    if (family == "memory") {
        memory_family();
    } else if (family == "cases" && argument_count == 5) {
        cases_family(arguments[3], arguments[4]);
    } else {
        std::fprintf(stderr, "unknown family %s\n", arguments[2]);
        return 2;
    }
    if (g_failures != 0) {
        std::fprintf(stderr, "%d of %d checks failed\n", g_failures, g_checks);
        return 1;
    }
    std::printf("%s ok: %d checks\n", arguments[2], g_checks);
    return 0;
}
