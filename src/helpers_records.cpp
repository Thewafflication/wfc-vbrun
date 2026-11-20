// REQ-0005: the record helpers that copy, destroy and convert a user-defined
// type from its descriptor: __vbaRecAssign, CopyRecord, __vbaRecDestruct,
// __vbaRecDestructAnsi, __vbaRecUniToAnsi, __vbaRecAnsiToUni,
// __vbaAryRecCopy and __vbaCVarAryUdt. Written from the disassembly of the
// installed MSVBVM60 6.00.9848 and checked against the descriptors the Visual
// Basic 6 compiler emits (probe programs built with VB6.EXE /make).
//
// The descriptor: its own length (UInt16 at +0), the record size (+2), the
// number of fields (+6), the size of the ANSI layout (+8), then the field
// entries from +0xc, each of its own length. An entry is the offset of the
// field (+0), a type word (+2: the kind in the low nibble, flags above it) and
// more words after that, by kind:
//   1, 4   string (a BSTR in the record); entry 4 bytes, kind 4 is 10
//   2      Variant                        4
//   3      object (IUnknown pointer)      4
//   5      array, see below
//   6      a locked array reference       4 (destroying unlocks it)
//   7      nothing                        0
//   8      a pointer of the 0x400 chain   6
//   9      nested record                  0x1c, the nested descriptor's
//                                         address at +8 when 0x20 is set
//   10     fixed-length string            10, its length at +8
//   11     raw bytes                      4 (8 with 0x2000: the length at
//                                         +4, used by the ANSI conversion)
// A type word with 0x2000 gives the field its own offset in the ANSI layout
// at +6 and widens kinds 1, 2, 3 and 11 to 8 bytes; 0x400 puts the field
// behind a chain of pointers (the number of links at +4) that starts at the
// last pointer of the record, and makes the entry at least 6 bytes.
//
// An array entry (kind 5) has the element's type word at +8. A dynamic array
// (0x100) holds a SAFEARRAY pointer in the record; its entry is 10 bytes, 14
// for fixed-length string elements (the length at +0xc) and 0x20 for record
// elements (the element descriptor at +0xc). A fixed-size array (0x80) lies
// in the record; its entry carries a SAFEARRAY header for the shape at +0x10
// (+0x1c for record elements), and is 0x28 bytes (0x38 for record elements)
// plus 4 when the header's features have 0xe0 bits plus 8 for every dimension
// after the first.
//
// Copying fills the destination with the source bytes and then copies each
// string, Variant, array and nested record deeply and counts each object;
// destroying frees them again. The descriptor's addresses are read with
// pointer-sized loads and the shape headers by their 32-bit offsets, so the
// helpers behave the same on every target the library is built for.

#include <cstdint>
#include <cstring>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr unsigned kString = 1;
constexpr unsigned kVariant = 2;
constexpr unsigned kObject = 3;
constexpr unsigned kString4 = 4;
constexpr unsigned kArray = 5;
constexpr unsigned kLockedArray = 6;
constexpr unsigned kChainLink = 8;
constexpr unsigned kRecord = 9;
constexpr unsigned kFixedString = 10;
constexpr unsigned kRaw = 11;

constexpr unsigned kHasNested = 0x20;
constexpr unsigned kNestedInline = 0x40;
constexpr unsigned kRecordElements = 0x60;
constexpr unsigned kFixedArray = 0x80;
constexpr unsigned kDynamicArray = 0x100;
constexpr unsigned kChained = 0x400;
constexpr unsigned kAnsiNested = 0x1000;
constexpr unsigned kAnsiOffset = 0x2000;

constexpr int kSignedOffset = 1;
constexpr int kAnsiLayout = 2;
constexpr int kTypeMismatch = 13;
constexpr HRESULT kOutOfMemory = static_cast<HRESULT>(0x8007000e);
constexpr USHORT kFixedFeatures = 0x17;
constexpr USHORT kTypedFeatures = 0xe0;

using Bytes = const std::uint8_t;

std::uint16_t word_at(Bytes* const p) {
    std::uint16_t value = 0;
    std::memcpy(&value, p, sizeof value);
    return value;
}

std::uint32_t dword_at(Bytes* const p) {
    std::uint32_t value = 0;
    std::memcpy(&value, p, sizeof value);
    return value;
}

const std::uint8_t* pointer_at(Bytes* const p) {
    const std::uint8_t* value = nullptr;
    std::memcpy(&value, p, sizeof value);
    return value;
}

template <typename T>
T load(const void* const p) {
    T value{};
    std::memcpy(&value, p, sizeof value);
    return value;
}

template <typename T>
void store(void* const p, const T value) {
    std::memcpy(p, &value, sizeof value);
}

unsigned type_of(Bytes* const entry) {
    return word_at(entry + 2);
}

unsigned kind_of(Bytes* const entry) {
    return type_of(entry) & 0xfu;
}

std::uint16_t field_count(Bytes* const descriptor) {
    return word_at(descriptor + 6);
}

std::uint16_t record_size(Bytes* const descriptor) {
    return word_at(descriptor + 2);
}

std::uint16_t ansi_size(Bytes* const descriptor) {
    return word_at(descriptor + 8);
}

Bytes* first_entry(Bytes* const descriptor) {
    return descriptor + 0xc;
}

// --- arrays ----------------------------------------------------------------

unsigned element_type(Bytes* const entry) {
    return word_at(entry + 8);
}

bool record_elements(Bytes* const entry) {
    return (element_type(entry) & kRecordElements) != 0;
}

// The shape header of a fixed-size array entry (the 32-bit SAFEARRAY layout:
// dimensions at +0, features at +2, element size at +4, bounds from +0x10).
Bytes* shape_of(Bytes* const entry) {
    return entry + (record_elements(entry) ? 0x1c : 0x10);
}

// Whether the elements of an array hold anything to free or copy.
bool elements_need_work(const unsigned element) {
    const unsigned kind = element & 0xfu;
    return kind == kString || kind == kVariant || kind == kObject ||
           (element & kRecordElements) != 0;
}

std::size_t array_entry_size(Bytes* const entry) {
    const unsigned type = type_of(entry);
    const unsigned element = element_type(entry);
    if ((type & kDynamicArray) != 0) {
        if ((element & kRecordElements) != 0) {
            return 0x20;
        }
        return (element & 0xfu) == kFixedString ? 0xe : 0xa;
    }
    if (!elements_need_work(element) && (type & kFixedArray) != 0 &&
        (type & 0x2200) == 0) {
        return 0;
    }
    Bytes* const shape = shape_of(entry);
    const unsigned dimensions = word_at(shape);
    const unsigned features = word_at(shape + 2);
    std::size_t size = (element & kRecordElements) != 0 ? 0x38 : 0x28;
    if ((features & kTypedFeatures) != 0) {
        size += 4;
    }
    if (dimensions > 0) {
        size += 8 * (dimensions - 1);
    }
    return size;
}

// The length of an entry (the reference's field-size routine).
std::size_t entry_size(Bytes* const entry) {
    const unsigned type = type_of(entry);
    const std::size_t floor = (type & kChained) != 0 ? 6 : 0;
    std::size_t size = 0;
    switch (type & 0xfu) {
        case kString:
        case kVariant:
        case kObject:
        case kRaw:
            size = (type & kAnsiOffset) != 0 ? 8 : 4;
            return size > floor ? size : floor;
        case kLockedArray:
            return 4 > floor ? 4 : floor;
        case kString4:
        case kFixedString:
            return 10 > floor ? 10 : floor;
        case kArray:
            return array_entry_size(entry);
        case kChainLink:
            return 6;
        case kRecord:
            return 0x1c;
        default:
            return 0;
    }
}

const std::uint8_t* nested_descriptor(Bytes* const entry) {
    return pointer_at(entry + 8);
}

// The address of a field. A chained field (0x400) is found by following the
// pointer in the last slot of the record as many times as the entry says; the
// ANSI layout of an entry with 0x2000 has its own offset at +6.
std::uint8_t* field_address(Bytes* const descriptor, Bytes* const entry,
                            std::uint8_t* const record, const int layout) {
    const unsigned type = type_of(entry);
    if ((type & kChained) != 0) {
        unsigned links = word_at(entry + 4);
        const bool link_itself = (type & 0xfu) == kChainLink;
        if (link_itself && links > 0) {
            --links;
        }
        std::uint8_t* at = record + record_size(descriptor) - sizeof(void*);
        for (; links > 0; --links) {
            at = load<std::uint8_t*>(at);
        }
        return link_itself ? at : at + word_at(entry);
    }
    if ((layout & kSignedOffset) != 0) {
        return record + static_cast<std::int16_t>(word_at(entry));
    }
    if ((layout & kAnsiLayout) != 0 && (type & kAnsiOffset) != 0) {
        return record + word_at(entry + 6);
    }
    return record + word_at(entry);
}

// The number of elements of a fixed-size array (the product of its bounds).
ULONG shape_count(Bytes* const shape) {
    ULONG count = 1;
    for (unsigned i = 0; i < word_at(shape); ++i) {
        count *= dword_at(shape + 0x10 + 8 * i);
    }
    return word_at(shape) == 0 ? 0 : count;
}

ULONG array_count(const SAFEARRAY* const array) {
    if (array->cDims == 0) {
        return 0;
    }
    ULONG count = 1;
    for (USHORT i = 0; i < array->cDims; ++i) {
        count *= array->rgsabound[i].cElements;
    }
    return count;
}

void destroy_range(Bytes* descriptor, std::uint8_t* record, std::size_t start,
                   std::size_t end, int layout);
HRESULT copy_record(Bytes* descriptor, std::uint8_t* destination,
                    Bytes* source);

// Frees what one element of an array holds.
void destroy_element(const unsigned element, Bytes* const nested,
                     std::uint8_t* const at, const int layout) {
    switch (element & 0xfu) {
        case kString:
        case kString4: {
            const BSTR text = load<BSTR>(at);
            if (text != nullptr) {
                SysFreeString(text);
                store<BSTR>(at, nullptr);
            }
            break;
        }
        case kVariant:
            VariantClear(reinterpret_cast<VARIANT*>(at));
            break;
        case kObject: {
            IUnknown* const object = load<IUnknown*>(at);
            if (object != nullptr) {
                store<IUnknown*>(at, nullptr);
                object->Release();
            }
            break;
        }
        default:
            if ((element & kRecordElements) != 0 && nested != nullptr) {
                destroy_range(nested, at, 0, 0xffff, layout);
            }
            break;
    }
}

// Makes an element that was filled with the bytes of its source a copy of its
// own.
HRESULT copy_element(const unsigned element, Bytes* const nested,
                     std::uint8_t* const to, Bytes* const from) {
    switch (element & 0xfu) {
        case kString:
        case kString4: {
            const BSTR text = load<BSTR>(from);
            BSTR copy = nullptr;
            if (text != nullptr) {
                copy =
                    SysAllocStringByteLen(reinterpret_cast<const char*>(text),
                                          SysStringByteLen(text));
                if (copy == nullptr) {
                    store<BSTR>(to, nullptr);
                    return kOutOfMemory;
                }
            }
            store<BSTR>(to, copy);
            return S_OK;
        }
        case kVariant: {
            VARIANT* const target = reinterpret_cast<VARIANT*>(to);
            V_VT(target) = VT_EMPTY;
            return VariantCopy(target, reinterpret_cast<VARIANT*>(
                                           const_cast<std::uint8_t*>(from)));
        }
        case kObject: {
            IUnknown* const object = load<IUnknown*>(from);
            if (object != nullptr) {
                object->AddRef();
            }
            return S_OK;
        }
        default:
            if ((element & kRecordElements) != 0 && nested != nullptr) {
                return copy_record(nested, to, from);
            }
            return S_OK;
    }
}

// A dynamic array of the same shape and features as the source with its data
// allocated (and not filled).
HRESULT same_shape(const SAFEARRAY* const source, SAFEARRAY*& result,
                   const ULONG element_size) {
    result = nullptr;
    HRESULT hr = SafeArrayAllocDescriptor(source->cDims, &result);
    if (FAILED(hr)) {
        return hr;
    }
    result->cbElements = element_size;
    result->fFeatures = static_cast<USHORT>(
        source->fFeatures & ~(FADF_AUTO | FADF_STATIC | FADF_EMBEDDED));
    for (USHORT i = 0; i < source->cDims; ++i) {
        result->rgsabound[i] = source->rgsabound[i];
    }
    hr = SafeArrayAllocData(result);
    if (FAILED(hr)) {
        SafeArrayDestroyDescriptor(result);
        result = nullptr;
    }
    return hr;
}

// A copy of a dynamic array of records: the bytes, then each element deeply.
HRESULT clone_record_array(const SAFEARRAY* const source, Bytes* const nested,
                           SAFEARRAY*& result) {
    HRESULT hr = same_shape(source, result, source->cbElements);
    if (FAILED(hr)) {
        return hr;
    }
    const ULONG count = array_count(source);
    auto* const to = static_cast<std::uint8_t*>(result->pvData);
    const auto* const from = static_cast<const std::uint8_t*>(source->pvData);
    for (ULONG i = 0; i < count && SUCCEEDED(hr); ++i) {
        const std::size_t at = static_cast<std::size_t>(i) * source->cbElements;
        hr = copy_record(nested, to + at, from + at);
        if (FAILED(hr)) {
            for (ULONG j = 0; j < i; ++j) {
                destroy_range(
                    nested,
                    to + static_cast<std::size_t>(j) * source->cbElements, 0,
                    0xffff, 0);
            }
        }
    }
    if (FAILED(hr)) {
        SafeArrayDestroyData(result);
        SafeArrayDestroyDescriptor(result);
        result = nullptr;
    }
    return hr;
}

// Frees a dynamic array: the records of a record array one by one, the rest
// through the array's own features.
void destroy_dynamic(SAFEARRAY* const array, const unsigned element,
                     Bytes* const nested, const int layout) {
    if (array->pvData != nullptr) {
        if ((element & kHasNested) != 0 && nested != nullptr) {
            const ULONG count = array_count(array);
            auto* const data = static_cast<std::uint8_t*>(array->pvData);
            for (ULONG i = 0; i < count; ++i) {
                destroy_range(
                    nested,
                    data + static_cast<std::size_t>(i) * array->cbElements, 0,
                    0xffff, layout);
            }
        }
        SafeArrayDestroyData(array);
    }
    SafeArrayDestroyDescriptor(array);
}

const std::uint8_t* array_nested(Bytes* const entry) {
    return record_elements(entry) ? pointer_at(entry + 0xc) : nullptr;
}

void destroy_array_field(Bytes* const entry, std::uint8_t* const address,
                         const int layout) {
    const unsigned element = element_type(entry);
    Bytes* const nested = array_nested(entry);
    if ((type_of(entry) & kDynamicArray) != 0) {
        SAFEARRAY* const array = load<SAFEARRAY*>(address);
        if (array != nullptr) {
            destroy_dynamic(array, element, nested, layout);
            store<SAFEARRAY*>(address, nullptr);
        }
        return;
    }
    if (!elements_need_work(element)) {
        return;
    }
    Bytes* const shape = shape_of(entry);
    const ULONG count = shape_count(shape);
    const std::uint32_t size = dword_at(shape + 4);
    for (ULONG i = 0; i < count; ++i) {
        destroy_element(element, nested,
                        address + static_cast<std::size_t>(i) * size, layout);
    }
}

HRESULT copy_array_field(Bytes* const entry, std::uint8_t* const to,
                         Bytes* const from) {
    const unsigned element = element_type(entry);
    Bytes* const nested = array_nested(entry);
    if ((type_of(entry) & kDynamicArray) != 0) {
        store<SAFEARRAY*>(to, nullptr);
        SAFEARRAY* const source = load<SAFEARRAY*>(from);
        if (source == nullptr) {
            return S_OK;
        }
        SAFEARRAY* copy = nullptr;
        const HRESULT hr = (element & kRecordElements) != 0 && nested != nullptr
                               ? clone_record_array(source, nested, copy)
                               : SafeArrayCopy(source, &copy);
        store<SAFEARRAY*>(to, copy);
        return hr;
    }
    if (!elements_need_work(element)) {
        return S_OK;
    }
    Bytes* const shape = shape_of(entry);
    const ULONG count = shape_count(shape);
    const std::uint32_t size = dword_at(shape + 4);
    for (ULONG i = 0; i < count; ++i) {
        const std::size_t at = static_cast<std::size_t>(i) * size;
        const HRESULT hr = copy_element(element, nested, to + at, from + at);
        if (FAILED(hr)) {
            for (ULONG j = 0; j < i; ++j) {
                destroy_element(element, nested,
                                to + static_cast<std::size_t>(j) * size, 0);
            }
            return hr;
        }
    }
    return S_OK;
}

// --- destroy and copy ------------------------------------------------------

void destroy_field(Bytes* const descriptor, Bytes* const entry,
                   std::uint8_t* const record, const int layout) {
    std::uint8_t* const address =
        field_address(descriptor, entry, record, layout);
    switch (kind_of(entry)) {
        case kString:
        case kString4:
        case kVariant:
        case kObject:
            destroy_element(kind_of(entry), nullptr, address, layout);
            break;
        case kArray:
            destroy_array_field(entry, address, layout);
            break;
        case kLockedArray: {
            SAFEARRAY* const array = load<SAFEARRAY*>(address);
            if (array != nullptr) {
                SafeArrayUnlock(array);
                store<SAFEARRAY*>(address, nullptr);
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
            const int nested_layout = ((type_of(entry) & kAnsiNested) != 0 ||
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
            destroy_field(descriptor, entry, record, layout);
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
        std::uint8_t* const from = field_address(descriptor, entry, source, 0);
        std::uint8_t* const to =
            field_address(descriptor, entry, destination, 0);
        HRESULT result = S_OK;
        switch (kind_of(entry)) {
            case kString:
            case kString4:
            case kVariant:
            case kObject:
                result = copy_element(kind_of(entry), nullptr, to, from);
                break;
            case kArray:
                result = copy_array_field(entry, to, from);
                break;
            case kLockedArray: {
                SAFEARRAY* const array = load<SAFEARRAY*>(from);
                if (array != nullptr) {
                    result = SafeArrayLock(array);
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

// --- the ANSI layout ---------------------------------------------------------

HRESULT convert_record(Bytes* descriptor, std::uint8_t* destination,
                       Bytes* source, bool to_ansi);

std::size_t ansi_offset(Bytes* const entry) {
    return (type_of(entry) & kAnsiOffset) != 0 ? word_at(entry + 6)
                                               : word_at(entry);
}

// A string between Unicode and ANSI (a BSTR that holds the ANSI bytes).
BSTR convert_string(const BSTR text, const bool to_ansi) {
    if (text == nullptr) {
        return nullptr;
    }
    if (to_ansi) {
        const int length = static_cast<int>(SysStringLen(text));
        const int bytes = WideCharToMultiByte(CP_ACP, 0, text, length, nullptr,
                                              0, nullptr, nullptr);
        std::string buffer(static_cast<std::size_t>(bytes > 0 ? bytes : 0),
                           '\0');
        if (bytes > 0) {
            WideCharToMultiByte(CP_ACP, 0, text, length, buffer.data(), bytes,
                                nullptr, nullptr);
        }
        return SysAllocStringByteLen(buffer.data(),
                                     static_cast<UINT>(buffer.size()));
    }
    const int bytes = static_cast<int>(SysStringByteLen(text));
    const char* const narrow = reinterpret_cast<const char*>(text);
    const int length =
        MultiByteToWideChar(CP_ACP, 0, narrow, bytes, nullptr, 0);
    std::wstring buffer(static_cast<std::size_t>(length > 0 ? length : 0),
                        L'\0');
    if (length > 0) {
        MultiByteToWideChar(CP_ACP, 0, narrow, bytes, buffer.data(), length);
    }
    return SysAllocStringLen(buffer.data(), static_cast<UINT>(buffer.size()));
}

// A fixed-length string of `length` characters between its Unicode and its
// ANSI form (one byte a character in the ANSI layout).
void convert_fixed(std::uint8_t* const to, Bytes* const from,
                   const unsigned length, const bool to_ansi) {
    if (length == 0) {
        return;
    }
    if (to_ansi) {
        std::wstring text(length, L'\0');
        std::memcpy(text.data(), from, length * sizeof(wchar_t));
        std::string bytes(length, '\0');
        WideCharToMultiByte(CP_ACP, 0, text.data(), static_cast<int>(length),
                            bytes.data(), static_cast<int>(length), nullptr,
                            nullptr);
        std::memcpy(to, bytes.data(), length);
    } else {
        std::wstring text(length, L'\0');
        MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(from),
                            static_cast<int>(length), text.data(),
                            static_cast<int>(length));
        std::memcpy(to, text.data(), length * sizeof(wchar_t));
    }
}

// The size of an element in the ANSI layout.
std::size_t ansi_element_size(const unsigned element, Bytes* const nested,
                              const unsigned length, const std::size_t size) {
    switch (element & 0xfu) {
        case kFixedString:
            return length;
        default:
            if ((element & kRecordElements) != 0 && nested != nullptr) {
                return ansi_size(nested);
            }
            return size;
    }
}

HRESULT convert_element(const unsigned element, Bytes* const nested,
                        const unsigned length, std::uint8_t* const to,
                        Bytes* const from, const std::size_t size,
                        const bool to_ansi) {
    switch (element & 0xfu) {
        case kString:
        case kString4: {
            const BSTR converted = convert_string(load<BSTR>(from), to_ansi);
            store<BSTR>(to, converted);
            return S_OK;
        }
        case kVariant: {
            VARIANT* const target = reinterpret_cast<VARIANT*>(to);
            V_VT(target) = VT_EMPTY;
            return VariantCopy(target, reinterpret_cast<VARIANT*>(
                                           const_cast<std::uint8_t*>(from)));
        }
        case kObject: {
            IUnknown* const object = load<IUnknown*>(from);
            store<IUnknown*>(to, object);
            if (object != nullptr) {
                object->AddRef();
            }
            return S_OK;
        }
        case kFixedString:
            convert_fixed(to, from, length, to_ansi);
            return S_OK;
        default:
            if ((element & kRecordElements) != 0 && nested != nullptr) {
                return convert_record(nested, to, from, to_ansi);
            }
            std::memcpy(to, from, size);
            return S_OK;
    }
}

HRESULT convert_array_field(Bytes* const entry, std::uint8_t* const to,
                            Bytes* const from, const bool to_ansi) {
    const unsigned element = element_type(entry);
    Bytes* const nested = array_nested(entry);
    const unsigned length =
        (element & 0xfu) == kFixedString ? dword_at(entry + 0xc) : 0;
    if ((type_of(entry) & kDynamicArray) != 0) {
        store<SAFEARRAY*>(to, nullptr);
        SAFEARRAY* const source = load<SAFEARRAY*>(from);
        if (source == nullptr) {
            return S_OK;
        }
        const std::size_t from_size = source->cbElements;
        const std::size_t ansi =
            ansi_element_size(element, nested, length, from_size);
        std::size_t to_size = to_ansi ? ansi : from_size;
        if (!to_ansi) {
            to_size = (element & 0xfu) == kFixedString ? length * 2
                      : (element & kRecordElements) != 0 && nested != nullptr
                          ? record_size(nested)
                          : from_size;
        }
        SAFEARRAY* copy = nullptr;
        HRESULT hr = same_shape(source, copy, static_cast<ULONG>(to_size));
        if (FAILED(hr)) {
            return hr;
        }
        std::memset(copy->pvData, 0, array_count(source) * to_size);
        const ULONG count = array_count(source);
        for (ULONG i = 0; i < count && SUCCEEDED(hr); ++i) {
            hr = convert_element(
                element, nested, length,
                static_cast<std::uint8_t*>(copy->pvData) + i * to_size,
                static_cast<const std::uint8_t*>(source->pvData) +
                    i * from_size,
                from_size < to_size ? from_size : to_size, to_ansi);
        }
        store<SAFEARRAY*>(to, copy);
        return hr;
    }
    Bytes* const shape = shape_of(entry);
    const ULONG count = shape_count(shape);
    const std::size_t unicode = dword_at(shape + 4);
    const std::size_t ansi =
        ansi_element_size(element, nested, length, unicode);
    const std::size_t to_step = to_ansi ? ansi : unicode;
    const std::size_t from_step = to_ansi ? unicode : ansi;
    for (ULONG i = 0; i < count; ++i) {
        const HRESULT hr = convert_element(
            element, nested, length, to + i * to_step, from + i * from_step,
            to_step < from_step ? to_step : from_step, to_ansi);
        if (FAILED(hr)) {
            return hr;
        }
    }
    return S_OK;
}

// The record converted between its Unicode and its ANSI layout: the first
// bytes (as many as the ANSI layout has) are copied, then each field is
// converted. Until the first fixed-length string both layouts put a field at
// the same offset; after it the ANSI one is at the entry's +6.
HRESULT convert_record(Bytes* const descriptor, std::uint8_t* const destination,
                       Bytes* const source, const bool to_ansi) {
    std::memcpy(destination, source, ansi_size(descriptor));
    bool shifted = false;
    Bytes* entry = first_entry(descriptor);
    for (unsigned i = 0; i < field_count(descriptor); ++i) {
        const unsigned type = type_of(entry);
        std::size_t from_offset = word_at(entry);
        std::size_t to_offset = word_at(entry);
        if (shifted) {
            if (to_ansi) {
                to_offset = ansi_offset(entry);
            } else {
                from_offset = ansi_offset(entry);
            }
        }
        std::uint8_t* const to = destination + to_offset;
        Bytes* const from = source + from_offset;
        HRESULT hr = S_OK;
        switch (type & 0xfu) {
            case kString:
                hr = convert_element(kString, nullptr, 0, to, from, 0, to_ansi);
                break;
            case kVariant:
                if (shifted || V_VT(reinterpret_cast<VARIANT*>(to)) >= 8) {
                    hr = convert_element(kVariant, nullptr, 0, to, from, 0,
                                         to_ansi);
                }
                break;
            case kArray:
                hr = convert_array_field(entry, to, from, to_ansi);
                break;
            case kRecord:
                if ((type & kNestedInline) != 0) {
                    Bytes* const nested = nested_descriptor(entry);
                    std::memset(to, 0, ansi_size(nested));
                    hr = convert_record(nested, to, from, to_ansi);
                } else if (shifted) {
                    const std::size_t length =
                        (type & kHasNested) != 0
                            ? record_size(nested_descriptor(entry))
                            : word_at(entry + 4);
                    std::memcpy(to, from, length);
                }
                break;
            case kFixedString:
                convert_fixed(to, from, word_at(entry + 8), to_ansi);
                shifted = true;
                break;
            case kRaw:
                std::memcpy(to, from, word_at(entry + 4));
                break;
            default:
                break;
        }
        if (FAILED(hr)) {
            return hr;
        }
        entry += entry_size(entry);
    }
    return S_OK;
}

void raise_failure(const HRESULT result) {
    if (FAILED(result)) {
        static_cast<void>(vbrun::raise_hresult(result));
    }
}

}  // namespace

extern "C" {

// The record held by the destination is destroyed, then replaced by a deep
// copy of the source; an error of the copy is raised. Returns the destination.
void* WINAPI __vbaRecAssign(const void* const descriptor,
                            void* const destination, const void* const source) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    if (destination != source) {
        destroy_range(layout, static_cast<std::uint8_t*>(destination), 0,
                      0xffff, 0);
        raise_failure(copy_record(layout,
                                  static_cast<std::uint8_t*>(destination),
                                  static_cast<const std::uint8_t*>(source)));
    }
    return destination;
}

// The copy alone (the destination is taken as holding nothing); returns the
// HRESULT.
HRESULT WINAPI CopyRecord(void* const destination, const void* const source,
                          const void* const descriptor) {
    return copy_record(static_cast<const std::uint8_t*>(descriptor),
                       static_cast<std::uint8_t*>(destination),
                       static_cast<const std::uint8_t*>(source));
}

// Frees the strings, Variants, objects and arrays of a record and nested
// records.
void* WINAPI __vbaRecDestruct(const void* const descriptor,
                              void* const record) {
    destroy_range(static_cast<const std::uint8_t*>(descriptor),
                  static_cast<std::uint8_t*>(record), 0, 0xffff, 0);
    return record;
}

// The same for a record in the ANSI layout (field offsets from +6 where the
// entry has them).
void* WINAPI __vbaRecDestructAnsi(const void* const descriptor,
                                  void* const record) {
    destroy_range(static_cast<const std::uint8_t*>(descriptor),
                  static_cast<std::uint8_t*>(record), 0, 0xffff, kAnsiLayout);
    return record;
}

// A record made ANSI for a Declare'd procedure: what the ANSI record held is
// freed, then it receives the converted record (strings as ANSI strings,
// fixed-length strings one byte a character). Returns the ANSI record.
void* WINAPI __vbaRecUniToAnsi(const void* const descriptor, void* const ansi,
                               const void* const unicode) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    destroy_range(layout, static_cast<std::uint8_t*>(ansi), 0, 0xffff,
                  kAnsiLayout);
    raise_failure(convert_record(layout, static_cast<std::uint8_t*>(ansi),
                                 static_cast<const std::uint8_t*>(unicode),
                                 true));
    return ansi;
}

// The way back after the call. Returns the Unicode record.
void* WINAPI __vbaRecAnsiToUni(const void* const descriptor,
                               void* const unicode, const void* const ansi) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    destroy_range(layout, static_cast<std::uint8_t*>(unicode), 0, 0xffff, 0);
    raise_failure(convert_record(layout, static_cast<std::uint8_t*>(unicode),
                                 static_cast<const std::uint8_t*>(ansi),
                                 false));
    return unicode;
}

// An array of records assigned (a = b): the destination's array is freed,
// record by record, and replaced by a deep copy of the source. A fixed-size
// destination is error 13; the same array on both sides does nothing.
void WINAPI __vbaAryRecCopy(const void* const descriptor,
                            SAFEARRAY** const destination,
                            SAFEARRAY** const source) {
    const std::uint8_t* const layout =
        static_cast<const std::uint8_t*>(descriptor);
    SAFEARRAY* const old = *destination;
    if (old != nullptr) {
        if ((old->fFeatures & kFixedFeatures) != 0) {
            static_cast<void>(vbrun::raise(kTypeMismatch));
            return;
        }
        if (*source == old) {
            return;
        }
        destroy_dynamic(
            old,
            (old->fFeatures & kTypedFeatures) == 0 && layout != nullptr
                ? kRecord | kHasNested
                : 0,
            layout, 0);
        *destination = nullptr;
    }
    SAFEARRAY* const from = *source;
    if (from == nullptr) {
        return;
    }
    SAFEARRAY* copy = nullptr;
    const HRESULT hr =
        layout != nullptr && (from->fFeatures & kTypedFeatures) == 0
            ? clone_record_array(from, layout, copy)
            : SafeArrayCopy(from, &copy);
    if (FAILED(hr)) {
        static_cast<void>(vbrun::raise_hresult(hr));
        return;
    }
    copy->fFeatures = static_cast<USHORT>(copy->fFeatures & ~kFixedFeatures);
    *destination = copy;
}

// An array of records (or of a typed interface) put in a Variant. An array
// that does not exist yet is made empty, carrying the record information
// (VT_RECORD: its size becomes the element size) or the interface identifier.
// The Variant takes the array, or a reference to the slot for VT_BYREF.
VARIANT* WINAPI __vbaCVarAryUdt(void* const information, VARIANT* const result,
                                const LONG type, SAFEARRAY** const slot) {
    if (*slot == nullptr) {
        const VARTYPE element = static_cast<VARTYPE>(type & 0xfff);
        const HRESULT hr = SafeArrayAllocDescriptorEx(element, 1, slot);
        if (FAILED(hr)) {
            static_cast<void>(vbrun::raise_hresult(hr));
        } else if (element == VT_RECORD) {
            auto* const record = static_cast<IRecordInfo*>(information);
            SafeArraySetRecordInfo(*slot, record);
            if (record != nullptr) {
                record->GetSize(&(*slot)->cbElements);
            }
        } else {
            SafeArraySetIID(*slot, *static_cast<const IID*>(information));
            (*slot)->cbElements = sizeof(void*);
            if (element == VT_DISPATCH) {
                (*slot)->fFeatures |= FADF_DISPATCH;
            } else if (element == VT_UNKNOWN) {
                (*slot)->fFeatures |= FADF_UNKNOWN;
            }
        }
    }
    V_VT(result) = static_cast<VARTYPE>(type);
    if ((type & VT_BYREF) != 0) {
        V_ARRAYREF(result) = slot;
    } else {
        V_ARRAY(result) = *slot;
    }
    return result;
}

}  // extern "C"
