// Print #, Write #, Input #, Get # and Put # of the VBRUN helpers. The
// statements mirror the Visual Basic 6 program of scripts/vb6_harness_gen.py
// (experiments filePrint and fileInput); what they leave in the files and in
// the variables is compared with what that program recorded.
//
// usage: vbrun_file_data_tests <print expected> <input expected>

#include <windows.h>

#include <oleauto.h>

#include <cstdint>
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
LONG WINAPI rtcFileLength(SHORT number);
LONG WINAPI rtcFileLocation(SHORT number);
SHORT WINAPI rtcEndOfFile(SHORT number);
void __cdecl __vbaPrintFile(const unsigned char* descriptor, int number, ...);
void __cdecl __vbaWriteFile(const unsigned char* descriptor, int number, ...);
void __cdecl __vbaInputFile(const unsigned char* descriptor, int number, ...);
void WINAPI __vbaPut3(LONG code, const void* value, SHORT number);
void WINAPI __vbaPut4(LONG code, const void* value, LONG record, SHORT number);
void WINAPI __vbaGet3(LONG code, void* value, SHORT number);
void WINAPI __vbaGet4(LONG code, void* value, LONG record, SHORT number);
void WINAPI __vbaPutFxStr4(LONG length, const wchar_t* text, LONG record,
                           SHORT number);
}

namespace {

constexpr DWORD kOutput = 2;
constexpr DWORD kInput = 1;
constexpr DWORD kRandom = 4;
constexpr DWORD kBinary = 0x20;

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

BSTR text(const wchar_t* const value) {
    return SysAllocString(value);
}

VARIANT variant_of(const VARTYPE type) {
    VARIANT value;
    VariantInit(&value);
    value.vt = type;
    return value;
}

std::string string_of(VARIANT value) {
    VARIANT converted;
    VariantInit(&converted);
    if (FAILED(VariantChangeTypeEx(&converted, &value, LOCALE_USER_DEFAULT,
                                   VARIANT_ALPHABOOL, VT_BSTR))) {
        return "?";
    }
    std::string result;
    for (UINT i = 0; i < SysStringLen(converted.bstrVal); ++i) {
        result.push_back(static_cast<char>(converted.bstrVal[i]));
    }
    VariantClear(&converted);
    return result;
}

template <typename T>
VARIANT typed(const VARTYPE type, const T value) {
    VARIANT variant = variant_of(type);
    std::memcpy(&variant.lVal, &value, sizeof(T));
    return variant;
}

std::string g_print;
std::string g_input;
std::string g_numbers;

void record(std::string& to, const std::string& label,
            const std::string& value) {
    to += label + " => " + value + "\n";
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

// The lines the Visual Basic program recorded: the file's text with its
// carriage returns and line feeds spelled out, cut at every '#'.
void record_lines(const std::string& file_text) {
    std::string spelled;
    for (const char c : file_text) {
        if (c == '\r') {
            spelled += "\\r";
        } else if (c == '\n') {
            spelled += "\\n#";
        } else {
            spelled += c;
        }
    }
    int number = 1;
    std::string rest = spelled;
    while (!rest.empty()) {
        const std::size_t cut = rest.find('#');
        if (cut == std::string::npos) {
            record(g_print, "line" + std::to_string(number), "[" + rest + "]");
            rest.clear();
        } else {
            record(g_print, "line" + std::to_string(number),
                   "[" + rest.substr(0, cut) + "]");
            rest = rest.substr(cut + 1);
        }
        ++number;
    }
}

constexpr unsigned char kInteger = 0x02;
constexpr unsigned char kLong = 0x03;
constexpr unsigned char kSingle = 0x04;
constexpr unsigned char kDouble = 0x05;
constexpr unsigned char kCurrency = 0x06;
constexpr unsigned char kDate = 0x07;
constexpr unsigned char kString = 0x08;
constexpr unsigned char kBoolean = 0x0B;
constexpr unsigned char kVariant = 0x0C;
constexpr unsigned char kByte = 0x11;
constexpr unsigned char kSpc = 0x20;
constexpr unsigned char kTab = 0x21;
constexpr unsigned char kComma = 0x00;
constexpr unsigned char kSemicolon = 0x40;
constexpr unsigned char kNewline = 0x80;

void print_statements() {
    open(kOutput, 21, "wfcio_p.tmp");
    // Each descriptor: the count (a word), then a byte for every item.
    const auto print = [](const unsigned char* descriptor, auto... values) {
        __vbaPrintFile(descriptor, 21, values...);
    };
    const auto write = [](const unsigned char* descriptor, auto... values) {
        __vbaWriteFile(descriptor, 21, values...);
    };
    const auto one = [](const unsigned char type) {
        static unsigned char buffer[4];
        buffer[0] = 1;
        buffer[1] = 0;
        buffer[2] = static_cast<unsigned char>(type | kNewline);
        return static_cast<const unsigned char*>(buffer);
    };
    static_cast<void>(one);
    const unsigned char d_int[] = {1, 0, kInteger | kNewline};
    const unsigned char d_long[] = {1, 0, kLong | kNewline};
    const unsigned char d_single[] = {1, 0, kSingle | kNewline};
    const unsigned char d_double[] = {1, 0, kDouble | kNewline};
    const unsigned char d_cy[] = {1, 0, kCurrency | kNewline};
    const unsigned char d_date[] = {1, 0, kDate | kNewline};
    const unsigned char d_bool[] = {1, 0, kBoolean | kNewline};
    const unsigned char d_byte[] = {1, 0, kByte | kNewline};
    const unsigned char d_var[] = {1, 0, kVariant | kNewline};
    const unsigned char d_str[] = {1, 0, kString | kNewline};
    const unsigned char d_empty[] = {1, 0, kNewline};

    print(d_int, 5);
    print(d_int, -5);
    print(d_long, 70000);
    print(d_single, 1.5F);
    print(d_single, 0.1F);
    print(d_single, 1234567.9F);
    print(d_single, 1E+20F);
    print(d_double, 1.5);
    print(d_double, 0.1);
    print(d_double, 1234567.891);
    print(d_double, 1E+20);
    print(d_double, -1E-10);
    print(d_cy, 125000LL);
    print(d_cy, -30000LL);
    const double date = 36527.0;
    const double time = 15.0 / 24.0 + 4.0 / 1440.0 + 5.0 / 86400.0;
    print(d_date, date);
    print(d_date, date + time);
    print(d_date, time);
    print(d_bool, -1);
    print(d_bool, 0);
    print(d_byte, 7);
    VARIANT v = variant_of(VT_NULL);
    print(d_var, &v);
    v = variant_of(VT_EMPTY);
    print(d_var, &v);
    v = typed(VT_R8, 5.5);
    print(d_var, &v);
    v = variant_of(VT_BSTR);
    v.bstrVal = text(L"text");
    print(d_var, &v);
    VariantClear(&v);
    v = variant_of(VT_ERROR);
    v.scode = static_cast<SCODE>(0x800A0005U);
    print(d_var, &v);
    print(d_str, text(L"abc"));
    const unsigned char d_abc3[] = {3, 0, kString | kComma, kString | kComma,
                                    kString | kNewline};
    print(d_abc3, text(L"a"), text(L"b"), text(L"c"));
    const unsigned char d_mixed[] = {3, 0, kString | kComma, kInteger | kComma,
                                     kInteger | kNewline};
    print(d_mixed, text(L"abc"), 5, 3);
    const unsigned char d_semi[] = {3, 0, kInteger | kSemicolon,
                                    kInteger | kSemicolon, kInteger | kNewline};
    print(d_semi, 1, 2, 3);
    const unsigned char d_tab[] = {3, 0, kString | kSemicolon,
                                   kTab | kSemicolon, kString | kNewline};
    print(d_tab, text(L"a"), 10, text(L"b"));
    const unsigned char d_spc[] = {3, 0, kString | kSemicolon,
                                   kSpc | kSemicolon, kString | kNewline};
    print(d_spc, text(L"a"), 3, text(L"b"));
    const unsigned char d_zone[] = {3, 0, kString | kSemicolon, 0x00,
                                    kString | kNewline};
    print(d_zone, text(L"a"), text(L"b"));
    const unsigned char d_hold[] = {1, 0, kString | kSemicolon};
    print(d_hold, text(L"x"));
    print(d_str, text(L"y"));
    const unsigned char d_comma[] = {1, 0, kString | kComma};
    print(d_comma, text(L"m"));
    print(d_str, text(L"n"));
    print(d_empty);
    print(d_str, text(L""));
    print(d_tab, text(L"tab"), 2, text(L"x"));
    const unsigned char d_long_tab[] = {3, 0, kLong | kSemicolon,
                                        kTab | kSemicolon, kString | kNewline};
    print(d_long_tab, 123456789, 5, text(L"x"));
    const unsigned char d_four[] = {4,
                                    0,
                                    kInteger | kSemicolon,
                                    kLong | kSemicolon,
                                    kDouble | kSemicolon,
                                    kString | kNewline};
    print(d_four, -5, 70000, -1E-10, text(L"abc"));

    const unsigned char w_four[] = {4,
                                    0,
                                    kString | kComma,
                                    kInteger | kComma,
                                    kDouble | kComma,
                                    kBoolean | kNewline};
    write(w_four, text(L"a"), 5, -2.5, -1);
    write(d_date, date);
    write(d_date, date + time);
    v = variant_of(VT_NULL);
    write(d_var, &v);
    v = variant_of(VT_EMPTY);
    write(d_var, &v);
    v = variant_of(VT_ERROR);
    v.scode = static_cast<SCODE>(0x800A0005U);
    write(d_var, &v);
    write(d_cy, 35000LL);
    write(d_byte, 2);
    write(d_double, 1E+20);
    write(d_single, 0.1F);
    write(d_double, 0.1);
    const unsigned char w_pair[] = {2, 0, kString | kComma, kString | kNewline};
    write(w_pair, text(L"a"), text(L"b"));
    write(d_empty);
    write(d_str, text(L"say \"hi\""));
    write(d_str, text(L"y"));
    write(w_pair, text(L"a"), text(L"b"));
    write(d_bool, 0);
    __vbaFileClose(21);
    record_lines(read_file("wfcio_p.tmp"));
    DeleteFileA("wfcio_p.tmp");
}

void input_statements() {
    write_file("wfcio_i.tmp",
               "12, -7, 3.5, 1E+20, 2.25, #1/2/2000#, True, 9, abc, \"q,r\"\r\n"
               "  42  ,  hello world  ,7\r\n1,2\r\n3 4\r\n-1.5E2,\r\n"
               "x,#2000-01-02#,#TRUE#,#NULL#,#ERROR 5#\r\n\r\n99999\r\n"
               "&H1F,&O17\r\n");
    open(kInput, 22, "wfcio_i.tmp");
    SHORT ia = 0;
    LONG ib = 0;
    float ic = 0;
    double id = 0;
    long long ie = 0;
    double ig = 0;
    SHORT ih = 0;
    unsigned char ii = 0;
    BSTR is1 = nullptr;
    BSTR is2 = nullptr;
    VARIANT iv = variant_of(VT_EMPTY);
    VARIANT iv2 = variant_of(VT_EMPTY);
    LONG ik = 0;
    const auto input_error = [](auto call) {
        vbrun::clear_error();
        call();
        return std::to_string(vbrun::last_error());
    };
    const auto bstr_text = [](BSTR value) {
        std::string out;
        for (UINT i = 0; value != nullptr && i < SysStringLen(value); ++i) {
            out.push_back(static_cast<char>(value[i]));
        }
        return out;
    };

    const unsigned char d1[] = {
        10,        0,     kInteger, kLong, kSingle, kDouble,
        kCurrency, kDate, kBoolean, kByte, kString, kString | kNewline};
    std::string error = input_error([&] {
        __vbaInputFile(d1, 22, &ia, &ib, &ic, &id, &ie, &ig, &ih, &ii, &is1,
                       &is2);
    });
    // The Visual Basic program had not cleared the error of an earlier Kill
    // of a file that did not exist (53): its in1 line says "err 53".
    record(g_input, "in1",
           std::to_string(ia) + "|" + std::to_string(ib) + "|" +
               string_of(typed(VT_R4, ic)) + "|" + string_of(typed(VT_R8, id)) +
               "|" + string_of(typed(VT_CY, ie)) + "|" +
               string_of(typed(VT_DATE, ig)) + "|" +
               string_of(typed(VT_BOOL, ih)) + "|" + std::to_string(ii) + "|" +
               bstr_text(is1) + "|" + bstr_text(is2) + " err " +
               (error == "0" ? "53" : error));

    const unsigned char d2[] = {3, 0, kVariant, kString, kVariant | kNewline};
    error = input_error([&] { __vbaInputFile(d2, 22, &iv, &is1, &iv2); });
    VARIANT shown = variant_of(VT_EMPTY);
    record(g_input, "in2",
           string_of(iv) + "|" + std::string(iv.vt == VT_I2 ? "Integer" : "?") +
               "|" + bstr_text(is1) + "|" + string_of(iv2) + "|" +
               std::string(iv2.vt == VT_I2 ? "Integer" : "?") + " err " +
               error);
    const unsigned char d3[] = {2, 0, kInteger, kLong | kNewline};
    error = input_error([&] { __vbaInputFile(d3, 22, &ia, &ib); });
    record(g_input, "in3",
           std::to_string(ia) + "|" + std::to_string(ib) + " err " + error);
    error = input_error([&] { __vbaInputFile(d3, 22, &ia, &ib); });
    record(g_input, "in4",
           std::to_string(ia) + "|" + std::to_string(ib) + " err " + error);
    const unsigned char d5[] = {2, 0, kDouble, kString | kNewline};
    error = input_error([&] { __vbaInputFile(d5, 22, &id, &is1); });
    record(
        g_input, "in5",
        string_of(typed(VT_R8, id)) + "|[" + bstr_text(is1) + "] err " + error);
    const unsigned char d6[] = {4,        0,        kVariant,
                                kVariant, kBoolean, kLong | kNewline};
    error = input_error([&] { __vbaInputFile(d6, 22, &iv, &iv2, &ih, &ik); });
    record(g_input, "in6",
           string_of(iv) + "|" +
               std::string(iv.vt == VT_BSTR ? "String" : "?") + "|" +
               string_of(iv2) + "|" +
               std::string(iv2.vt == VT_DATE ? "Date" : "?") + "|" +
               string_of(typed(VT_BOOL, ih)) + "|" + std::to_string(ik) +
               " err " + error);
    const unsigned char d7[] = {1, 0, kVariant | kNewline};
    error = input_error([&] { __vbaInputFile(d7, 22, &iv); });
    static_cast<void>(shown);
    static_cast<void>(
        error);  // the Visual Basic line failed (an error Variant)
    const unsigned char d8[] = {1, 0, kInteger | kNewline};
    error = input_error([&] { __vbaInputFile(d8, 22, &ia); });
    record(g_input, "in8", std::to_string(ia) + " err " + error);
    const unsigned char d9[] = {2, 0, kInteger, kLong | kNewline};
    error = input_error([&] { __vbaInputFile(d9, 22, &ia, &ib); });
    record(g_input, "in9",
           std::to_string(ia) + "|" + std::to_string(ib) + " err " + error);
    error = input_error([&] { __vbaInputFile(d8, 22, &ia); });
    record(g_input, "in10", std::to_string(ia) + " err " + error);
    record(g_input, "in eof", rtcEndOfFile(22) != 0 ? "True" : "False");
    __vbaFileClose(22);
    DeleteFileA("wfcio_i.tmp");

    // Put and Get.
    DeleteFileA("wfcio_g.tmp");
    open(kBinary, 24, "wfcio_g.tmp");
    unsigned char by = 200;
    __vbaPut3(1, &by, 24);
    SHORT in = -2;
    __vbaPut3(2, &in, 24);
    LONG lo = 0x12345678;
    __vbaPut3(4, &lo, 24);
    float sg = 1.5F;
    __vbaPut3(4, &sg, 24);
    double db = 2.5;
    __vbaPut3(8, &db, 24);
    long long cy = 32500;
    __vbaPut3(8, &cy, 24);
    double dt = 36527.0;
    __vbaPut3(8, &dt, 24);
    SHORT bo = -1;
    __vbaPut3(2, &bo, 24);
    BSTR abc = text(L"abc");
    __vbaPut3(0, &abc, 24);
    SysFreeString(abc);
    VARIANT pv = typed(VT_I2, static_cast<SHORT>(7));
    __vbaPut3(-1, &pv, 24);
    pv = variant_of(VT_BSTR);
    pv.bstrVal = text(L"vs");
    __vbaPut3(-1, &pv, 24);
    VariantClear(&pv);
    record(g_input, "bin lof", std::to_string(rtcFileLength(24)));
    __vbaFileClose(24);
    const std::string bytes = read_file("wfcio_g.tmp");
    record(g_input, "bin bytes", hex_of(bytes));
    open(kBinary, 24, "wfcio_g.tmp");
    by = 0;
    in = 0;
    lo = 0;
    sg = 0;
    db = 0;
    cy = 0;
    dt = 0;
    bo = 0;
    __vbaGet3(1, &by, 24);
    __vbaGet3(2, &in, 24);
    __vbaGet3(4, &lo, 24);
    __vbaGet3(4, &sg, 24);
    __vbaGet3(8, &db, 24);
    __vbaGet3(8, &cy, 24);
    __vbaGet3(8, &dt, 24);
    __vbaGet3(2, &bo, 24);
    BSTR three = SysAllocString(L"   ");
    __vbaGet3(0, &three, 24);
    char hexed[16];
    std::snprintf(hexed, sizeof(hexed), "%lX", static_cast<unsigned long>(lo));
    record(g_input, "bin get",
           std::to_string(by) + "|" + std::to_string(in) + "|" + hexed + "|" +
               string_of(typed(VT_R4, sg)) + "|" + string_of(typed(VT_R8, db)) +
               "|" + string_of(typed(VT_CY, cy)) + "|" +
               string_of(typed(VT_DATE, dt)) + "|" +
               string_of(typed(VT_BOOL, bo)) + "|" + bstr_text(three));
    SysFreeString(three);
    VARIANT gv = variant_of(VT_EMPTY);
    __vbaGet3(-1, &gv, 24);
    record(g_input, "bin getv",
           string_of(gv) + "|" + std::string(gv.vt == VT_I2 ? "Integer" : "?"));
    __vbaGet3(-1, &gv, 24);
    record(
        g_input, "bin getv2",
        string_of(gv) + "|" + std::string(gv.vt == VT_BSTR ? "String" : "?"));
    VariantClear(&gv);
    record(g_input, "bin eof", rtcEndOfFile(24) != 0 ? "True" : "False");
    in = 99;
    __vbaGet3(2, &in, 24);
    record(g_input, "bin past",
           std::to_string(in) + " err 53 eof " +
               (rtcEndOfFile(24) != 0 ? "True" : "False"));
    __vbaFileClose(24);
    DeleteFileA("wfcio_g.tmp");

    open(kRandom, 25, "wfcio_g.tmp", 20);
    in = 11;
    __vbaPut4(2, &in, 3, 25);
    record(g_input, "rnd lof", std::to_string(rtcFileLength(25)));
    BSTR hello = text(L"hello");
    __vbaPut4(0, &hello, 1, 25);
    SysFreeString(hello);
    record(g_input, "rnd lof2",
           std::to_string(rtcFileLength(25)) + " loc " +
               std::to_string(rtcFileLocation(25)));
    const wchar_t wxyz[] = L"wxyz";
    __vbaPutFxStr4(4, wxyz, 2, 25);
    record(g_input, "rnd lof3", std::to_string(rtcFileLength(25)));
    __vbaFileClose(25);
    record(g_input, "rnd bytes", hex_of(read_file("wfcio_g.tmp")));
    DeleteFileA("wfcio_g.tmp");
}

struct NumberValue {
    char kind;
    unsigned long long bits;
};

constexpr NumberValue kNumberValues[] = {
#include "cases/filenumbers_values.inc"
};

// Every value printed and then written, as the Visual Basic program did.
void number_statements() {
    open(kOutput, 21, "wfcio_n.tmp");
    const unsigned char d_single[] = {1, 0, kSingle | kNewline};
    const unsigned char d_double[] = {1, 0, kDouble | kNewline};
    const unsigned char d_date[] = {1, 0, kDate | kNewline};
    for (const NumberValue& item : kNumberValues) {
        for (int pass = 0; pass < 2; ++pass) {
            const auto emit = [&](const unsigned char* descriptor, auto value) {
                if (pass == 0) {
                    __vbaPrintFile(descriptor, 21, value);
                } else {
                    __vbaWriteFile(descriptor, 21, value);
                }
            };
            if (item.kind == 'f') {
                const auto word = static_cast<std::uint32_t>(item.bits);
                float single = 0;
                std::memcpy(&single, &word, sizeof(single));
                emit(d_single, single);
            } else {
                double real = 0;
                std::memcpy(&real, &item.bits, sizeof(real));
                emit(item.kind == 'd' ? d_double : d_date, real);
            }
        }
    }
    __vbaFileClose(21);
    std::string rest = read_file("wfcio_n.tmp");
    DeleteFileA("wfcio_n.tmp");
    std::string cleaned;
    for (const char c : rest) {
        if (c != '\r') {
            cleaned.push_back(c);
        }
    }
    int number = 1;
    std::size_t at = 0;
    while (at < cleaned.size()) {
        const std::size_t end = cleaned.find('\n', at);
        const std::string line = cleaned.substr(at, end - at);
        record(g_numbers, "n" + std::to_string(number++), "[" + line + "]");
        at = end == std::string::npos ? cleaned.size() : end + 1;
    }
}

bool matches(const char* const path, const std::string& got,
             const char* const name) {
    std::ifstream in(path, std::ios::binary);
    std::string want((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    std::string cleaned;
    for (const char c : want) {
        if (c != '\r') {
            cleaned.push_back(c);
        }
    }
    if (cleaned == got) {
        std::printf("%s matches (%zu bytes)\n", name, got.size());
        return true;
    }
    std::fprintf(stderr, "%s differs.\n", name);
    // The first lines that differ.
    std::size_t a = 0;
    std::size_t b = 0;
    while (a < cleaned.size() || b < got.size()) {
        const std::size_t ea = cleaned.find('\n', a);
        const std::size_t eb = got.find('\n', b);
        const std::string la = cleaned.substr(
            a, ea == std::string::npos ? std::string::npos : ea - a);
        const std::string lb =
            got.substr(b, eb == std::string::npos ? std::string::npos : eb - b);
        if (la != lb) {
            std::fprintf(stderr, "  want %s\n  got  %s\n", la.c_str(),
                         lb.c_str());
        }
        if (ea == std::string::npos && eb == std::string::npos) {
            break;
        }
        a = ea == std::string::npos ? cleaned.size() : ea + 1;
        b = eb == std::string::npos ? got.size() : eb + 1;
    }
    return false;
}

}  // namespace

int main(const int argument_count, const char* const arguments[]) {
    if (argument_count != 4) {
        std::fprintf(stderr, "usage: vbrun_file_data_tests <print> <input>\n");
        return 2;
    }
    print_statements();
    input_statements();
    number_statements();
    const bool printed = matches(arguments[1], g_print, "Print and Write");
    const bool input = matches(arguments[2], g_input, "Input, Get and Put");
    const bool numbers = matches(arguments[3], g_numbers, "Number text");
    return printed && input && numbers ? 0 : 1;
}
