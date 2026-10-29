// Behavior tests of the VBRUN helpers (REQ-0100 to REQ-0104). Usage:
//   vbrun_helper_tests <wfcvbrun.dll> <family>
// Each family calls exports of the library by name with the argument lists of
// the reference runtime and checks the results and the effects the reference
// has (probed on MSVBVM60 6.00.9848 with scripts/Probe-Vbrun.ps1).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
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
            v->fltVal = std::stof(number);
            break;
        case 5:
        case 7:
            v->dblVal = std::stod(number);
            break;
        case 6:
            v->cyVal.int64 = std::stoll(number);
            break;
        case 8:
            v->bstrVal =
                SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
            break;
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
        default:
            return reinterpret_cast<Word(WINAPI*)(
                Word, Word, Word, Word, Word)>(proc)(a[0], a[1], a[2], a[3],
                                                     a[4]);
    }
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
            if (at < rest.size() && rest[at] == ':') {
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
    const Word raw = call_with(proc, arguments);
    std::string result;
    if (kind == "s") {
        const auto text = reinterpret_cast<BSTR>(raw);
        if (text == nullptr) {
            result = "s0";
        } else {
            const UINT bytes = SysStringByteLen(text);
            result = "s\"" + write_case_text(text, bytes / 2U) + "\"" +
                     ((bytes % 2U) != 0U ? "+1" : "");
            SysFreeString(text);
        }
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
