// REQ-0103: __vbaRecAssign, CopyRecord, __vbaRecDestruct and
// __vbaRecDestructAnsi on hand-built descriptors (the layout the helpers read:
// size at +2, field count at +6, entries from +0xc).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void* WINAPI __vbaRecAssign(const void* descriptor, void* destination,
                            const void* source);
HRESULT WINAPI CopyRecord(void* destination, const void* source,
                          const void* descriptor);
void* WINAPI __vbaRecDestruct(const void* descriptor, void* record);
void* WINAPI __vbaRecDestructAnsi(const void* descriptor, void* record);
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

    // An array field is refused before anything is touched.
    Descriptor with_array;
    with_array.header(sizeof(Record), 2);
    with_array.simple(static_cast<unsigned>(offsetof(Record, name)), 1);
    with_array.simple(static_cast<unsigned>(offsetof(Record, object)), 5);
    Record keep = {};
    keep.name = bytes_string("keep");
    vbrun::clear_error();
    expect(CopyRecord(&copy, &keep, with_array.bytes.data()) ==
               static_cast<HRESULT>(0x80004001),
           "CopyRecord refuses an array field");
    __vbaRecDestruct(with_array.bytes.data(), &keep);
    expect(keep.name != nullptr && vbrun::last_error() != 0,
           "RecDestruct refuses an array field and raises");
    vbrun::clear_error();

    SysFreeString(keep.name);
    SysFreeString(source.name);
    SysFreeString(V_BSTR(&source.variant));
    SysFreeString(source.inner.text);
}

}  // namespace

int main() {
    records();
    if (failures == 0) {
        std::printf("vbrun records: all checks passed\n");
        return 0;
    }
    return 1;
}
