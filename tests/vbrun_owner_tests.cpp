// REQ-0005: Get # and Put # of records and arrays (__vbaGetOwner3/4,
// __vbaPutOwner3/4) and Print on an object (__vbaPrintObj). The expected file
// bytes were measured with Visual Basic 6 programs built with VB6.EXE /make;
// the layouts are built the way the compiler builds them, with the padding
// of this target's records.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void __cdecl __vbaFileOpen(DWORD flags, int record_length, int number,
                           BSTR name);
void WINAPI __vbaFileClose(SHORT number);
void WINAPI __vbaPutOwner3(const void* layout, void* value, SHORT number);
void WINAPI __vbaPutOwner4(const void* layout, void* value, LONG record,
                           SHORT number);
void WINAPI __vbaGetOwner3(const void* layout, void* value, SHORT number);
void WINAPI __vbaGetOwner4(const void* layout, void* value, LONG record,
                           SHORT number);
void __cdecl __vbaPrintObj(const unsigned char* descriptor, IUnknown* object,
                           ...);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr DWORD kRandom = 4;
constexpr DWORD kBinary = 0x20;

// A file layout under construction: the record size, the items (12 bytes
// from +0xa), then the layouts of nested records, found from their item.
struct Layout {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0xa);
    std::vector<std::pair<std::size_t, const Layout*>> nested;

    explicit Layout(const std::size_t size) { put32(0, size); }

    void put32(const std::size_t at, const std::size_t value) {
        const std::uint32_t v = static_cast<std::uint32_t>(value);
        std::memcpy(&bytes[at], &v, 4);
    }
    void put16(const std::size_t at, const unsigned value) {
        const std::uint16_t v = static_cast<std::uint16_t>(value);
        std::memcpy(&bytes[at], &v, 2);
    }
    void item(const std::size_t raw, const unsigned type,
              const std::size_t count = 1, const unsigned length = 0) {
        const std::size_t at = bytes.size();
        bytes.resize(at + 12);
        put32(at, raw);
        put16(at + 4, type);
        put32(at + 6, count);
        put16(at + 10, length);
        std::uint32_t items = 0;
        std::memcpy(&items, &bytes[4], 4);
        put32(4, items + 1);
    }
    void padding(const std::size_t size) {
        item(0, 5 | static_cast<unsigned>(size << 8));
    }
    void record(const std::size_t raw, const Layout& inner) {
        nested.emplace_back(bytes.size(), &inner);
        item(raw, 2);
    }
    // The finished bytes: nested layouts after the items.
    std::vector<std::uint8_t> build() const {
        std::vector<std::uint8_t> out = bytes;
        for (const auto& [at, inner] : nested) {
            const std::vector<std::uint8_t> more = inner->build();
            const std::uint16_t distance =
                static_cast<std::uint16_t>(out.size() - at);
            std::memcpy(&out[at + 10], &distance, 2);
            out.insert(out.end(), more.begin(), more.end());
        }
        return out;
    }
};

std::string hex(const std::string& bytes) {
    std::string out;
    char cell[4];
    for (const char c : bytes) {
        std::snprintf(cell, sizeof cell, "%02X ",
                      static_cast<unsigned>(static_cast<unsigned char>(c)));
        out += cell;
    }
    return out;
}

std::string file_bytes(const wchar_t* const name) {
    std::string bytes;
    const HANDLE file = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return bytes;
    }
    char buffer[512];
    DWORD got = 0;
    while (ReadFile(file, buffer, sizeof buffer, &got, nullptr) != 0 &&
           got > 0) {
        bytes.append(buffer, got);
    }
    CloseHandle(file);
    return bytes;
}

void open(const wchar_t* const name, const DWORD mode, const int number,
          const int length = 0) {
    BSTR path = SysAllocString(name);
    __vbaFileOpen(mode, length, number, path);
    SysFreeString(path);
}

std::wstring wide_of(const BSTR text) {
    return text == nullptr ? std::wstring()
                           : std::wstring(text, SysStringLen(text));
}

struct Inner {
    short a;
    BSTR t;
};

struct Rec {
    LONG n;
    BSTR s;
    wchar_t f[3];
    VARIANT v;
    SAFEARRAY* d;
    short x[2];
    Inner i;
};

// Type Rec of the probe: n As Long, s As String, f As String * 3,
// v As Variant, d() As Integer, x(1 To 2) As Integer, i As Inner.
void records() {
    Layout inner(sizeof(Inner));
    inner.item(sizeof(short), 5 | ((offsetof(Inner, t) - sizeof(short)) << 8));
    inner.item(0, 0);
    Layout rec(sizeof(Rec));
    rec.item(sizeof(LONG), 5 | ((offsetof(Rec, s) - sizeof(LONG)) << 8));
    rec.item(0, 0);
    rec.item(0, 6, 1, 3);
    rec.padding(offsetof(Rec, v) - offsetof(Rec, f) - 6);
    rec.item(0, 1);
    rec.item(0, 0x33, 0xffffffff, 2);
    rec.item(sizeof(short) * 2,
             5 | ((offsetof(Rec, i) - offsetof(Rec, x) - 4) << 8));
    rec.record(0, inner);
    const std::vector<std::uint8_t> layout = rec.build();

    Rec r = {};
    r.n = 7;
    r.s = SysAllocString(L"hey");
    std::memcpy(r.f, L"ab ", 6);
    V_VT(&r.v) = VT_R8;
    V_R8(&r.v) = 3.5;
    SAFEARRAYBOUND bound = {2, 2};
    r.d = SafeArrayCreate(VT_I2, 1, &bound);
    static_cast<short*>(r.d->pvData)[0] = 11;
    static_cast<short*>(r.d->pvData)[1] = 12;
    r.x[0] = 21;
    r.x[1] = 22;
    r.i.a = 5;
    r.i.t = SysAllocString(L"in");

    const std::string measured =
        "07 00 00 00 03 00 68 65 79 61 62 20 05 00 00 00 00 00 00 00 0C 40 "
        "01 00 02 00 00 00 02 00 00 00 0B 00 0C 00 15 00 16 00 05 00 02 00 "
        "69 6E ";
    DeleteFileW(L"owner.bin");
    open(L"owner.bin", kBinary, 1);
    __vbaPutOwner3(layout.data(), &r, 1);
    __vbaPutOwner4(layout.data(), &r, 1, 1);
    __vbaFileClose(1);
    expect(hex(file_bytes(L"owner.bin")) == measured,
           "Put of a record writes what Visual Basic writes ('binary ...')");

    Rec g = {};
    open(L"owner.bin", kBinary, 1);
    __vbaGetOwner3(layout.data(), &g, 1);
    __vbaFileClose(1);
    expect(g.n == 7 && wide_of(g.s) == L"hey" &&
               std::memcmp(g.f, L"ab ", 6) == 0 && V_VT(&g.v) == VT_R8 &&
               V_R8(&g.v) == 3.5 && g.x[1] == 22 && g.i.a == 5 &&
               wide_of(g.i.t) == L"in",
           "Get reads the record back ('got 7 hey [ab ] 3.5 ... in')");
    expect(g.d != nullptr && g.d->rgsabound[0].lLbound == 2 &&
               g.d->rgsabound[0].cElements == 2 &&
               static_cast<short*>(g.d->pvData)[1] == 12,
           "Get makes the dynamic array of the shape read ('2:3 12')");

    // A Random file: each Put is a record of its own.
    DeleteFileW(L"owner.bin");
    open(L"owner.bin", kRandom, 1, 80);
    __vbaPutOwner3(layout.data(), &r, 1);
    __vbaPutOwner4(layout.data(), &r, 2, 1);
    Rec h = {};
    __vbaGetOwner4(layout.data(), &h, 1, 1);
    __vbaFileClose(1);
    const std::string random = file_bytes(L"owner.bin");
    expect(random.size() == 126 && hex(random.substr(0, 46)) == measured &&
               hex(random.substr(80)) == measured &&
               random.substr(46, 34) == std::string(34, '\0'),
           "Put to a Random file writes record 1 and record 2");
    expect(h.n == 7 && wide_of(h.i.t) == L"in", "Get of record 1");

    // Too long for the record: error 59.
    open(L"owner.bin", kRandom, 1, 16);
    vbrun::clear_error();
    __vbaPutOwner3(layout.data(), &r, 1);
    expect(vbrun::last_error() == 59, "a record longer than Len is error 59");
    vbrun::clear_error();
    __vbaFileClose(1);
    DeleteFileW(L"owner.bin");

    for (Rec* const each : {&r, &g, &h}) {
        SysFreeString(each->s);
        VariantClear(&each->v);
        if (each->d != nullptr) {
            SafeArrayDestroy(each->d);
        }
        SysFreeString(each->i.t);
    }
}

std::string put_bytes_of(const std::vector<std::uint8_t>& layout,
                         void* const value) {
    DeleteFileW(L"owner2.bin");
    open(L"owner2.bin", kBinary, 2);
    __vbaPutOwner3(layout.data(), value, 2);
    __vbaFileClose(2);
    return hex(file_bytes(L"owner2.bin"));
}

SAFEARRAY* strings(const LONG lower, const wchar_t* const a,
                   const wchar_t* const b) {
    SAFEARRAY* const array = SafeArrayCreateVector(VT_BSTR, lower, 2);
    LONG at = lower;
    BSTR text = SysAllocString(a);
    SafeArrayPutElement(array, &at, text);
    SysFreeString(text);
    ++at;
    text = SysAllocString(b);
    SafeArrayPutElement(array, &at, text);
    SysFreeString(text);
    return array;
}

void arrays() {
    // Type TStrDyn: n As Long, a() As String ("strdyn put").
    struct StrDyn {
        LONG n;
        SAFEARRAY* a;
    };
    Layout strdyn(sizeof(StrDyn));
    strdyn.item(sizeof(LONG), 5 | ((offsetof(StrDyn, a) - sizeof(LONG)) << 8));
    strdyn.item(0, 0x30, 0xffffffff);
    const std::vector<std::uint8_t> layout = strdyn.build();
    StrDyn x = {9, strings(1, L"p", L"qq")};
    expect(put_bytes_of(layout, &x) ==
               "09 00 00 00 01 00 02 00 00 00 01 00 00 00 01 00 70 02 00 "
               "71 71 ",
           "a dynamic String array: its shape, then each string");
    StrDyn empty = {};
    expect(put_bytes_of(layout, &empty) == "00 00 00 00 00 00 ",
           "an array that does not exist: no dimensions");
    // Get replaces an array of another shape ("strdyn get2 1:2 pqq").
    put_bytes_of(layout, &x);
    StrDyn w = {0, strings(5, L"a", L"b")};
    open(L"owner2.bin", kBinary, 2);
    __vbaGetOwner3(layout.data(), &w, 2);
    __vbaFileClose(2);
    LONG at = 2;
    BSTR text = nullptr;
    SafeArrayGetElement(w.a, &at, &text);
    expect(w.n == 9 && w.a->rgsabound[0].lLbound == 1 &&
               w.a->rgsabound[0].cElements == 2 && wide_of(text) == L"qq",
           "Get gives the array the shape in the file");
    SysFreeString(text);
    SafeArrayDestroy(w.a);
    SafeArrayDestroy(x.a);

    // Type TVarDyn: a() As Variant ("vardyn put").
    struct VarDyn {
        SAFEARRAY* a;
    };
    Layout vardyn(sizeof(VarDyn));
    vardyn.item(0, 0x31, 0xffffffff);
    SAFEARRAY* const variants = SafeArrayCreateVector(VT_VARIANT, 0, 2);
    auto* const items = static_cast<VARIANT*>(variants->pvData);
    V_VT(&items[0]) = VT_I2;
    V_I2(&items[0]) = 5;
    V_VT(&items[1]) = VT_BSTR;
    V_BSTR(&items[1]) = SysAllocString(L"s");
    VarDyn v = {variants};
    expect(put_bytes_of(vardyn.build(), &v) ==
               "01 00 02 00 00 00 00 00 00 00 02 00 05 00 08 00 01 00 73 ",
           "a dynamic Variant array: each Variant with its type");
    SafeArrayDestroy(variants);

    // Type TFxsDyn: a() As String * 2 ("fxsdyn put ... 61 62 63 20").
    struct FxsDyn {
        SAFEARRAY* a;
    };
    Layout fxsdyn(sizeof(FxsDyn));
    fxsdyn.item(0, 0x36, 0xffffffff, 2);
    SAFEARRAY* fixed = nullptr;
    SafeArrayAllocDescriptor(1, &fixed);
    fixed->cbElements = 4;
    fixed->rgsabound[0].cElements = 2;
    SafeArrayAllocData(fixed);
    std::memcpy(fixed->pvData, L"abc ", 8);
    FxsDyn f = {fixed};
    expect(put_bytes_of(fxsdyn.build(), &f) ==
               "01 00 02 00 00 00 00 00 00 00 61 62 63 20 ",
           "a dynamic fixed-length String array: the characters as bytes");
    SafeArrayDestroy(fixed);

    // A Byte array variable on its own in a Binary file: the data only.
    Layout bytes(sizeof(SAFEARRAY*));
    bytes.item(0, 0x173, 0xffffffff, 1);
    SAFEARRAY* data = SafeArrayCreateVector(VT_UI1, 0, 2);
    std::memcpy(data->pvData, "hi", 2);
    expect(put_bytes_of(bytes.build(), &data) == "68 69 ",
           "an array variable in a Binary file has no shape");
    std::memset(data->pvData, 0, 2);
    open(L"owner2.bin", kBinary, 2);
    __vbaGetOwner3(bytes.build().data(), &data, 2);
    __vbaFileClose(2);
    expect(std::memcmp(data->pvData, "hi", 2) == 0,
           "Get fills the array variable from the file");
    SafeArrayDestroy(data);
    DeleteFileW(L"owner2.bin");
}

// An object with a Print member that keeps what it was given.
class Printer final : public IDispatch {
public:
    std::wstring text;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDispatch)) {
            *out = this;
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {
        *count = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* names, UINT, LCID,
                                            DISPID* ids) override {
        if (std::wstring(names[0]) == L"Print") {
            ids[0] = 7;
            return S_OK;
        }
        ids[0] = DISPID_UNKNOWN;
        return DISP_E_UNKNOWNNAME;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* params, VARIANT*, EXCEPINFO*,
                                     UINT*) override {
        if (id != 7 || flags != DISPATCH_METHOD || params->cArgs != 1 ||
            V_VT(&params->rgvarg[0]) != VT_BSTR) {
            return DISP_E_MEMBERNOTFOUND;
        }
        text += wide_of(V_BSTR(&params->rgvarg[0]));
        return S_OK;
    }
};

// An object without one.
class Plain final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override {
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
};

void print_object() {
    // Me.Print "hello"; 5; Tab(10); "x", then Me.Print (descriptors of the
    // probe: 48 42 61 08 and 80).
    const unsigned char list[] = {4, 0, 0x48, 0x42, 0x61, 0x08};
    const unsigned char end[] = {1, 0, 0x80};
    const unsigned char more[] = {1, 0, 0x48};
    Printer printer;
    BSTR hello = SysAllocString(L"hello");
    BSTR x = SysAllocString(L"x");
    BSTR y = SysAllocString(L"y");
    __vbaPrintObj(list, &printer, hello, 5, 10, x);
    expect(printer.text == L"hello 5  x    ",
           "Print lays the list out as Print # does");
    __vbaPrintObj(more, &printer, y);
    expect(printer.text == L"hello 5  x    y",
           "the next Print goes on from the object's column");
    __vbaPrintObj(end, &printer);
    expect(printer.text == L"hello 5  x    y\r\n", "Print alone ends the line");
    Plain plain;
    vbrun::clear_error();
    __vbaPrintObj(more, &plain, y);
    expect(vbrun::last_error() == 438, "an object without Print is error 438");
    vbrun::clear_error();
    __vbaPrintObj(more, nullptr, y);
    expect(vbrun::last_error() == 91, "Nothing is error 91");
    vbrun::clear_error();
    SysFreeString(hello);
    SysFreeString(x);
    SysFreeString(y);
}

}  // namespace

int main() {
    records();
    arrays();
    print_object();
    if (failures == 0) {
        std::printf("vbrun owner and print: all checks passed\n");
        return 0;
    }
    return 1;
}
