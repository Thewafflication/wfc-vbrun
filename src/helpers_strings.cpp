// REQ-0102: the string functions of the language runtime on Strings
// (Left, LeftB, Right, RightB, UCase, LCase, Trim, LTrim, RTrim, Space, Asc,
// AscB, AscW, Chr, ChrB, ChrW, StrReverse).
//
// The reference raises a Visual Basic error through the frames of the
// running program (and fails outside one); here the error is remembered for
// the thread (vbrun::raise) and the helper returns an empty result.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kOutOfStringSpace = 14;

// An allocated empty String: what the reference gives for an empty result.
BSTR empty_string() {
    BSTR text = SysAllocStringByteLen(nullptr, 0);
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
    }
    return text;
}

BSTR bytes_of(const void* const data, const UINT bytes) {
    BSTR text = SysAllocStringByteLen(static_cast<const char*>(data), bytes);
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
    }
    return text;
}

BSTR characters_of(const OLECHAR* const data, const UINT length) {
    BSTR text = SysAllocStringLen(data, length);
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
    }
    return text;
}

UINT byte_length(const BSTR text) {
    return text == nullptr ? 0U : SysStringByteLen(text);
}

UINT length(const BSTR text) {
    return text == nullptr ? 0U : SysStringLen(text);
}

// Maps the case of a String with the user's locale.
BSTR map_case(const BSTR text, const DWORD flag) {
    const UINT count = length(text);
    if (count == 0U) {
        return empty_string();
    }
    BSTR result = SysAllocStringLen(nullptr, count);
    if (result == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
        return nullptr;
    }
    if (LCMapStringW(LOCALE_USER_DEFAULT, flag, text, static_cast<int>(count),
                     result, static_cast<int>(count)) == 0) {
        std::memcpy(result, text, static_cast<std::size_t>(count) * 2U);
    }
    return result;
}

}  // namespace

extern "C" {

// LeftB(text, bytes) and RightB(text, bytes) count bytes: an odd count leaves
// a String with a last byte of its own.
BSTR WINAPI rtcLeftBstr(const BSTR text, const LONG bytes) {
    if (bytes < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    const UINT have = byte_length(text);
    const UINT take = std::min(static_cast<UINT>(bytes), have);
    return take == 0U ? empty_string() : bytes_of(text, take);
}

BSTR WINAPI rtcRightBstr(const BSTR text, const LONG bytes) {
    if (bytes < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    const UINT have = byte_length(text);
    const UINT take = std::min(static_cast<UINT>(bytes), have);
    return take == 0U
               ? empty_string()
               : bytes_of(reinterpret_cast<const char*>(text) + (have - take),
                          take);
}

// Left(text, characters) and Right(text, characters).
BSTR WINAPI rtcLeftCharBstr(const BSTR text, const LONG count) {
    if (count < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    const UINT take = std::min(static_cast<UINT>(count), length(text));
    return take == 0U ? empty_string() : characters_of(text, take);
}

BSTR WINAPI rtcRightCharBstr(const BSTR text, const LONG count) {
    if (count < 0) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    const UINT have = length(text);
    const UINT take = std::min(static_cast<UINT>(count), have);
    return take == 0U ? empty_string()
                      : characters_of(text + (have - take), take);
}

BSTR WINAPI rtcUpperCaseBstr(const BSTR text) {
    return map_case(text, LCMAP_UPPERCASE);
}

BSTR WINAPI rtcLowerCaseBstr(const BSTR text) {
    return map_case(text, LCMAP_LOWERCASE);
}

// Trim removes spaces (U+0020) only.
BSTR WINAPI rtcLeftTrimBstr(const BSTR text) {
    const UINT count = length(text);
    UINT first = 0U;
    while (first < count && text[first] == L' ') {
        ++first;
    }
    return first == count ? empty_string()
                          : characters_of(text + first, count - first);
}

BSTR WINAPI rtcRightTrimBstr(const BSTR text) {
    UINT count = length(text);
    while (count > 0U && text[count - 1U] == L' ') {
        --count;
    }
    return count == 0U ? empty_string() : characters_of(text, count);
}

BSTR WINAPI rtcTrimBstr(const BSTR text) {
    const UINT count = length(text);
    UINT first = 0U;
    while (first < count && text[first] == L' ') {
        ++first;
    }
    UINT last = count;
    while (last > first && text[last - 1U] == L' ') {
        --last;
    }
    return last == first ? empty_string()
                         : characters_of(text + first, last - first);
}

BSTR WINAPI rtcSpaceBstr(const LONG count) {
    if (count < 0 || count > 0x3FFFFFFF) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    BSTR result = SysAllocStringLen(nullptr, static_cast<UINT>(count));
    if (result == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
        return nullptr;
    }
    std::wmemset(result, L' ', static_cast<std::size_t>(count));
    return result;
}

// Asc(text): the code of the first character in the ANSI code page (the lead
// and trail byte of a double-byte character make one number).
LONG WINAPI rtcAnsiValueBstr(const BSTR text) {
    if (length(text) == 0U) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0;
    }
    const wchar_t first = text[0];
    if (first < 0x80) {
        return first;
    }
    char bytes[4] = {};
    const int made = WideCharToMultiByte(CP_ACP, 0, &first, 1, bytes,
                                         sizeof(bytes), nullptr, nullptr);
    if (made >= 2) {
        return (static_cast<unsigned char>(bytes[0]) << 8) |
               static_cast<unsigned char>(bytes[1]);
    }
    return made == 1 ? static_cast<unsigned char>(bytes[0]) : 0;
}

// AscW(text): the first UTF-16 unit.
LONG WINAPI rtcCharValueBstr(const BSTR text) {
    if (length(text) == 0U) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0;
    }
    return text[0];
}

// AscB(text): the first byte.
LONG WINAPI rtcByteValueBstr(const BSTR text) {
    if (byte_length(text) == 0U) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return 0;
    }
    return static_cast<unsigned char>(reinterpret_cast<const char*>(text)[0]);
}

// ChrW(code): one UTF-16 unit (-32768 to 65535).
BSTR WINAPI rtcBstrFromChar(const LONG code) {
    if (code > 0xFFFF || code < -0x8000) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return nullptr;
    }
    const OLECHAR unit = static_cast<OLECHAR>(code);
    return characters_of(&unit, 1U);
}

// ChrB(code): a String of one byte.
BSTR WINAPI rtcBstrFromByte(const std::uint8_t code) {
    return bytes_of(&code, 1U);
}

BSTR WINAPI rtcStrReverse(const BSTR text) {
    if (text == nullptr) {
        return nullptr;
    }
    const UINT count = SysStringLen(text);
    BSTR result = SysAllocStringLen(nullptr, count);
    if (result == nullptr) {
        static_cast<void>(vbrun::raise(kOutOfStringSpace));
        return nullptr;
    }
    for (UINT i = 0; i < count; ++i) {
        result[i] = text[count - 1U - i];
    }
    return result;
}

}  // extern "C"
