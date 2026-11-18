// REQ-0103: __vbaRecAssign, CopyRecord, __vbaRecDestruct,
// __vbaRecDestructAnsi, the ANSI conversion, __vbaAryRecCopy and
// __vbaCVarAryUdt on descriptors built as the VB6 compiler builds them (the
// layout the helpers read: size at +2, field count at +6, entries from +0xc).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void* WINAPI __vbaRecAssign(const void* descriptor, void* destination,
                            const void* source);
HRESULT WINAPI CopyRecord(void* destination, const void* source,
                          const void* descriptor);
void* WINAPI __vbaRecDestruct(const void* descriptor, void* record);
void* WINAPI __vbaRecDestructAnsi(const void* descriptor, void* record);
void* WINAPI __vbaRecUniToAnsi(const void* descriptor, void* ansi,
                               const void* unicode);
void* WINAPI __vbaRecAnsiToUni(const void* descriptor, void* unicode,
                               const void* ansi);
void WINAPI __vbaAryRecCopy(const void* descriptor, SAFEARRAY** destination,
                            SAFEARRAY** source);
VARIANT* WINAPI __vbaCVarAryUdt(void* information, VARIANT* result, LONG type,
                                SAFEARRAY** slot);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

struct Object final : IUnknown {
    LONG count = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override {
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++count; }
    ULONG STDMETHODCALLTYPE Release() override { return --count; }
};

struct Inner {
    BSTR text;
};

struct Record {
    BSTR name;
    IUnknown* object;
    VARIANT variant;
    int number;
    Inner inner;
};

// A descriptor under construction.
struct Descriptor {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0xc);

    void put16(const std::size_t at, const unsigned value) {
        if (bytes.size() < at + 2) {
            bytes.resize(at + 2);
        }
        bytes[at] = static_cast<std::uint8_t>(value);
        bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
    }
    void header(const unsigned size, const unsigned fields) {
        put16(2, size);
        put16(6, fields);
    }
    // A four-byte entry: offset, type word.
    void simple(const unsigned offset, const unsigned type) {
        const std::size_t at = bytes.size();
        bytes.resize(at + 4);
        put16(at, offset);
        put16(at + 2, type);
    }
    // A nested-record entry: 0x1c bytes with the descriptor's address at +8.
    void nested(const unsigned offset, const unsigned type,
                const Descriptor& inner) {
        const std::size_t at = bytes.size();
        bytes.resize(at + 0x1c);
        put16(at, offset);
        put16(at + 2, type);
        const std::uint8_t* const address = inner.bytes.data();
        std::memcpy(&bytes[at + 8], &address, sizeof address);
    }
};

BSTR bytes_string(const char* const text) {
    const UINT length = static_cast<UINT>(std::strlen(text));
    return SysAllocStringByteLen(text, length);
}

void records() {
    Descriptor inner;
    inner.header(sizeof(Inner), 1);
    inner.simple(0, 1);

    Descriptor outer;
    outer.header(sizeof(Record), 5);
    outer.simple(static_cast<unsigned>(offsetof(Record, name)), 1);
    outer.simple(static_cast<unsigned>(offsetof(Record, object)), 3);
    outer.simple(static_cast<unsigned>(offsetof(Record, variant)), 2);
    outer.simple(static_cast<unsigned>(offsetof(Record, number)), 11);
    outer.nested(static_cast<unsigned>(offsetof(Record, inner)), 0x29, inner);

    Object object;
    Record source = {};
    source.name = bytes_string("abc");
    source.object = &object;
    V_VT(&source.variant) = VT_BSTR;
    V_BSTR(&source.variant) = bytes_string("variant");
    source.number = 42;
    source.inner.text = bytes_string("inner");

    // CopyRecord: a deep copy; the object is counted.
    Record copy = {};
    expect(CopyRecord(&copy, &source, outer.bytes.data()) == S_OK,
           "CopyRecord succeeds");
    expect(copy.name != nullptr && copy.name != source.name &&
               SysStringByteLen(copy.name) == 3 &&
               std::memcmp(copy.name, "abc", 3) == 0,
           "CopyRecord copies the string");
    expect(copy.object == &object && object.count == 2,
           "CopyRecord counts the object");
    expect(V_VT(&copy.variant) == VT_BSTR &&
               V_BSTR(&copy.variant) != V_BSTR(&source.variant) &&
               std::memcmp(V_BSTR(&copy.variant), "variant", 7) == 0,
           "CopyRecord copies the Variant");
    expect(copy.number == 42, "CopyRecord copies a raw field");
    expect(copy.inner.text != nullptr && copy.inner.text != source.inner.text &&
               std::memcmp(copy.inner.text, "inner", 5) == 0,
           "CopyRecord copies the nested record");

    // RecDestruct frees all of it again.
    expect(__vbaRecDestruct(outer.bytes.data(), &copy) == &copy,
           "RecDestruct returns the record");
    expect(copy.name == nullptr && copy.object == nullptr &&
               V_VT(&copy.variant) == VT_EMPTY && copy.inner.text == nullptr &&
               object.count == 1,
           "RecDestruct frees strings, Variant, object and nested record");

    // RecAssign destroys what the destination held, then copies.
    Record target = {};
    target.name = bytes_string("old");
    target.inner.text = bytes_string("old inner");
    expect(__vbaRecAssign(outer.bytes.data(), &target, &source) == &target,
           "RecAssign returns the destination");
    expect(target.name != nullptr && std::memcmp(target.name, "abc", 3) == 0 &&
               target.object == &object && object.count == 2 &&
               target.number == 42 &&
               std::memcmp(target.inner.text, "inner", 5) == 0,
           "RecAssign replaces the destination with a copy");
    // Assigning a record to itself changes nothing.
    BSTR before = target.name;
    __vbaRecAssign(outer.bytes.data(), &target, &target);
    expect(target.name == before && object.count == 2,
           "RecAssign of a record to itself is a no-op");
    __vbaRecDestructAnsi(outer.bytes.data(), &target);
    expect(target.name == nullptr && object.count == 1,
           "RecDestructAnsi frees the record");

    SysFreeString(source.name);
    SysFreeString(V_BSTR(&source.variant));
    SysFreeString(source.inner.text);
}

// --- array fields (kind 5), measured with VB6.EXE /make probes -------------

// The element shapes the compiler writes into an array entry.
void put32(std::vector<std::uint8_t>& bytes, const std::size_t at,
           const std::uint32_t value) {
    if (bytes.size() < at + 4) {
        bytes.resize(at + 4);
    }
    std::memcpy(&bytes[at], &value, 4);
}

// A dynamic array entry: 10 bytes, 14 with a fixed-length string's length,
// 0x20 with the element descriptor at +0xc.
void dynamic_entry(Descriptor& d, const unsigned offset, const unsigned type,
                   const unsigned element, const Descriptor* nested,
                   const unsigned length = 0) {
    const std::size_t at = d.bytes.size();
    const bool records = (element & 0x60) != 0;
    d.bytes.resize(at + (records ? 0x20 : (element & 0xf) == 10 ? 0xe : 0xa));
    d.put16(at, offset);
    d.put16(at + 2, type);
    d.put16(at + 6, offset);
    d.put16(at + 8, element);
    if (records) {
        const std::uint8_t* const address = nested->bytes.data();
        std::memcpy(&d.bytes[at + 0xc], &address, sizeof address);
    } else if ((element & 0xf) == 10) {
        put32(d.bytes, at + 0xc, length);
    }
}

// A fixed-size array entry with its 32-bit shape header (one dimension).
void fixed_entry(Descriptor& d, const unsigned offset, const unsigned element,
                 const unsigned features, const std::uint32_t size,
                 const std::uint32_t count, const std::uint32_t lower,
                 const Descriptor* nested) {
    const std::size_t at = d.bytes.size();
    const bool records = (element & 0x60) != 0;
    const std::size_t shape = at + (records ? 0x1c : 0x10);
    d.bytes.resize(at + (records ? 0x38 : 0x28) +
                   ((features & 0xe0) != 0 ? 4 : 0));
    d.put16(at, offset);
    d.put16(at + 2, 0x85);
    d.put16(at + 8, element);
    if (records) {
        const std::uint8_t* const address = nested->bytes.data();
        std::memcpy(&d.bytes[at + 0xc], &address, sizeof address);
    }
    d.put16(shape, 1);
    d.put16(shape + 2, features);
    put32(d.bytes, shape + 4, size);
    put32(d.bytes, shape + 0x10, count);
    put32(d.bytes, shape + 0x14, lower);
}

void put_size(Descriptor& d, const std::size_t fields) {
    d.put16(0, static_cast<unsigned>(d.bytes.size()));
    d.put16(6, static_cast<unsigned>(fields));
}

std::string text_of(const BSTR text) {
    return text == nullptr ? std::string()
                           : std::string(reinterpret_cast<const char*>(text),
                                         SysStringByteLen(text));
}

std::wstring wide_of(const BSTR text) {
    return text == nullptr ? std::wstring()
                           : std::wstring(text, SysStringLen(text));
}

SAFEARRAY* string_array(const LONG lower,
                        const std::initializer_list<const wchar_t*> items) {
    SAFEARRAY* const array =
        SafeArrayCreateVector(VT_BSTR, lower, static_cast<ULONG>(items.size()));
    LONG at = lower;
    for (const wchar_t* const item : items) {
        BSTR text = SysAllocString(item);
        SafeArrayPutElement(array, &at, text);
        SysFreeString(text);
        ++at;
    }
    return array;
}

struct InnerU {
    short a;
    BSTR t;
};

// The descriptor of InnerU (a Integer, t String).
Descriptor inner_descriptor() {
    Descriptor d;
    d.header(sizeof(InnerU), 1);
    d.simple(static_cast<unsigned>(offsetof(InnerU, t)), 1);
    put_size(d, 1);
    return d;
}

SAFEARRAY* record_array(const ULONG count) {
    SAFEARRAY* array = nullptr;
    SafeArrayAllocDescriptor(1, &array);
    array->cbElements = sizeof(InnerU);
    array->rgsabound[0].cElements = count;
    array->rgsabound[0].lLbound = 0;
    SafeArrayAllocData(array);
    std::memset(array->pvData, 0, count * sizeof(InnerU));
    return array;
}

void array_fields() {
    // Type TStrDyn: n As Long, a() As String. y = x copies the array deeply
    // ("strdyn copy pqq 1"); the copy keeps its lower bound.
    struct StrDyn {
        LONG n;
        SAFEARRAY* a;
    };
    Descriptor strdyn;
    strdyn.header(sizeof(StrDyn), 1);
    dynamic_entry(strdyn, offsetof(StrDyn, a), 0x105, 0x0001, nullptr);
    put_size(strdyn, 1);
    StrDyn x = {9, string_array(1, {L"p", L"qq"})};
    StrDyn y = {};
    expect(CopyRecord(&y, &x, strdyn.bytes.data()) == S_OK && y.n == 9 &&
               y.a != nullptr && y.a != x.a,
           "a dynamic String array is copied to an array of its own");
    LONG at = 1;
    BSTR text = nullptr;
    SafeArrayGetElement(y.a, &at, &text);
    expect(wide_of(text) == L"p" && y.a->rgsabound[0].lLbound == 1,
           "the copied array keeps the strings and the lower bound");
    SysFreeString(text);
    __vbaRecDestruct(strdyn.bytes.data(), &y);
    expect(y.a == nullptr, "RecDestruct frees a dynamic array field");
    __vbaRecDestruct(strdyn.bytes.data(), &x);

    // Type TRecDyn: k As Byte, a() As Inner: the records of the array are
    // copied one by one ("recdyn copy uvv").
    Descriptor inner = inner_descriptor();
    struct RecDyn {
        BYTE k;
        SAFEARRAY* a;
    };
    Descriptor recdyn;
    recdyn.header(sizeof(RecDyn), 1);
    dynamic_entry(recdyn, offsetof(RecDyn, a), 0x2105, 0x2069, &inner);
    put_size(recdyn, 1);
    RecDyn rx = {3, record_array(2)};
    auto* const items = static_cast<InnerU*>(rx.a->pvData);
    items[0].t = SysAllocString(L"u");
    items[1].a = 2;
    items[1].t = SysAllocString(L"vv");
    RecDyn ry = {};
    expect(CopyRecord(&ry, &rx, recdyn.bytes.data()) == S_OK &&
               ry.a != nullptr && ry.a != rx.a,
           "a dynamic record array is copied");
    const auto* const copied = static_cast<const InnerU*>(ry.a->pvData);
    expect(copied[1].a == 2 && copied[1].t != items[1].t &&
               wide_of(copied[1].t) == L"vv",
           "each record of the array is copied deeply");
    __vbaRecDestruct(recdyn.bytes.data(), &ry);
    __vbaRecDestruct(recdyn.bytes.data(), &rx);
    expect(ry.a == nullptr && rx.a == nullptr,
           "a dynamic record array is freed with its records");

    // Type TStrFix: a(1 To 3) As String, the array in the record.
    struct StrFix {
        BSTR a[3];
    };
    Descriptor strfix;
    strfix.header(sizeof(StrFix), 1);
    fixed_entry(strfix, 0, 0x0001, 0x196, sizeof(BSTR), 3, 1, nullptr);
    put_size(strfix, 1);
    StrFix fx = {{SysAllocString(L"a"), nullptr, SysAllocString(L"ccc")}};
    StrFix fy = {};
    expect(CopyRecord(&fy, &fx, strfix.bytes.data()) == S_OK &&
               fy.a[0] != fx.a[0] && wide_of(fy.a[0]) == L"a" &&
               fy.a[1] == nullptr && wide_of(fy.a[2]) == L"ccc",
           "a fixed String array is copied element by element");
    __vbaRecDestruct(strfix.bytes.data(), &fy);
    __vbaRecDestruct(strfix.bytes.data(), &fx);
    expect(fy.a[0] == nullptr && fy.a[2] == nullptr && fx.a[2] == nullptr,
           "a fixed String array is freed element by element");

    // Type TRecFix: a(0 To 1) As Inner ("recfix copy mnn").
    struct RecFix {
        InnerU a[2];
    };
    Descriptor recfix;
    recfix.header(sizeof(RecFix), 1);
    fixed_entry(recfix, 0, 0x2069, 0x16, sizeof(InnerU), 2, 0, &inner);
    put_size(recfix, 1);
    RecFix gx = {};
    gx.a[0].a = 7;
    gx.a[0].t = SysAllocString(L"m");
    gx.a[1].t = SysAllocString(L"nn");
    RecFix gy = {};
    __vbaRecAssign(recfix.bytes.data(), &gy, &gx);
    expect(gy.a[0].a == 7 && gy.a[0].t != gx.a[0].t &&
               wide_of(gy.a[0].t) == L"m" && wide_of(gy.a[1].t) == L"nn",
           "a fixed record array is assigned deeply");
    __vbaRecDestruct(recfix.bytes.data(), &gy);
    __vbaRecDestruct(recfix.bytes.data(), &gx);
    expect(gy.a[1].t == nullptr && gx.a[0].t == nullptr,
           "a fixed record array is freed");

    // The entry sizes of the reference: these descriptors put a field after
    // the array entry, which is found only when the size is right.
    struct After {
        BSTR a[3];
        BSTR s;
    };
    Descriptor after;
    after.header(sizeof(After), 2);
    fixed_entry(after, 0, 0x0001, 0x196, sizeof(BSTR), 3, 1, nullptr);
    after.simple(static_cast<unsigned>(offsetof(After, s)), 1);
    put_size(after, 2);
    After ax = {{nullptr, nullptr, nullptr}, SysAllocString(L"tail")};
    After ay = {};
    CopyRecord(&ay, &ax, after.bytes.data());
    expect(ay.s != ax.s && wide_of(ay.s) == L"tail",
           "the field after a fixed array entry is found (entry 0x2c)");
    __vbaRecDestruct(after.bytes.data(), &ay);
    __vbaRecDestruct(after.bytes.data(), &ax);
}

// A locked array reference (kind 6) is unlocked when the record goes; a
// field behind the 0x400 chain is found through the record's last pointer.
void locked_and_chained() {
    struct Locked {
        SAFEARRAY* array;
    };
    Descriptor locked;
    locked.header(sizeof(Locked), 1);
    locked.simple(0, 6);
    put_size(locked, 1);
    SAFEARRAY* const array = SafeArrayCreateVector(VT_I4, 0, 2);
    SafeArrayLock(array);
    Locked lx = {array};
    Locked ly = {};
    CopyRecord(&ly, &lx, locked.bytes.data());
    expect(array->cLocks == 2, "copying a locked reference locks again");
    __vbaRecDestruct(locked.bytes.data(), &ly);
    __vbaRecDestruct(locked.bytes.data(), &lx);
    expect(array->cLocks == 0 && lx.array == nullptr,
           "destroying a locked reference unlocks it");
    SafeArrayDestroy(array);

    struct Block {
        LONG pad;
        BSTR text;
    };
    struct Chained {
        LONG n;
        Block* link;  // the last pointer of the record
    };
    Block block = {0, SysAllocString(L"far")};
    Chained c = {1, &block};
    Descriptor chained;
    chained.header(sizeof(Chained), 1);
    const std::size_t at = chained.bytes.size();
    chained.bytes.resize(at + 6);
    chained.put16(at, static_cast<unsigned>(offsetof(Block, text)));
    chained.put16(at + 2, 0x401);
    chained.put16(at + 4, 1);
    put_size(chained, 1);
    __vbaRecDestruct(chained.bytes.data(), &c);
    expect(block.text == nullptr, "a field behind the 0x400 chain is freed");
}

// Unicode <-> ANSI for a Declare'd call. Type Plain: n As Long, s As String,
// f As String * 4, measured: "44 43 42 41 | <ANSI string> | 71 72 20 20".
void ansi() {
    struct Plain {
        LONG n;
        BSTR s;
        wchar_t f[4];
    };
    const unsigned ansi_f = static_cast<unsigned>(offsetof(Plain, s)) +
                            static_cast<unsigned>(sizeof(BSTR));
    Descriptor plain;
    plain.header(sizeof(Plain), 2);
    plain.put16(8, ansi_f + 4);
    plain.simple(static_cast<unsigned>(offsetof(Plain, s)), 1);
    const std::size_t at = plain.bytes.size();
    plain.bytes.resize(at + 10);
    plain.put16(at, static_cast<unsigned>(offsetof(Plain, f)));
    plain.put16(at + 2, 0x200a);
    plain.put16(at + 6, ansi_f);
    plain.put16(at + 8, 4);
    put_size(plain, 2);
    Plain u = {0x41424344, SysAllocString(L"zz"), {L'q', L'r', L' ', L' '}};
    std::uint8_t a[32] = {};
    expect(__vbaRecUniToAnsi(plain.bytes.data(), a, &u) == a,
           "RecUniToAnsi returns the ANSI record");
    BSTR narrow = nullptr;
    std::memcpy(&narrow, a + offsetof(Plain, s), sizeof narrow);
    expect(std::memcmp(a, "\x44\x43\x42\x41", 4) == 0 &&
               text_of(narrow) == "zz" &&
               std::memcmp(a + ansi_f, "qr  ", 4) == 0,
           "the ANSI layout: raw bytes, an ANSI string, one byte a char");
    // The call changes the ANSI record; the way back converts it again.
    SysFreeString(narrow);
    narrow = SysAllocStringByteLen("back", 4);
    std::memcpy(a + offsetof(Plain, s), &narrow, sizeof narrow);
    a[ansi_f] = 'Z';
    expect(__vbaRecAnsiToUni(plain.bytes.data(), &u, a) == &u &&
               wide_of(u.s) == L"back" && u.f[0] == L'Z' && u.f[1] == L'r',
           "RecAnsiToUni brings the strings back as Unicode");
    __vbaRecDestructAnsi(plain.bytes.data(), a);
    std::memcpy(&narrow, a + offsetof(Plain, s), sizeof narrow);
    expect(narrow == nullptr, "RecDestructAnsi frees the ANSI string");
    __vbaRecDestruct(plain.bytes.data(), &u);
}

void record_arrays() {
    Descriptor inner = inner_descriptor();
    SAFEARRAY* a1 = record_array(2);
    static_cast<InnerU*>(a1->pvData)[0].t = SysAllocString(L"one");
    static_cast<InnerU*>(a1->pvData)[1].t = SysAllocString(L"two");
    SAFEARRAY* a2 = record_array(1);
    __vbaAryRecCopy(inner.bytes.data(), &a2, &a1);
    expect(a2 != nullptr && a2 != a1 && a2->rgsabound[0].cElements == 2,
           "AryRecCopy replaces the destination with a copy");
    const auto* const copied = static_cast<const InnerU*>(a2->pvData);
    expect(wide_of(copied[1].t) == L"two" &&
               copied[1].t != static_cast<InnerU*>(a1->pvData)[1].t,
           "AryRecCopy copies the records deeply ('arycopy 1 one two')");
    SAFEARRAY* same = a2;
    __vbaAryRecCopy(inner.bytes.data(), &a2, &same);
    expect(a2 == same, "AryRecCopy of an array to itself does nothing");
    SAFEARRAY* fixed = record_array(1);
    fixed->fFeatures |= FADF_STATIC;
    vbrun::clear_error();
    __vbaAryRecCopy(inner.bytes.data(), &fixed, &a1);
    expect(vbrun::last_error() == 13, "a fixed-size destination is error 13");
    vbrun::clear_error();
    fixed->fFeatures &= static_cast<USHORT>(~FADF_STATIC);
    SAFEARRAY* none = nullptr;
    __vbaAryRecCopy(inner.bytes.data(), &fixed, &none);
    expect(fixed == nullptr, "AryRecCopy of no array erases the destination");
    __vbaAryRecCopy(inner.bytes.data(), &a2, &none);
    __vbaAryRecCopy(inner.bytes.data(), &a1, &none);
    expect(a1 == nullptr && a2 == nullptr, "the arrays are freed");

    // CVarAryUdt: an array that does not exist yet is made empty; the Variant
    // takes the array (or, VT_BYREF, the slot).
    SAFEARRAY* slot = nullptr;
    VARIANT v;
    VariantInit(&v);
    expect(__vbaCVarAryUdt(const_cast<IID*>(&IID_IDispatch), &v,
                           VT_ARRAY | VT_DISPATCH, &slot) == &v &&
               slot != nullptr && slot->cDims == 1 &&
               (slot->fFeatures & FADF_DISPATCH) != 0 &&
               slot->cbElements == sizeof(void*) &&
               V_VT(&v) == (VT_ARRAY | VT_DISPATCH) && V_ARRAY(&v) == slot,
           "CVarAryUdt makes an empty object array for the Variant");
    VARIANT r;
    VariantInit(&r);
    __vbaCVarAryUdt(const_cast<IID*>(&IID_IDispatch), &r,
                    VT_BYREF | VT_ARRAY | VT_DISPATCH, &slot);
    expect(V_VT(&r) == (VT_BYREF | VT_ARRAY | VT_DISPATCH) &&
               V_ARRAYREF(&r) == &slot,
           "CVarAryUdt by reference points at the slot");
    SafeArrayDestroy(slot);
}

}  // namespace

int main() {
    records();
    array_fields();
    locked_and_chained();
    ansi();
    record_arrays();
    if (failures == 0) {
        std::printf("vbrun records: all checks passed\n");
        return 0;
    }
    return 1;
}
