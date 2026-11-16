// REQ-0103: the record helpers that copy and destroy a user-defined type from
// its descriptor: __vbaRecAssign, CopyRecord, __vbaRecDestruct and
// __vbaRecDestructAnsi. Written from the disassembly of the installed
// MSVBVM60 6.00.9848.
//
// The descriptor the compiler emits: the record size (UInt16 at +2), the
// number of fields (+6), the size of the ANSI layout (+8), then the field
// entries from +0xc, each of its own length. An entry is the offset of the
// field (+0), a type word (+2: the kind in the low nibble, flags above it) and
// more words after that, by kind:
//   1, 4   string (a BSTR in the record); entry 4 bytes, kind 4 is 10
//   2      Variant                        4
//   3      object (IUnknown pointer)      4
//   6      4        7  0        8  6      9  nested record, 0x1c, the nested
//   10     10       11 raw bytes, 4         descriptor's address at +8 when
//                                           the type word has 0x20 set
// A type word with 0x2000 widens kinds 1, 2, 3 and 11 to 8 bytes (the ANSI
// offset moves to +6); 0x400 makes the entries at least 6 bytes and puts the
// field behind a chain of pointers. Copying fills the destination with the
// source bytes and then copies each string, Variant and nested record deeply
// and counts each object; destroying frees them again.
//
// Not reproduced: array fields (kind 5), kind 6 and the 0x400 chain. A record
// whose descriptor holds one is refused (E_NOTIMPL, raised as a Basic error)
// before anything is touched, instead of being half handled. The entry layout
// is read with pointer-sized loads where the reference holds an address, so
// the helpers behave the same on every target the library is built for.

#include <cstdint>
#include <cstring>

#include "vbrun_helpers.hpp"

namespace {

constexpr unsigned kString = 1;
constexpr unsigned kVariant = 2;
constexpr unsigned kObject = 3;
constexpr unsigned kFixedString = 4;
constexpr unsigned kArray = 5;
constexpr unsigned kOther6 = 6;
constexpr unsigned kRecord = 9;

constexpr unsigned kHasNested = 0x20;
constexpr unsigned kAnsiNested = 0x1000;
constexpr unsigned kAnsiOffset = 0x2000;
constexpr unsigned kChained = 0x400;

constexpr int kAnsiLayout = 2;
constexpr HRESULT kNotSupported = static_cast<HRESULT>(0x80004001);
constexpr HRESULT kOutOfMemory = static_cast<HRESULT>(0x8007000e);

using Bytes = const std::uint8_t;

std::uint16_t word_at(Bytes* const p) {
    std::uint16_t value = 0;
    std::memcpy(&value, p, sizeof value);
    return value;
}

const std::uint8_t* pointer_at(Bytes* const p) {
    const std::uint8_t* value = nullptr;
    std::memcpy(&value, p, sizeof value);
    return value;
}

unsigned kind_of(Bytes* const entry) { return word_at(entry + 2) & 0xfu; }

unsigned type_of(Bytes* const entry) { return word_at(entry + 2); }

std::uint16_t field_count(Bytes* const descriptor) {
    return word_at(descriptor + 6);
}

std::uint16_t record_size(Bytes* const descriptor) {
    return word_at(descriptor + 2);
}

Bytes* first_entry(Bytes* const descriptor) { return descriptor + 0xc; }

// The length of an entry (the reference's field-size routine).
std::size_t entry_size(Bytes* const entry) {
    const unsigned type = type_of(entry);
    const std::size_t floor = (type & kChained) != 0 ? 6 : 0;
    std::size_t size = 0;
    switch (type & 0xfu) {
        case 1:
        case 2:
        case 3:
        case 11:
            size = (type & kAnsiOffset) != 0 ? 8 : 4;
            return size > floor ? size : floor;
        case 6:
            return 4 > floor ? 4 : floor;
        case 4:
        case 10:
            return 10 > floor ? 10 : floor;
        case 8:
            return 6;
        case 9:
            return 0x1c;
        default:
            return 0;
    }
}

const std::uint8_t* nested_descriptor(Bytes* const entry) {
    return pointer_at(entry + 8);
}

// Whether every entry (and nested descriptor) is one this file handles.
bool supported(Bytes* const descriptor, const int depth = 0) {
    if (descriptor == nullptr || depth > 32) {
        return descriptor == nullptr;
    }
    Bytes* entry = first_entry(descriptor);
    for (unsigned i = 0; i < field_count(descriptor); ++i) {
        const unsigned type = type_of(entry);
        const unsigned kind = type & 0xfu;
        if (kind == kArray || kind == kOther6 || (type & kChained) != 0) {
            return false;
        }
        if (kind == kRecord && (type & kHasNested) != 0 &&
            !supported(nested_descriptor(entry), depth + 1)) {
            return false;
        }
        entry += entry_size(entry);
    }
    return true;
}

// The address of a field: the ANSI layout of an entry with 0x2000 has its own
// offset at +6.
std::uint8_t* field_address(Bytes* const entry, std::uint8_t* const record,
                            const int layout) {
    if ((layout & kAnsiLayout) != 0 && (type_of(entry) & kAnsiOffset) != 0) {
        return record + word_at(entry + 6);
    }
    return record + word_at(entry);
}

void destroy_range(Bytes* descriptor, std::uint8_t* record,
                   std::size_t start, std::size_t end, int layout);

void destroy_field(Bytes* const entry, std::uint8_t* const record,
                   const int layout) {
    std::uint8_t* const address = field_address(entry, record, layout);
    switch (kind_of(entry)) {
        case kString:
        case kFixedString: {
            BSTR text = nullptr;
            std::memcpy(&text, address, sizeof text);
            if (text != nullptr) {
                SysFreeString(text);
                const BSTR none = nullptr;
                std::memcpy(address, &none, sizeof none);
            }
            break;
        }
        case kVariant:
            VariantClear(reinterpret_cast<VARIANT*>(address));
            break;
        case kObject: {
            IUnknown* object = nullptr;
            std::memcpy(&object, address, sizeof object);
            if (object != nullptr) {
                IUnknown* const none = nullptr;
                std::memcpy(address, &none, sizeof none);
                object->Release();
            }
            break;
        }
        case kRecord: {
            if ((type_of(entry) & kHasNested) == 0) {
                break;
            }
            Bytes* const nested = nested_descriptor(entry);
            if (nested == nullptr) {
                break;
            }
            const int nested_layout =
                ((type_of(entry) & kAnsiNested) != 0 ||
                 (layout & kAnsiLayout) != 0)
                    ? kAnsiLayout
                    : 0;
            destroy_range(nested, address, 0, 0xffff, nested_layout);
            break;
        }
        default:
            break;
    }
}

// Destroys the fields whose entries lie in [start, end) of the descriptor.
void destroy_range(Bytes* const descriptor, std::uint8_t* const record,
                   const std::size_t start, const std::size_t end,
                   const int layout) {
    Bytes* entry = first_entry(descriptor);
    for (unsigned i = 0; i < field_count(descriptor); ++i) {
        const std::size_t at = static_cast<std::size_t>(entry - descriptor);
        if (at >= start && at < end) {
            destroy_field(entry, record, layout);
        }
        entry += entry_size(entry);
    }
}

HRESULT copy_record(Bytes* const descriptor, std::uint8_t* const destination,
                    Bytes* const source_bytes) {
    std::uint8_t* const source = const_cast<std::uint8_t*>(source_bytes);
    if (destination == source) {
        return S_OK;
    }
    std::memcpy(destination, source, record_size(descriptor));
    Bytes* entry = first_entry(descriptor);
    for (unsigned i = 0; i < field_count(descriptor); ++i) {
        std::uint8_t* const from = source + word_at(entry);
        std::uint8_t* const to = destination + word_at(entry);
        HRESULT result = S_OK;
        switch (kind_of(entry)) {
            case kString:
            case kFixedString: {
                BSTR text = nullptr;
                std::memcpy(&text, from, sizeof text);
                BSTR copy = nullptr;
                if (text != nullptr) {
                    copy = SysAllocStringByteLen(
                        reinterpret_cast<const char*>(text),
                        SysStringByteLen(text));
                    if (copy == nullptr) {
                        result = kOutOfMemory;
                    }
                }
                std::memcpy(to, &copy, sizeof copy);
                break;
            }
            case kVariant: {
                VARIANT* const target = reinterpret_cast<VARIANT*>(to);
                if (V_VT(target) >= 8) {
                    V_VT(target) = VT_EMPTY;
                    result = VariantCopy(target,
                                         reinterpret_cast<VARIANT*>(from));
                }
                break;
            }
            case kObject: {
                IUnknown* object = nullptr;
                std::memcpy(&object, from, sizeof object);
                if (object != nullptr) {
                    object->AddRef();
                }
                break;
            }
            case kRecord:
                if ((type_of(entry) & kHasNested) != 0) {
                    result = copy_record(nested_descriptor(entry), to, from);
                }
                break;
            default:
                break;
        }
        if (FAILED(result)) {
            destroy_range(descriptor, destination, 0,
                          static_cast<std::size_t>(entry - descriptor), 0);
            return result;
        }
        entry += entry_size(entry);
    }
    return S_OK;
}

}  // namespace

extern "C" {

// The record held by the destination is destroyed, then replaced by a deep
// copy of the source; an error of the copy is raised. Returns the destination.
void* WINAPI __vbaRecAssign(const void* const descriptor,
                            void* const destination, const void* const source) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    if (!supported(layout)) {
        static_cast<void>(vbrun::raise_hresult(kNotSupported));
        return destination;
    }
    if (destination != source) {
        destroy_range(layout, static_cast<std::uint8_t*>(destination), 0,
                      0xffff, 0);
        const HRESULT result =
            copy_record(layout, static_cast<std::uint8_t*>(destination),
                        static_cast<const std::uint8_t*>(source));
        if (FAILED(result)) {
            static_cast<void>(vbrun::raise_hresult(result));
        }
    }
    return destination;
}

// The copy alone (the destination is taken as holding nothing); returns the
// HRESULT.
HRESULT WINAPI CopyRecord(void* const destination, const void* const source,
                          const void* const descriptor) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    if (!supported(layout)) {
        return kNotSupported;
    }
    return copy_record(layout, static_cast<std::uint8_t*>(destination),
                       static_cast<const std::uint8_t*>(source));
}

// Frees the strings, Variants and objects of a record and nested records.
void* WINAPI __vbaRecDestruct(const void* const descriptor,
                              void* const record) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    if (!supported(layout)) {
        static_cast<void>(vbrun::raise_hresult(kNotSupported));
        return record;
    }
    destroy_range(layout, static_cast<std::uint8_t*>(record), 0, 0xffff, 0);
    return record;
}

// The same for a record in the ANSI layout (field offsets from +6 where the
// entry has them).
void* WINAPI __vbaRecDestructAnsi(const void* const descriptor,
                                  void* const record) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    if (!supported(layout)) {
        static_cast<void>(vbrun::raise_hresult(kNotSupported));
        return record;
    }
    destroy_range(layout, static_cast<std::uint8_t*>(record), 0, 0xffff,
                  kAnsiLayout);
    return record;
}

}  // extern "C"
