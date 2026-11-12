// Width, FileAttr, Name, Lock and Unlock, Input$ and InputB$, and the fixed
// length String forms of Get and Put. The statements mirror the Visual Basic
// 6 program of scripts/vb6_harness_gen.py (experiments fileMisc and
// fileCtrl); the lines they produce are compared with the ones that program
// recorded (tests/cases/filemisc.expected).
//
// usage: vbrun_file_misc_tests <expected file>

#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void __cdecl __vbaFileOpen(DWORD flags, int record_length, int number,
                           BSTR name);
void WINAPI __vbaFileClose(SHORT number);
void __cdecl __vbaPrintFile(const unsigned char* descriptor, int number, ...);
void __cdecl __vbaWriteFile(const unsigned char* descriptor, int number, ...);
void WINAPI __vbaGet4(LONG code, void* value, LONG record, SHORT number);
void WINAPI __vbaPut4(LONG code, const void* value, LONG record, SHORT number);
void WINAPI __vbaPutFxStr4(LONG length, const wchar_t* text, LONG record,
                           SHORT number);
void WINAPI __vbaGetFxStr4(LONG length, wchar_t* text, LONG record,
                           SHORT number);
LONG WINAPI rtcFileLength(SHORT number);
void WINAPI rtcFileWidth(SHORT number, SHORT width);
LONG WINAPI rtcFileAttributes(SHORT number, SHORT kind);
BSTR WINAPI rtcInputCount(LONG count, SHORT number);
BSTR WINAPI rtcInputCharCount(LONG count, SHORT number);
VARIANT* WINAPI rtcInputCountVar(VARIANT* result, LONG count, SHORT number);
VARIANT* WINAPI rtcInputCharCountVar(VARIANT* result, LONG count, SHORT number);
void WINAPI __vbaNameFile(BSTR old_name, BSTR new_name);
void WINAPI __vbaFileLock(DWORD flags, LONG to, LONG from, SHORT number);
SHORT WINAPI rtcEndOfFile(SHORT number);
LONG WINAPI rtcFileLocation(SHORT number);
LONG WINAPI rtcFileSeek(SHORT number);
void WINAPI __vbaLineInputStr(BSTR* text, SHORT number);
}

namespace {

constexpr DWORD kInput = 1;
constexpr DWORD kOutput = 2;
constexpr DWORD kRandom = 4;
constexpr DWORD kBinary = 0x20;
constexpr DWORD kReadWrite = 0x300;
constexpr DWORD kShared = 0x4000;

std::string g_lines;

void record(const std::string& label, const std::string& value) {
    g_lines += label + " => " + value + "\n";
}

BSTR text(const wchar_t* const value) {
    return SysAllocString(value);
}

void open(const DWORD mode, const int number, const char* const name,
          const int length = -1) {
    const std::wstring wide(name, name + std::strlen(name));
    BSTR path = SysAllocString(wide.c_str());
    __vbaFileOpen(mode, length, number, path);
    SysFreeString(path);
}

std::string read_file(const char* const name) {
    std::ifstream in(name, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

void write_file(const char* const name, const std::string& bytes) {
    std::ofstream out(name, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool exists(const char* const name) {
    return GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES;
}

// The name a file has on disk, as Dir$ reports it.
std::string disk_name(const char* const name) {
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(name, &found);
    if (handle == INVALID_HANDLE_VALUE) {
        return "";
    }
    FindClose(handle);
    return found.cFileName;
}

std::string hex_of(const std::string& bytes) {
    std::string out;
    char pair[4];
    for (const char c : bytes) {
        std::snprintf(pair, sizeof(pair), "%02X",
                      static_cast<unsigned char>(c));
        out += pair;
    }
    return out;
}

void dump(const std::string& label, const char* const name) {
    record(label, hex_of(read_file(name)));
}

// Runs `call` and returns the error it raised.
template <typename Call>
int error_of(Call call) {
    vbrun::clear_error();
    call();
    return vbrun::last_error();
}

std::string err_text() {
    return " err " + std::to_string(vbrun::last_error());
}

std::string narrow(const BSTR value) {
    std::string out;
    for (UINT i = 0; value != nullptr && i < SysStringLen(value); ++i) {
        out.push_back(value[i] < 128 ? static_cast<char>(value[i]) : '?');
    }
    return out;
}

constexpr unsigned char kInteger = 0x02;
constexpr unsigned char kDouble = 0x05;
constexpr unsigned char kDate = 0x07;
constexpr unsigned char kString = 0x08;
constexpr unsigned char kBoolean = 0x0B;
constexpr unsigned char kVariant = 0x0C;
constexpr unsigned char kSpc = 0x20;
constexpr unsigned char kTab = 0x21;
constexpr unsigned char kComma = 0x00;
constexpr unsigned char kSemicolon = 0x40;
constexpr unsigned char kNewline = 0x80;

template <typename... Values>
void print(const unsigned char* const descriptor, const Values... values) {
    __vbaPrintFile(descriptor, 21, values...);
}

void width_statements() {
    const unsigned char d_str[] = {1, 0, kString | kNewline};
    const unsigned char d_hold[] = {1, 0, kString | kSemicolon};
    const unsigned char d_three[] = {3, 0, kString | kSemicolon,
                                     kString | kSemicolon, kString | kNewline};
    const unsigned char d_comma[] = {2, 0, kString | kComma,
                                     kString | kNewline};
    const unsigned char d_semi2[] = {2, 0, kString | kSemicolon,
                                     kString | kNewline};
    open(kOutput, 21, "wfcio_m.tmp");
    record("fa out", std::to_string(rtcFileAttributes(21, 1)) + " err " +
                         std::to_string(vbrun::last_error()));
    record("fa handle", "False err " + std::to_string(error_of(
                                           [] { rtcFileAttributes(21, 2); })));
    record("fa 3",
           "err " + std::to_string(error_of([] { rtcFileAttributes(21, 3); })));
    record("fa 0",
           "err " + std::to_string(error_of([] { rtcFileAttributes(21, 0); })));
    record("fa closed",
           "err " + std::to_string(error_of([] { rtcFileAttributes(99, 1); })));
    record("fa zero",
           "err " + std::to_string(error_of([] { rtcFileAttributes(0, 1); })));
    record("fa big", "err " + std::to_string(
                                  error_of([] { rtcFileAttributes(513, 1); })));
    record("width err", std::to_string(error_of([] { rtcFileWidth(21, 10); })));
    const unsigned char d_five[] = {5,
                                    0,
                                    kInteger | kSemicolon,
                                    kInteger | kSemicolon,
                                    kInteger | kSemicolon,
                                    kInteger | kSemicolon,
                                    kInteger | kNewline};
    const unsigned char w_three[] = {3, 0, kString | kComma, kString | kComma,
                                     kString | kNewline};
    const unsigned char d_tabs[] = {4,
                                    0,
                                    kTab | kSemicolon,
                                    kString | kSemicolon,
                                    kTab | kSemicolon,
                                    kString | kNewline};
    print(d_three, text(L"abcdef"), text(L"ghijkl"), text(L"mn"));
    print(d_str, text(L"abcdefghijklmn"));
    print(d_five, 1, 2, 3, 4, 5);
    print(d_comma, text(L"abcde"), text(L"fg"));
    print(d_hold, text(L"abcdef"));
    print(d_hold, text(L"ghijkl"));
    print(d_str, text(L"mn"));
    __vbaWriteFile(w_three, 21, text(L"abc"), text(L"defgh"), text(L"ij"));
    print(d_tabs, 5, text(L"ab"), 9, text(L"cd"));
    const unsigned char d_spc[] = {2, 0, kSpc | kSemicolon, kString | kNewline};
    print(d_spc, 8, text(L"abcd"));
    print(d_str, text(L"x"));
    __vbaFileClose(21);
    dump("width10", "wfcio_m.tmp");

    open(kOutput, 21, "wfcio_m.tmp");
    record("width0 err", std::to_string(error_of([] { rtcFileWidth(21, 0); })));
    record("width255 err",
           std::to_string(error_of([] { rtcFileWidth(21, 255); })));
    record("width256 err",
           std::to_string(error_of([] { rtcFileWidth(21, 256); })));
    record("width-1 err",
           std::to_string(error_of([] { rtcFileWidth(21, -1); })));
    record("width closed err",
           std::to_string(error_of([] { rtcFileWidth(99, 10); })));
    rtcFileWidth(21, 4);
    print(d_three, text(L"ab"), text(L"cd"), text(L"ef"));
    print(d_str, text(L"abcdefgh"));
    print(d_semi2, text(L"ab"), text(L"cde"));
    rtcFileWidth(21, 0);
    print(d_str,
          text(L"abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz"
               L"0123456789abcdefghijklmnopqrstuvwxyz0123456789"));
    rtcFileWidth(21, 3);
    const unsigned char d_ints3[] = {3, 0, kInteger | kSemicolon,
                                     kInteger | kSemicolon,
                                     kInteger | kNewline};
    print(d_ints3, 12, 34, 56);
    const unsigned char d_four[] = {4,
                                    0,
                                    kString | kSemicolon,
                                    kString | kSemicolon,
                                    kString | kSemicolon,
                                    kString | kNewline};
    print(d_four, text(L"a"), text(L"b"), text(L"c"), text(L"d"));
    print(d_comma, text(L"a"), text(L"b"));
    __vbaFileClose(21);
    dump("width4", "wfcio_m.tmp");
}

struct Held {
    std::string shown;
    long length = 0;
    long bytes = 0;
};
Held g_held;

bool take(const BSTR value) {
    if (value == nullptr) {
        return false;
    }
    g_held.shown = narrow(value);
    g_held.length = static_cast<long>(SysStringLen(value));
    g_held.bytes = static_cast<long>(SysStringByteLen(value));
    SysFreeString(value);
    return true;
}

int input_error(BSTR(WINAPI* call)(LONG, SHORT), const LONG count,
                const SHORT number) {
    vbrun::clear_error();
    take(call(count, number));
    return vbrun::last_error();
}

std::string held_text() {
    return g_held.shown + "|" + std::to_string(g_held.length);
}

std::string yes_no(const bool value) {
    return value ? "True" : "False";
}

void input_statements() {
    const unsigned char d_hold[] = {1, 0, kString | kSemicolon};
    open(kOutput, 21, "wfcio_m.tmp");
    print(d_hold, text(L"Hello World and more text for Input"));
    __vbaFileClose(21);
    open(kInput, 22, "wfcio_m.tmp");
    int err = input_error(rtcInputCharCount, 5, 22);
    record("in5", held_text() + " err " + std::to_string(err));
    err = input_error(rtcInputCharCount, 0, 22);
    record("in0", held_text() + " err " + std::to_string(err));
    err = input_error(rtcInputCharCount, -1, 22);
    record("in-1", held_text() + " err " + std::to_string(err));
    VARIANT v;
    VariantInit(&v);
    vbrun::clear_error();
    rtcInputCharCountVar(&v, 3, 22);
    record("inv", narrow(v.bstrVal) + "|String err " +
                      std::to_string(vbrun::last_error()));
    VariantClear(&v);
    err = input_error(rtcInputCount, 4, 22);
    record("inb", g_held.shown + "|" + std::to_string(g_held.length) + "|" +
                      std::to_string(g_held.bytes) + " err " +
                      std::to_string(err));
    err = input_error(rtcInputCount, 3, 22);
    record("inb3",
           std::to_string(g_held.bytes) + " err " + std::to_string(err));
    vbrun::clear_error();
    rtcInputCountVar(&v, 2, 22);
    record("inbv", "String|" + std::to_string(SysStringByteLen(v.bstrVal)) +
                       " err " + std::to_string(vbrun::last_error()));
    VariantClear(&v);
    record("pos", std::to_string(rtcFileLocation(22)) + "|" +
                      std::to_string(rtcFileSeek(22)) + "|" +
                      yes_no(rtcEndOfFile(22) != 0));
    err = input_error(rtcInputCharCount, 1000, 22);
    record("inbig", std::to_string(g_held.length) + " err " +
                        std::to_string(err) + " eof " +
                        yes_no(rtcEndOfFile(22) != 0));
    err = input_error(rtcInputCharCount, 1, 22);
    record("inpast",
           std::to_string(g_held.length) + " err " + std::to_string(err));
    __vbaFileClose(22);
    open(kBinary, 22, "wfcio_m.tmp");
    err = input_error(rtcInputCharCount, 5, 22);
    record("binin5", g_held.shown + " err " + std::to_string(err));
    err = input_error(rtcInputCharCount, 1000, 22);
    record("binbig", std::to_string(g_held.length) + " err " +
                         std::to_string(err) + " eof " +
                         yes_no(rtcEndOfFile(22) != 0));
    err = input_error(rtcInputCharCount, 1, 22);
    record("binpast",
           std::to_string(g_held.length) + " err " + std::to_string(err));
    __vbaFileClose(22);
    write_file("wfcio_m.tmp", std::string("\xC8\x01\x96", 3) + "z");
    open(kInput, 22, "wfcio_m.tmp");
    vbrun::clear_error();
    BSTR high = rtcInputCharCount(4, 22);
    record("inhigh", std::to_string(high[0]) + "|" + std::to_string(high[1]) +
                         "|" + std::to_string(high[2]) + "|" +
                         std::to_string(SysStringLen(high)) + " err " +
                         std::to_string(vbrun::last_error()));
    SysFreeString(high);
    __vbaFileClose(22);
    open(kOutput, 22, "wfcio_m.tmp");
    record("inout err", std::to_string(input_error(rtcInputCharCount, 1, 22)));
    __vbaFileClose(22);
    open(kInput, 22, "wfcio_m.tmp");
    const auto name = [](const wchar_t* const from, const wchar_t* const to) {
        return error_of([&] { __vbaNameFile(text(from), text(to)); });
    };
    record("name open err",
           std::to_string(name(L"wfcio_m.tmp", L"wfcio_n.tmp")));
    __vbaFileClose(22);
    const int renamed = name(L"wfcio_m.tmp", L"wfcio_n.tmp");
    record("name ok err", std::to_string(renamed) + " old " +
                              yes_no(!exists("wfcio_m.tmp")) + " new " +
                              yes_no(exists("wfcio_n.tmp")));
    record("name missing err",
           std::to_string(name(L"wfcio_m.tmp", L"wfcio_o.tmp")));
    write_file("wfcio_m.tmp", "");
    record("name exists err",
           std::to_string(name(L"wfcio_m.tmp", L"wfcio_n.tmp")));
    record("name nodir err",
           std::to_string(name(L"wfcio_m.tmp", L"nodir_x/wfcio_o.tmp")));
    record("name empty err", std::to_string(name(L"wfcio_m.tmp", L"")));
    record("name same err",
           std::to_string(name(L"wfcio_m.tmp", L"wfcio_m.tmp")));
    const int cased = name(L"wfcio_n.tmp", L"wfcio_N.tmp");
    record("name case err",
           std::to_string(cased) + " " + disk_name("wfcio_n.tmp"));
    record("name srcdir err",
           std::to_string(name(L"nodir_x/a.tmp", L"wfcio_o.tmp")));
    const int moved = name(L"wfcio_m.tmp", L"wfcio_o.tmp");
    record("name move err",
           std::to_string(moved) + " " + disk_name("wfcio_o.tmp"));
    DeleteFileA("wfcio_m.tmp");
    DeleteFileA("wfcio_n.tmp");
    DeleteFileA("wfcio_o.tmp");
    DeleteFileA("wfcio_N.tmp");
}

void width_more_statements() {
    const unsigned char d_str[] = {1, 0, kString | kNewline};
    const unsigned char d_comma[] = {2, 0, kString | kComma,
                                     kString | kNewline};
    const unsigned char d_spc[] = {2, 0, kSpc | kSemicolon, kString | kNewline};
    const unsigned char d_tab[] = {2, 0, kTab | kSemicolon, kString | kNewline};
    const unsigned char d_stab[] = {3, 0, kString | kSemicolon,
                                    kTab | kSemicolon, kString | kNewline};
    const unsigned char d_two[] = {2, 0, kString | kSemicolon,
                                   kString | kNewline};
    const unsigned char d_three[] = {3, 0, kString | kSemicolon,
                                     kString | kSemicolon, kString | kNewline};
    const unsigned char d_hold[] = {1, 0, kString | kSemicolon};
    open(kOutput, 21, "wfcio_m.tmp");
    rtcFileWidth(21, 14);
    print(d_comma, text(L"abcde"), text(L"fg"));
    print(d_comma, text(L"abcdefghijklm"), text(L"n"));
    print(d_comma, text(L"abcdefghijklmn"), text(L"o"));
    print(d_spc, 12, text(L"a"));
    print(d_spc, 13, text(L"a"));
    print(d_tab, 12, text(L"abc"));
    print(d_stab, text(L"ab"), 14, text(L"c"));
    print(d_stab, text(L"ab"), 15, text(L"c"));
    print(d_spc, 20, text(L"a"));
    print(d_stab, text(L"a"), 30, text(L"b"));
    rtcFileWidth(21, 5);
    const unsigned char d_int[] = {2, 0, kString | kSemicolon,
                                   kInteger | kNewline};
    print(d_int, text(L"abc"), 1);
    const unsigned char d_bool[] = {2, 0, kString | kSemicolon,
                                    kBoolean | kNewline};
    print(d_bool, text(L"ab"), -1);
    const unsigned char d_dbl[] = {2, 0, kString | kSemicolon,
                                   kDouble | kNewline};
    print(d_dbl, text(L"ab"), 1.5);
    const unsigned char d_date[] = {1, 0, kDate | kNewline};
    print(d_date, 36527.0);
    const unsigned char d_var3[] = {3, 0, kString | kSemicolon,
                                    kVariant | kSemicolon, kString | kNewline};
    VARIANT null_value;
    VariantInit(&null_value);
    null_value.vt = VT_NULL;
    print(d_var3, text(L"ab"), &null_value, text(L"cd"));
    VARIANT empty_value;
    VariantInit(&empty_value);
    print(d_var3, text(L"abcd"), &empty_value, text(L"e"));
    print(d_three, text(L"abcdef"), text(L""), text(L"g"));
    print(d_three, text(L"abcd"), text(L""), text(L"e"));
    print(d_three, text(L"a"), text(L""), text(L"bcdefg"));
    static_cast<void>(d_str);
    static_cast<void>(d_two);
    static_cast<void>(d_hold);
    __vbaFileClose(21);
    dump("width14", "wfcio_m.tmp");
}

BSTR g_buffer = nullptr;

BSTR string_of(const wchar_t* const value) {
    return SysAllocString(value);
}

std::string fixed_of(const wchar_t* const chars, const int length) {
    std::string out;
    for (int i = 0; i < length; ++i) {
        out.push_back(chars[i] < 128 ? static_cast<char>(chars[i]) : '?');
    }
    return out;
}

void lock_statements() {
    constexpr DWORD kUnlock = 1;
    constexpr DWORD kRange = 2;
    constexpr DWORD kOne = 4;
    const auto lock = [](const DWORD flags, const LONG to, const LONG from,
                         const SHORT number) {
        return std::to_string(
            error_of([&] { __vbaFileLock(flags, to, from, number); }));
    };
    DeleteFileA("wfcio_m.tmp");
    open(kBinary | kReadWrite | kShared, 22, "wfcio_m.tmp");
    vbrun::clear_error();
    open(kBinary | kReadWrite | kShared, 23, "wfcio_m.tmp");
    record("shared open err", std::to_string(vbrun::last_error()));
    BSTR whole = string_of(L"0123456789abcdefghij");
    __vbaPut4(0, &whole, 1, 22);
    record("lock range err", lock(kRange, 6, 3, 22));
    BSTR two = string_of(L"  ");
    vbrun::clear_error();
    __vbaGet4(0, &two, 1, 23);
    record("get free",
           narrow(two) + " err " + std::to_string(vbrun::last_error()));
    record(
        "get locked",
        "err " + std::to_string(error_of([&] { __vbaGet4(0, &two, 4, 23); })));
    record(
        "put locked",
        "err " + std::to_string(error_of([&] { __vbaPut4(0, &two, 4, 23); })));
    vbrun::clear_error();
    __vbaGet4(0, &two, 4, 22);
    record("get own",
           narrow(two) + " err " + std::to_string(vbrun::last_error()));
    record("unlock err", lock(kRange | kUnlock, 6, 3, 22));
    vbrun::clear_error();
    __vbaGet4(0, &two, 4, 23);
    record("get unlocked",
           narrow(two) + " err " + std::to_string(vbrun::last_error()));
    record("unlock again err", lock(kRange | kUnlock, 6, 3, 22));
    record("lock one err", lock(kOne, 0, 2, 22));
    record(
        "get one locked",
        "err " + std::to_string(error_of([&] { __vbaGet4(0, &two, 2, 23); })));
    lock(kOne | kUnlock, 0, 2, 22);
    record("lock whole err", lock(0, 0, 0, 22));
    record(
        "get whole locked",
        "err " + std::to_string(error_of([&] { __vbaGet4(0, &two, 15, 23); })));
    record("unlock whole err", lock(kUnlock, 0, 0, 22));
    record("lock zero err", lock(kOne, 0, 0, 22));
    record("lock backwards err", lock(kRange, 3, 6, 22));
    record("lock huge err", lock(kOne, 0, 2147483647, 22));
    lock(kOne | kUnlock, 0, 2147483647, 22);
    lock(kRange, 2, 1, 22);
    record("lock overlap err", lock(kRange, 3, 2, 22));
    lock(kRange | kUnlock, 2, 1, 22);
    lock(kRange | kUnlock, 3, 2, 22);
    record("lock closed err", lock(0, 0, 0, 99));
    __vbaFileClose(22);
    __vbaFileClose(23);
    open(kRandom | kShared, 22, "wfcio_m.tmp", 4);
    open(kRandom | kShared, 23, "wfcio_m.tmp", 4);
    LONG number = 0;
    lock(kOne, 0, 2, 22);
    record("rnd get locked", "err " + std::to_string(error_of([&] {
                                 __vbaGet4(4, &number, 2, 23);
                             })));
    record("rnd get free", "err " + std::to_string(error_of([&] {
                               __vbaGet4(4, &number, 3, 23);
                           })));
    lock(kRange, 6, 5, 22);
    record("rnd range locked", "err " + std::to_string(error_of([&] {
                                   __vbaGet4(4, &number, 6, 23);
                               })));
    record("rnd range free", "err " + std::to_string(error_of([&] {
                                 __vbaGet4(4, &number, 7, 23);
                             })));
    lock(kOne | kUnlock, 0, 2, 22);
    lock(kRange | kUnlock, 6, 5, 22);
    __vbaFileClose(22);
    __vbaFileClose(23);
    open(kInput | kShared, 22, "wfcio_m.tmp");
    open(kInput | kShared, 23, "wfcio_m.tmp");
    record("seq lock err", lock(0, 0, 0, 22));
    record("seq read locked",
           "err " + std::to_string(input_error(rtcInputCharCount, 1, 23)));
    record("seq lock range err", lock(kRange, 2, 1, 22));
    record("seq unlock err", lock(kUnlock, 0, 0, 22));
    __vbaFileClose(22);
    __vbaFileClose(23);
    DeleteFileA("wfcio_m.tmp");
}

void fixed_statements() {
    open(kBinary, 25, "wfcio_m.tmp");
    __vbaPutFxStr4(6, L"abcdef", 0, 25);
    __vbaPutFxStr4(3, L"xyz", 0, 25);
    __vbaPutFxStr4(3, L"xyz", 2, 25);
    __vbaFileClose(25);
    dump("fxput", "wfcio_m.tmp");
    open(kBinary, 25, "wfcio_m.tmp");
    wchar_t six[6];
    wchar_t three[3];
    __vbaGetFxStr4(6, six, 0, 25);
    record("fxget", fixed_of(six, 6));
    __vbaGetFxStr4(3, three, 0, 25);
    record("fxget2", fixed_of(three, 3));
    __vbaGetFxStr4(3, three, 4, 25);
    record("fxget3", fixed_of(three, 3));
    vbrun::clear_error();
    __vbaGetFxStr4(6, six, 8, 25);
    record("fxget past",
           fixed_of(six, 6) + "|6 err " + std::to_string(vbrun::last_error()));
    __vbaFileClose(25);
    open(kRandom, 25, "wfcio_m.tmp", 8);
    __vbaPutFxStr4(6, L"ABCDEF", 2, 25);
    __vbaPutFxStr4(6, L"ABCDEF", 0, 25);
    __vbaPutFxStr4(3, L"xy ", 1, 25);
    record("rnd lof", std::to_string(rtcFileLength(25)));
    __vbaFileClose(25);
    dump("fxrnd", "wfcio_m.tmp");
    open(kRandom, 25, "wfcio_m.tmp", 8);
    __vbaGetFxStr4(6, six, 2, 25);
    record("rfxget", fixed_of(six, 6));
    __vbaGetFxStr4(6, six, 0, 25);
    record("rfxget2", fixed_of(six, 6));
    __vbaGetFxStr4(3, three, 1, 25);
    record("rfxget3", fixed_of(three, 3));
    vbrun::clear_error();
    __vbaGetFxStr4(6, six, 9, 25);
    record("rfxget past",
           fixed_of(six, 6) + " err " + std::to_string(vbrun::last_error()));
    __vbaFileClose(25);
    open(kRandom, 25, "wfcio_m.tmp", 4);
    record("rfx long", "err " + std::to_string(error_of([&] {
                           __vbaPutFxStr4(6, L"ABCDEF", 1, 25);
                       })));
    record("rfx long get", "err " + std::to_string(error_of([&] {
                               __vbaGetFxStr4(6, six, 1, 25);
                           })));
    __vbaFileClose(25);
    DeleteFileA("wfcio_m.tmp");
}

// Control bytes in text files: the statements of experiments fileCtrl and
// fileCtrl2.
std::string g_text;  // the String the Visual Basic program keeps (cb)

std::string codes_of(const std::string& chars) {
    std::string out;
    for (const unsigned char c : chars) {
        out += std::to_string(c) + ",";
    }
    return out;
}

// Input$(count, #number) into g_text; the error of the call.
int input_into(const LONG count, const SHORT number, const bool bytes) {
    vbrun::clear_error();
    BSTR got =
        bytes ? rtcInputCount(count, number) : rtcInputCharCount(count, number);
    if (got != nullptr) {
        g_text.clear();
        for (UINT i = 0; i < SysStringLen(got); ++i) {
            g_text.push_back(static_cast<char>(got[i]));
        }
        SysFreeString(got);
    }
    return vbrun::last_error();
}

int line_into(const SHORT number) {
    vbrun::clear_error();
    BSTR got = nullptr;
    __vbaLineInputStr(&got, number);
    if (got != nullptr) {
        g_text.clear();
        for (UINT i = 0; i < SysStringLen(got); ++i) {
            g_text.push_back(static_cast<char>(got[i]));
        }
        SysFreeString(got);
    }
    return vbrun::last_error();
}

std::string where_text(const SHORT number) {
    return "eof " + yes_no(rtcEndOfFile(number) != 0) + " seek " +
           std::to_string(rtcFileSeek(number)) + " loc " +
           std::to_string(rtcFileLocation(number));
}

void control_statements() {
    const std::string first =
        std::string("ab\0cd", 5) + "\x1A" + "ef\r\ngh" + "\x1A" + "ij";
    write_file("wfcio_c.tmp", first);
    record("len", std::to_string(first.size()));
    open(kInput, 22, "wfcio_c.tmp");
    int err = input_into(5, 22, false);
    record("in5", codes_of(g_text) + " len " + std::to_string(g_text.size()) +
                      " err " + std::to_string(err) + " loc " +
                      std::to_string(rtcFileLocation(22)) + " seek " +
                      std::to_string(rtcFileSeek(22)) + " eof " +
                      yes_no(rtcEndOfFile(22) != 0));
    err = input_into(3, 22, false);
    record("in3", codes_of(g_text) + " len " + std::to_string(g_text.size()) +
                      " err " + std::to_string(err) + " seek " +
                      std::to_string(rtcFileSeek(22)) + " eof " +
                      yes_no(rtcEndOfFile(22) != 0));
    err = input_into(1, 22, false);
    record("in1", std::to_string(g_text.size()) + " err " +
                      std::to_string(err) + " seek " +
                      std::to_string(rtcFileSeek(22)) + " eof " +
                      yes_no(rtcEndOfFile(22) != 0));
    __vbaFileClose(22);
    g_text.clear();
    open(kInput, 22, "wfcio_c.tmp");
    err = line_into(22);
    record("line1", codes_of(g_text) + " err " + std::to_string(err) +
                        " seek " + std::to_string(rtcFileSeek(22)) + " eof " +
                        yes_no(rtcEndOfFile(22) != 0));
    err = line_into(22);
    record("line2", std::to_string(g_text.size()) + " err " +
                        std::to_string(err) + " seek " +
                        std::to_string(rtcFileSeek(22)) + " eof " +
                        yes_no(rtcEndOfFile(22) != 0));
    __vbaFileClose(22);
    open(kBinary, 22, "wfcio_c.tmp");
    err = input_into(13, 22, false);
    record("bin13", codes_of(g_text) + " len " + std::to_string(g_text.size()) +
                        " err " + std::to_string(err) + " seek " +
                        std::to_string(rtcFileSeek(22)) + " eof " +
                        yes_no(rtcEndOfFile(22) != 0));
    err = input_into(20, 22, false);
    record("binrest", std::to_string(g_text.size()) + " err " +
                          std::to_string(err) + " eof " +
                          yes_no(rtcEndOfFile(22) != 0));
    __vbaFileClose(22);
    open(kRandom, 22, "wfcio_c.tmp", 4);
    record("rndin err", std::to_string(input_into(1, 22, false)));
    __vbaFileClose(22);
    DeleteFileA("wfcio_c.tmp");

    for (int k = 1; k <= 6; ++k) {
        std::string bytes;
        switch (k) {
            case 1:
                bytes = std::string("abc\x1A") + "def";
                break;
            case 2:
                bytes = std::string("\x1A") + "abc";
                break;
            case 3:
                bytes = std::string("abc\x1A");
                break;
            case 4:
                bytes = std::string("ab\0cd", 5);
                break;
            case 5:
                bytes = std::string("abc\x1A") + "def\r\nghi\r\n";
                break;
            default:
                bytes = std::string("\0\0", 2);
                break;
        }
        write_file("wfcio_d.tmp", bytes);
        const std::string tag = "case" + std::to_string(k);
        open(kInput, 22, "wfcio_d.tmp");
        record(tag + " start",
               where_text(22) + " lof " + std::to_string(bytes.size()));
        err = line_into(22);
        record(tag + " line1", codes_of(g_text) + " err " +
                                   std::to_string(err) + " " + where_text(22));
        err = line_into(22);
        record(tag + " line2", std::to_string(g_text.size()) + " err " +
                                   std::to_string(err) + " " + where_text(22));
        __vbaFileClose(22);
        open(kInput, 22, "wfcio_d.tmp");
        err = input_into(2, 22, false);
        record(tag + " in2", std::to_string(g_text.size()) + " err " +
                                 std::to_string(err) + " " + where_text(22));
        err = input_into(1, 22, false);
        record(tag + " in1", std::to_string(g_text.size()) + " err " +
                                 std::to_string(err) + " " + where_text(22));
        __vbaFileClose(22);
    }
    DeleteFileA("wfcio_d.tmp");
}

}  // namespace

int main(const int argument_count, const char* const arguments[]) {
    if (argument_count != 2) {
        std::fprintf(stderr, "usage: vbrun_file_misc_tests <expected>\n");
        return 2;
    }
    // The Visual Basic program had its own file open as #1.
    write_file("wfcio_own.tmp", "");
    open(kOutput, 1, "wfcio_own.tmp");
    width_statements();
    input_statements();
    width_more_statements();
    lock_statements();
    fixed_statements();
    control_statements();
    std::ofstream actual("filemisc.actual", std::ios::binary);
    actual << g_lines;
    actual.close();
    std::string expected = read_file(arguments[1]);
    if (expected != g_lines) {
        std::fprintf(stderr, "filemisc: output differs from %s\n",
                     arguments[1]);
        return 1;
    }
    return 0;
}
