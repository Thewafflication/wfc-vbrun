// The open-file table of the VBRUN helpers (Open, Close, FreeFile, LOF, Loc,
// Seek, EOF, Line Input). The sequence mirrors the Visual Basic 6 program of
// scripts/vb6_harness_gen.py (experiment fileTable); the lines it prints are
// compared with the ones that program wrote (tests/cases/openfiles.expected).
//
// usage: vbrun_file_tests <expected file>

#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void __cdecl __vbaFileOpen(DWORD flags, int record_length, int number,
                           BSTR name);
void WINAPI __vbaFileClose(SHORT number);
void __cdecl __vbaFileCloseAll();
SHORT WINAPI rtcFreeFile(const VARIANT* range);
LONG WINAPI rtcFileLength(SHORT number);
LONG WINAPI rtcFileSeek(SHORT number);
LONG WINAPI rtcFileLocation(SHORT number);
void WINAPI __vbaFileSeek(LONG position, SHORT number);
SHORT WINAPI rtcEndOfFile(SHORT number);
void WINAPI __vbaLineInputStr(BSTR* text, SHORT number);
}

namespace {

std::string g_lines;

void record(const std::string& label, const std::string& value) {
    g_lines += label + " => " + value + "\n";
}

void record(const std::string& label, const long value) {
    record(label, std::to_string(value));
}

void record_bool(const std::string& label, const SHORT value) {
    record(label, std::string(value != 0 ? "True" : "False"));
}

BSTR text(const wchar_t* const value) {
    return SysAllocString(value);
}

void open(const DWORD mode, const int number, const wchar_t* const name,
          const int length = -1) {
    BSTR path = text(name);
    __vbaFileOpen(mode, length, number, path);
    SysFreeString(path);
}

void write_file(const char* const name, const std::string& bytes) {
    std::ofstream out(name, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string line_of(const SHORT number) {
    BSTR value = nullptr;
    __vbaLineInputStr(&value, number);
    std::string narrow;
    for (UINT i = 0; value != nullptr && i < SysStringLen(value); ++i) {
        narrow.push_back(static_cast<char>(value[i]));
    }
    SysFreeString(value);
    return "[" + narrow + "]";
}

// Runs `call` and returns the error it raised.
template <typename Call>
int error_of(Call call) {
    vbrun::clear_error();
    call();
    return vbrun::last_error();
}

VARIANT missing() {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_ERROR;
    value.scode = static_cast<SCODE>(0x80020004U);
    return value;
}

VARIANT integer(const int number) {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = number;
    return value;
}

}  // namespace

int main(const int argument_count, const char* const arguments[]) {
    if (argument_count != 2) {
        std::fprintf(stderr, "usage: vbrun_file_tests <expected>\n");
        return 2;
    }
    // The Visual Basic program had its own file open as #1.
    write_file("wfcio_own.tmp", "");
    open(2, 1, L"wfcio_own.tmp");
    const VARIANT none = missing();
    const VARIANT one = integer(1);

    record("free0", rtcFreeFile(&none));
    record("openOut err", error_of([] { open(2, 21, L"wfcio_a.tmp"); }));
    record("free1", rtcFreeFile(&none));
    record("freeRange2", rtcFreeFile(&one));
    record("out lof", rtcFileLength(21));
    record("out loc", rtcFileLocation(21));
    record("out seek", rtcFileSeek(21));
    record_bool("out eof", rtcEndOfFile(21));
    record("close err", error_of([] { __vbaFileClose(21); }));
    record("lof closed err", error_of([] { rtcFileLength(21); }));
    record("close again err", error_of([] { __vbaFileClose(21); }));

    // The file the Visual Basic program had written.
    write_file("wfcio_a.tmp",
               "hello\r\nworld\r\n 12 -3.5 True\r\n\"q\",7,2.5\r\n");
    open(1, 22, L"wfcio_a.tmp");
    record("in free", rtcFreeFile(&none));
    record("in lof", rtcFileLength(22));
    record("in loc", rtcFileLocation(22));
    record("in seek", rtcFileSeek(22));
    record_bool("in eof", rtcEndOfFile(22));
    record("line1", line_of(22));
    record("in loc1", rtcFileLocation(22));
    record("in seek1", rtcFileSeek(22));
    record_bool("in eof1", rtcEndOfFile(22));
    record("line2", line_of(22));
    record("line3", line_of(22));
    record("line4", line_of(22));
    record_bool("in eof4", rtcEndOfFile(22));
    record("in loc4", rtcFileLocation(22));
    record("in seek4", rtcFileSeek(22));
    record("line5 err", error_of([] {
               BSTR value = nullptr;
               __vbaLineInputStr(&value, 22);
               SysFreeString(value);
           }));
    __vbaFileSeek(3, 22);
    record("after seek loc", rtcFileLocation(22));
    record("after seek pos", rtcFileSeek(22));
    record("line from 3", line_of(22));
    __vbaFileClose(22);

    open(8, 23, L"wfcio_a.tmp");
    record("app lof", rtcFileLength(23));
    record("app loc", rtcFileLocation(23));
    record("app seek", rtcFileSeek(23));
    __vbaFileClose(23);

    // The Binary file starts empty; the Random file is what it ended up as.
    std::string bytes(11, '\0');
    bytes[0] = '\x78';
    bytes[1] = '\x56';
    bytes[2] = '\x34';
    bytes[3] = '\x12';
    bytes[9] = '\x01';
    bytes[10] = '\x02';
    write_file("wfcio_b.tmp", "");
    open(0x20, 24, L"wfcio_b.tmp");
    record("bin lof", rtcFileLength(24));
    record("bin loc", rtcFileLocation(24));
    record("bin seek", rtcFileSeek(24));
    record_bool("bin eof", rtcEndOfFile(24));
    __vbaFileClose(24);
    write_file("wfcio_b.tmp", bytes);
    open(4, 25, L"wfcio_b.tmp", 6);
    record("rnd lof", rtcFileLength(25));
    record("rnd loc", rtcFileLocation(25));
    record("rnd seek", rtcFileSeek(25));
    record_bool("rnd eof", rtcEndOfFile(25));
    __vbaFileClose(25);

    record("open #0 err", error_of([] { open(1, 0, L"wfcio_b.tmp"); }));
    record("open #513 err", error_of([] { open(1, 513, L"wfcio_b.tmp"); }));
    record("open missing err", error_of([] { open(1, 21, L"nodir\\x.tmp"); }));
    open(1, 21, L"wfcio_b.tmp");
    record("open twice err", error_of([] { open(1, 21, L"wfcio_b.tmp"); }));
    __vbaFileClose(21);
    open(2, 21, L"wfcio_a.tmp");
    record("share err", error_of([] { open(1, 22, L"wfcio_a.tmp"); }));
    __vbaFileClose(21);
    __vbaFileClose(1);

    DeleteFileA("wfcio_a.tmp");
    DeleteFileA("wfcio_b.tmp");
    DeleteFileA("wfcio_own.tmp");

    std::ifstream expected_stream(arguments[1], std::ios::binary);
    std::string expected((std::istreambuf_iterator<char>(expected_stream)),
                         std::istreambuf_iterator<char>());
    std::string cleaned;
    for (const char c : expected) {
        if (c != '\r') {
            cleaned.push_back(c);
        }
    }
    if (cleaned != g_lines) {
        std::fprintf(stderr, "open-file sequence differs.\n--- got\n%s",
                     g_lines.c_str());
        return 1;
    }
    std::printf("open-file sequence matches (%zu bytes)\n", g_lines.size());
    return 0;
}
