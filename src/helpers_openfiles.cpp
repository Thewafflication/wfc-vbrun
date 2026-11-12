// The open-file table of Visual Basic: Open, Close, FreeFile, LOF, Loc, Seek,
// EOF and Line Input on file numbers 1 to 512.
//
// Measured from a Visual Basic 6 program that runs the file statements
// (scripts/vb6_harness_gen.py, experiment fileTable) and from the disassembly
// of the reference (the argument shapes). What the program saw:
//   * Open: the file number must be 1..512 (error 52), a number in use is
//     error 55, a name that does not exist is 53 (76 when the folder is
//     missing). A second Open of one file, or an Input of a file that is open
//     for Output, is error 55 (the files are opened exclusively).
//   * FreeFile gives the lowest number not in use; FreeFile(1) starts at 256.
//   * Seek(n): the next byte (1 based) for a sequential or Binary file, the
//     next record for a Random file. Loc(n): the 128 byte blocks read (an
//     Input file reports at least 1) or written for a sequential file, the
//     last byte for Binary, the last record for Random.
//   * EOF is True for a file opened for Output or Append, and for a file
//     read to its end; a Binary or Random file reports it only after a read
//     ran into the end.
//   * Line Input past the end is error 62; a closed number is error 52.
// A line ends at a carriage return (with a following line feed) or at a line
// feed alone.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kModeInput = 1;
constexpr int kModeOutput = 2;
constexpr int kModeRandom = 4;
constexpr int kModeAppend = 8;
constexpr int kModeBinary = 0x20;

constexpr int kBadFileMode = 54;
constexpr int kBadFileName = 52;
constexpr int kFileAlreadyOpen = 55;
constexpr int kFileNotFound = 53;
constexpr int kInputPastEnd = 62;
constexpr int kBadRecordNumber = 63;
constexpr int kTooManyFiles = 67;
constexpr int kPermissionDenied = 70;
constexpr int kPathNotFound = 76;
constexpr int kInvalidProcedureCall = 5;

constexpr int kFileCount = 512;
constexpr unsigned kMissingScode = 0x80020004U;  // DISP_E_PARAMNOTFOUND

struct FileSlot {
    bool used = false;
    int mode = 0;
    HANDLE handle = INVALID_HANDLE_VALUE;
    int record_length = 128;
    long long position = 0;  // bytes from the start (sequential and Binary)
    long long record = 0;    // the last record accessed (Random)
    long long written = 0;   // bytes written since the Open (Output, Append)
    long long column = 0;    // the print column (0 based)
    bool hit_end = false;    // a read ran into the end
    int width = 0;           // Width # (0: no limit)
    bool denied = false;     // a read met a locked region
};

FileSlot g_files[kFileCount + 1];

// The entry of an open file number; raises error 52 for a bad or closed one.
FileSlot* file_of(const int number) {
    if (number < 1 || number > kFileCount || !g_files[number].used) {
        static_cast<void>(vbrun::raise(kBadFileName));
        return nullptr;
    }
    return &g_files[number];
}

void close_entry(FileSlot& file) {
    if (file.handle != INVALID_HANDLE_VALUE) {
        CloseHandle(file.handle);
    }
    file = FileSlot{};
}

long long size_of(const FileSlot& file) {
    LARGE_INTEGER size;
    size.QuadPart = 0;
    return GetFileSizeEx(file.handle, &size) != 0 ? size.QuadPart : 0;
}

int open_error(const DWORD code) {
    switch (code) {
        case ERROR_FILE_NOT_FOUND:
            return kFileNotFound;
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_BAD_PATHNAME:
            return kPathNotFound;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return kFileAlreadyOpen;
        case ERROR_ACCESS_DENIED:
            return kPermissionDenied;
        default:
            return kPathNotFound;
    }
}

// One byte of a sequential file; false at the end.
bool read_byte(FileSlot& file, unsigned char& value) {
    LARGE_INTEGER where;
    where.QuadPart = file.position;
    DWORD got = 0;
    if (SetFilePointerEx(file.handle, where, nullptr, FILE_BEGIN) == 0 ||
        ReadFile(file.handle, &value, 1, &got, nullptr) == 0 || got != 1) {
        file.hit_end = true;
        if (GetLastError() == ERROR_LOCK_VIOLATION) {
            file.denied = true;
            static_cast<void>(vbrun::raise(kPermissionDenied));
        }
        return false;
    }
    ++file.position;
    return true;
}

// The text of a line (without its end), read from a file opened for Input.
bool read_line(FileSlot& file, std::string& line) {
    unsigned char value = 0;
    if (!read_byte(file, value)) {
        return false;
    }
    while (true) {
        if (value == '\n') {
            return true;
        }
        if (value == '\r') {
            const long long mark = file.position;
            if (read_byte(file, value) && value != '\n') {
                file.position = mark;
            }
            file.hit_end = false;
            return true;
        }
        line.push_back(static_cast<char>(value));
        if (!read_byte(file, value)) {
            file.hit_end = false;
            return true;
        }
    }
}

// --- Print # and Write # ---------------------------------------------------

constexpr int kBadRecordLength = 59;
constexpr int kTypeMismatch = 13;
constexpr int kOverflow = 6;

// Where a print list's values are read from: the stack of a Visual Basic call
// (every value in a four byte slot, eight bytes for a Double, a Currency and
// a Date) or the arguments of a C++ call.
class ValueReader {
public:
    virtual ~ValueReader() = default;
    virtual int word() = 0;
    virtual float single() = 0;
    virtual double real() = 0;
    virtual long long big() = 0;
    virtual const void* pointer() = 0;
};

class StackReader final : public ValueReader {
public:
    explicit StackReader(const unsigned char* const start) : next_(start) {}
    int word() override { return take<int>(); }
    float single() override { return take<float>(); }
    double real() override { return take<double>(); }
    long long big() override { return take<long long>(); }
    const void* pointer() override {
        return reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(take<std::uint32_t>()));
    }

private:
    template <typename T>
    T take() {
        T value;
        std::memcpy(&value, next_, sizeof(T));
        next_ += sizeof(T);
        return value;
    }
    const unsigned char* next_;
};

class ListReader final : public ValueReader {
public:
    explicit ListReader(va_list& list) : list_(list) {}
    int word() override { return va_arg(list_, int); }
    float single() override {
        return static_cast<float>(va_arg(list_, double));
    }
    double real() override { return va_arg(list_, double); }
    long long big() override { return va_arg(list_, long long); }
    const void* pointer() override { return va_arg(list_, const void*); }

private:
    va_list& list_;
};

// A number as text: at most `digits` significant digits, no trailing zeros, an
// exponent form for large and small values.
std::string number_text(const double value, const int digits,
                        const bool bare_fraction, const int largest_plain) {
    if (value == 0.0) {
        return "0";
    }
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.*e", digits - 1, std::fabs(value));
    std::string mantissa;
    int exponent = 0;
    for (const char* at = buffer; *at != '\0'; ++at) {
        if (*at == 'e') {
            exponent = std::atoi(at + 1);
            break;
        }
        if (*at != '.') {
            mantissa.push_back(*at);
        }
    }
    while (mantissa.size() > 1 && mantissa.back() == '0') {
        mantissa.pop_back();
    }
    std::string text = value < 0.0 ? "-" : "";
    // Written in full while it has no more than `digits` integer digits and
    // no more than `digits` decimals; otherwise with an exponent.
    const int decimals = static_cast<int>(mantissa.size()) - 1 - exponent;
    if (exponent >= largest_plain || decimals > digits) {
        text += mantissa[0];
        if (mantissa.size() > 1) {
            text += '.';
            text += mantissa.substr(1);
        }
        char tail[16];
        std::snprintf(tail, sizeof(tail), "E%c%02d", exponent < 0 ? '-' : '+',
                      std::abs(exponent));
        return text + tail;
    }
    if (exponent >= 0) {
        std::string whole = mantissa.substr(
            0, std::min<std::size_t>(mantissa.size(),
                                     static_cast<std::size_t>(exponent) + 1));
        whole.append(static_cast<std::size_t>(exponent) + 1 - whole.size(),
                     '0');
        text += whole;
        if (mantissa.size() > static_cast<std::size_t>(exponent) + 1) {
            text += '.';
            text += mantissa.substr(static_cast<std::size_t>(exponent) + 1);
        }
        return text;
    }
    text += bare_fraction ? "." : "0.";
    text.append(static_cast<std::size_t>(-exponent - 1), '0');
    return text + mantissa;
}

std::string currency_text(const long long scaled, const bool bare_fraction) {
    const bool negative = scaled < 0;
    const unsigned long long magnitude =
        negative ? 0ULL - static_cast<unsigned long long>(scaled)
                 : static_cast<unsigned long long>(scaled);
    std::string text = std::to_string(magnitude / 10000ULL);
    std::string fraction = std::to_string(10000ULL + magnitude % 10000ULL);
    fraction.erase(0, 1);
    while (!fraction.empty() && fraction.back() == '0') {
        fraction.pop_back();
    }
    if (bare_fraction && text == "0" && !fraction.empty()) {
        text.clear();
    }
    if (!fraction.empty()) {
        text += "." + fraction;
    }
    return (negative ? "-" : "") + text;
}

std::string narrow(const wchar_t* const text, const std::size_t length) {
    if (length == 0) {
        return std::string();
    }
    const int size =
        WideCharToMultiByte(CP_ACP, 0, text, static_cast<int>(length), nullptr,
                            0, nullptr, nullptr);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_ACP, 0, text, static_cast<int>(length), bytes.data(),
                        size, nullptr, nullptr);
    return bytes;
}

std::wstring widen(const char* const bytes, const std::size_t length) {
    if (length == 0) {
        return std::wstring();
    }
    const int size = MultiByteToWideChar(CP_ACP, 0, bytes,
                                         static_cast<int>(length), nullptr, 0);
    std::wstring text(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_ACP, 0, bytes, static_cast<int>(length), text.data(),
                        size);
    return text;
}

// A Print # date: the system's short date and long time (the date or the
// time alone when the other part is zero).
std::string date_print_text(const double value) {
    BSTR text = nullptr;
    if (FAILED(VarBstrFromDate(value, LOCALE_USER_DEFAULT, 0, &text)) ||
        text == nullptr) {
        return std::string();
    }
    const std::string result = narrow(text, SysStringLen(text));
    SysFreeString(text);
    return result;
}

// A Write # date: #yyyy-mm-dd# with the time as hh:nn:ss when there is one.
std::string date_write_text(const double value) {
    SYSTEMTIME time{};
    if (VariantTimeToSystemTime(value, &time) == 0) {
        return "#" + date_print_text(value) + "#";
    }
    char buffer[48];
    const bool has_time =
        time.wHour != 0 || time.wMinute != 0 || time.wSecond != 0;
    if (has_time) {
        std::snprintf(buffer, sizeof(buffer), "#%04d-%02d-%02d %02d:%02d:%02d#",
                      time.wYear, time.wMonth, time.wDay, time.wHour,
                      time.wMinute, time.wSecond);
    } else {
        std::snprintf(buffer, sizeof(buffer), "#%04d-%02d-%02d#", time.wYear,
                      time.wMonth, time.wDay);
    }
    return buffer;
}

std::string quoted(const std::string& text) {
    std::string result = "\"";
    for (const char c : text) {
        result += c;
        if (c == '"') {
            result += '"';
        }
    }
    return result + "\"";
}

// What one value looks like in a Print # list (a number has a space before
// a positive value and one after) and in a Write # list.
struct Value {
    int type = 0;  // the Visual Basic type: 2 3 4 5 6 7 8 11 12 17
    long long whole = 0;
    double real = 0.0;
    float single = 0.0F;
    std::wstring text;
    int variant_type = VT_EMPTY;
    int error = 0;
};

std::string numeric_print(const std::string& body) {
    return (body.empty() || body[0] != '-' ? " " : "") + body + " ";
}

std::string value_text(const Value& value, const bool write) {
    switch (value.type) {
        case 2:
        case 3:
        case 17:
        case 11:
            if (value.type == 11) {
                if (write) {
                    return value.whole != 0 ? "#TRUE#" : "#FALSE#";
                }
                return value.whole != 0 ? "True" : "False";
            }
            return write ? std::to_string(value.whole)
                         : numeric_print(std::to_string(value.whole));
        case 4: {
            const std::string body = number_text(value.single, 7, write, 7);
            return write ? body : numeric_print(body);
        }
        case 5: {
            const std::string body = number_text(value.real, 15, write, 15);
            return write ? body : numeric_print(body);
        }
        case 6: {
            const std::string body = currency_text(value.whole, write);
            return write ? body : numeric_print(body);
        }
        case 7:
            return write ? date_write_text(value.real)
                         : date_print_text(value.real) + " ";
        case 8: {
            const std::string text =
                narrow(value.text.data(), value.text.size());
            return write ? quoted(text) : text;
        }
        default:
            return std::string();
    }
}

// A Variant of a print list as a Value (false for a type that cannot be
// printed: error 13).
bool variant_value(const VARIANT* const argument, Value& value,
                   std::string& special, const bool write) {
    VARIANT copy;
    VariantInit(&copy);
    if (FAILED(VariantCopyInd(&copy, const_cast<VARIANT*>(argument)))) {
        return false;
    }
    bool good = true;
    special.clear();
    switch (copy.vt) {
        case VT_EMPTY:
            break;
        case VT_NULL:
            special = write ? "#NULL#" : "Null";
            break;
        case VT_I2:
            value.type = 2;
            value.whole = copy.iVal;
            break;
        case VT_I4:
            value.type = 3;
            value.whole = copy.lVal;
            break;
        case VT_UI1:
            value.type = 17;
            value.whole = copy.bVal;
            break;
        case VT_R4:
            value.type = 4;
            value.single = copy.fltVal;
            break;
        case VT_R8:
            value.type = 5;
            value.real = copy.dblVal;
            break;
        case VT_CY:
            value.type = 6;
            value.whole = copy.cyVal.int64;
            break;
        case VT_DATE:
            value.type = 7;
            value.real = copy.date;
            break;
        case VT_BOOL:
            value.type = 11;
            value.whole = copy.boolVal != 0 ? 1 : 0;
            break;
        case VT_BSTR:
            value.type = 8;
            value.text.assign(copy.bstrVal == nullptr ? L"" : copy.bstrVal,
                              SysStringLen(copy.bstrVal));
            break;
        case VT_ERROR: {
            const int number = vbrun::error_of_hresult(copy.scode);
            special = (write ? "#ERROR " : "Error ") + std::to_string(number) +
                      (write ? "#" : "");
            break;
        }
        default:
            good = false;
            break;
    }
    VariantClear(&copy);
    return good;
}

// Text to a sequential file.
bool put_text(FileSlot& file, const std::string& text) {
    if (text.empty()) {
        return true;
    }
    if (file.mode == kModeAppend) {
        LARGE_INTEGER end;
        end.QuadPart = 0;
        SetFilePointerEx(file.handle, end, nullptr, FILE_END);
    }
    DWORD done = 0;
    if (WriteFile(file.handle, text.data(), static_cast<DWORD>(text.size()),
                  &done, nullptr) == 0) {
        static_cast<void>(vbrun::raise(kPermissionDenied));
        return false;
    }
    file.written += done;
    for (const char c : text) {
        if (c == '\n') {
            file.column = 0;
        } else if (c != '\r') {
            ++file.column;
        }
    }
    return true;
}

void pad_to(FileSlot& file, const long long column) {
    if (column > file.column) {
        put_text(
            file,
            std::string(static_cast<std::size_t>(column - file.column), ' '));
    }
}

// Reads one item of a print list from `reader`.
bool read_value(const int type, ValueReader& reader, Value& value,
                std::string& special, const bool write) {
    value = Value{};
    special.clear();
    value.type = type;
    switch (type) {
        case 2:
            value.whole = static_cast<short>(reader.word());
            return true;
        case 3:
        case 17:
            value.whole = reader.word();
            if (type == 17) {
                value.whole &= 0xFF;
            }
            return true;
        case 11:
            value.whole = static_cast<short>(reader.word()) != 0 ? 1 : 0;
            return true;
        case 4:
            value.single = reader.single();
            return true;
        case 5:
        case 7:
            value.real = reader.real();
            return true;
        case 6:
            value.whole = reader.big();
            return true;
        case 8: {
            const auto* const text =
                static_cast<const wchar_t*>(reader.pointer());
            value.text.assign(
                text == nullptr ? L"" : text,
                text == nullptr ? 0U : SysStringLen(const_cast<BSTR>(text)));
            return true;
        }
        case 12:
            return variant_value(static_cast<const VARIANT*>(reader.pointer()),
                                 value, special, write);
        default:
            return true;
    }
}

void print_list(const int number, const unsigned char* const descriptor,
                ValueReader& reader, const bool write) {
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return;
    }
    if (file->mode != kModeOutput && file->mode != kModeAppend) {
        static_cast<void>(vbrun::raise(kBadFileMode));
        return;
    }
    const int count = descriptor[0] | (descriptor[1] << 8);
    for (int index = 0; index < count; ++index) {
        const int code = descriptor[2 + index];
        const int type = code & 0x3F;
        const int separator = code >> 6;
        Value value;
        std::string special;
        if (type == 0x20 || type == 0x21) {
            const int amount = reader.word();
            if (!write) {
                const int shown = file->width > 0 && amount > file->width
                                      ? amount % file->width
                                      : amount;
                if (type == 0x20) {
                    put_text(*file, std::string(static_cast<std::size_t>(
                                                    shown > 0 ? shown : 0),
                                                ' '));
                } else {
                    const long long target = (shown < 1 ? 1 : shown) - 1;
                    if (target < file->column) {
                        put_text(*file, "\r\n");
                    }
                    pad_to(*file, target);
                }
            }
        } else if (type != 0) {
            if (!read_value(type, reader, value, special, write)) {
                static_cast<void>(vbrun::raise(kTypeMismatch));
                return;
            }
            const std::string text =
                special.empty() ? value_text(value, write) : special;
            if (write && index > 0) {
                put_text(*file, ",");
            }
            if (!write && file->width > 0 && file->column > 0 &&
                !text.empty() &&
                file->column + static_cast<long long>(text.size()) >
                    file->width) {
                put_text(*file, "\r\n");
            }
            put_text(*file, text);
        } else if (write && index > 0 && separator != 2) {
            put_text(*file, ",");
        }
        if (separator == 2) {
            put_text(*file, "\r\n");
        } else if (separator == 0 && !write) {
            const long long zone = (file->column / 14 + 1) * 14;
            if (file->width > 0 && zone >= file->width) {
                put_text(*file, "\r\n");
            } else {
                pad_to(*file, zone);
            }
        }
    }
}

// --- Get # and Put # -------------------------------------------------------

// Positions the file for a Get or Put: `record` is the record (Random) or
// byte (Binary) number, 0 for the next one.
bool data_position(FileSlot& file, const long long record) {
    if (file.mode != kModeBinary && file.mode != kModeRandom) {
        static_cast<void>(vbrun::raise(kBadFileMode));
        return false;
    }
    if (record != 0 && record < 1) {
        static_cast<void>(vbrun::raise(kBadRecordNumber));
        return false;
    }
    file.hit_end = false;
    if (file.mode == kModeRandom) {
        file.record = record != 0 ? record - 1 : file.record;
        file.position = file.record * file.record_length;
    } else if (record != 0) {
        file.position = record - 1;
    }
    return true;
}

bool put_bytes(FileSlot& file, const std::string& bytes) {
    LARGE_INTEGER where;
    where.QuadPart = file.position;
    DWORD done = 0;
    if (SetFilePointerEx(file.handle, where, nullptr, FILE_BEGIN) == 0 ||
        WriteFile(file.handle, bytes.data(), static_cast<DWORD>(bytes.size()),
                  &done, nullptr) == 0) {
        static_cast<void>(vbrun::raise(kPermissionDenied));
        return false;
    }
    file.position += done;
    return true;
}

// Reads `size` bytes (zero filled when the file is shorter).
std::string get_bytes(FileSlot& file, const std::size_t size) {
    std::string bytes(size, '\0');
    LARGE_INTEGER where;
    where.QuadPart = file.position;
    DWORD got = 0;
    if (size > 0 &&
        (SetFilePointerEx(file.handle, where, nullptr, FILE_BEGIN) == 0 ||
         ReadFile(file.handle, bytes.data(), static_cast<DWORD>(size), &got,
                  nullptr) == 0)) {
        got = 0;
        if (GetLastError() == ERROR_LOCK_VIOLATION) {
            static_cast<void>(vbrun::raise(kPermissionDenied));
        }
    }
    if (got < size) {
        file.hit_end = true;
    }
    file.position += got;
    return bytes;
}

void put_string(FileSlot& file, std::string& out, const BSTR text) {
    const std::string bytes = narrow(text == nullptr ? L"" : text,
                                     text == nullptr ? 0U : SysStringLen(text));
    if (file.mode == kModeRandom) {
        out += static_cast<char>(bytes.size() & 0xFF);
        out += static_cast<char>((bytes.size() >> 8) & 0xFF);
    }
    out += bytes;
}

// The bytes Put writes for a Variant: its type, then its value.
bool variant_bytes(FileSlot& file, const VARIANT* const argument,
                   std::string& out) {
    VARIANT copy;
    VariantInit(&copy);
    if (FAILED(VariantCopyInd(&copy, const_cast<VARIANT*>(argument)))) {
        return false;
    }
    out += static_cast<char>(copy.vt & 0xFF);
    out += static_cast<char>((copy.vt >> 8) & 0xFF);
    bool good = true;
    auto raw = [&out](const void* const data, const std::size_t size) {
        out.append(static_cast<const char*>(data), size);
    };
    switch (copy.vt) {
        case VT_EMPTY:
        case VT_NULL:
            break;
        case VT_I2:
        case VT_BOOL:
            raw(&copy.iVal, 2);
            break;
        case VT_I4:
        case VT_R4:
        case VT_ERROR:
            raw(&copy.lVal, 4);
            break;
        case VT_UI1:
            raw(&copy.bVal, 1);
            break;
        case VT_R8:
        case VT_CY:
        case VT_DATE:
            raw(&copy.dblVal, 8);
            break;
        case VT_BSTR: {
            const std::string bytes =
                narrow(copy.bstrVal == nullptr ? L"" : copy.bstrVal,
                       SysStringLen(copy.bstrVal));
            out += static_cast<char>(bytes.size() & 0xFF);
            out += static_cast<char>((bytes.size() >> 8) & 0xFF);
            out += bytes;
            break;
        }
        default:
            good = false;
            break;
    }
    static_cast<void>(file);
    VariantClear(&copy);
    return good;
}

void put_value(const int code, const void* const value, const long long record,
               const int number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr || !data_position(*file, record)) {
        return;
    }
    std::string bytes;
    if (code > 0) {
        bytes.assign(static_cast<const char*>(value),
                     static_cast<std::size_t>(code));
    } else if (code == 0) {
        put_string(*file, bytes, *static_cast<const BSTR*>(value));
    } else if (!variant_bytes(*file, static_cast<const VARIANT*>(value),
                              bytes)) {
        static_cast<void>(vbrun::raise(kTypeMismatch));
        return;
    }
    if (file->mode == kModeRandom &&
        static_cast<long long>(bytes.size()) > file->record_length) {
        static_cast<void>(vbrun::raise(kBadRecordLength));
        return;
    }
    if (put_bytes(*file, bytes) && file->mode == kModeRandom) {
        ++file->record;
    }
}

void get_value(const int code, void* const value, const long long record,
               const int number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr || !data_position(*file, record)) {
        return;
    }
    if (code > 0) {
        const std::string bytes =
            get_bytes(*file, static_cast<std::size_t>(code));
        std::memcpy(value, bytes.data(), bytes.size());
    } else if (code == 0) {
        BSTR* const text = static_cast<BSTR*>(value);
        std::size_t length =
            text == nullptr || *text == nullptr ? 0U : SysStringLen(*text);
        if (file->mode == kModeRandom) {
            const std::string prefix = get_bytes(*file, 2);
            length =
                static_cast<unsigned char>(prefix[0]) |
                (static_cast<std::size_t>(static_cast<unsigned char>(prefix[1]))
                 << 8);
        }
        const std::string bytes = get_bytes(*file, length);
        const std::wstring wide = widen(bytes.data(), bytes.size());
        SysFreeString(*text);
        *text = SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
    } else {
        VARIANT* const target = static_cast<VARIANT*>(value);
        const std::string head = get_bytes(*file, 2);
        VARTYPE type =
            static_cast<VARTYPE>(static_cast<unsigned char>(head[0]) |
                                 (static_cast<unsigned char>(head[1]) << 8));
        VariantClear(target);
        target->vt = type;
        std::size_t size = 0;
        switch (type) {
            case VT_I2:
            case VT_BOOL:
                size = 2;
                break;
            case VT_I4:
            case VT_R4:
            case VT_ERROR:
                size = 4;
                break;
            case VT_UI1:
                size = 1;
                break;
            case VT_R8:
            case VT_CY:
            case VT_DATE:
                size = 8;
                break;
            default:
                break;
        }
        if (type == VT_BSTR) {
            const std::string prefix = get_bytes(*file, 2);
            const std::size_t length =
                static_cast<unsigned char>(prefix[0]) |
                (static_cast<std::size_t>(static_cast<unsigned char>(prefix[1]))
                 << 8);
            const std::string bytes = get_bytes(*file, length);
            const std::wstring wide = widen(bytes.data(), bytes.size());
            target->bstrVal =
                SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
        } else if (size > 0) {
            const std::string bytes = get_bytes(*file, size);
            std::memcpy(&target->lVal, bytes.data(), bytes.size());
        } else if (type != VT_EMPTY && type != VT_NULL) {
            target->vt = VT_EMPTY;
            static_cast<void>(vbrun::raise(kTypeMismatch));
        }
    }
    if (file->mode == kModeRandom) {
        ++file->record;
    }
}

// --- Input # ---------------------------------------------------------------

struct Field {
    std::string text;
    bool quoted = false;
    bool hashed = false;
};

bool peek_byte(FileSlot& file, unsigned char& value) {
    const long long mark = file.position;
    const bool got = read_byte(file, value);
    file.position = mark;
    return got;
}

// Reads one field; false at the end of the file before any text.
bool read_field(FileSlot& file, Field& field, const bool stop_at_space) {
    unsigned char value = 0;
    while (peek_byte(file, value) && (value == ' ' || value == '\t')) {
        read_byte(file, value);
    }
    if (!peek_byte(file, value)) {
        file.hit_end = true;
        return false;
    }
    if (value == '"') {
        read_byte(file, value);
        field.quoted = true;
        while (read_byte(file, value)) {
            if (value == '"') {
                unsigned char next = 0;
                if (peek_byte(file, next) && next == '"') {
                    read_byte(file, next);
                    field.text.push_back('"');
                    continue;
                }
                break;
            }
            field.text.push_back(static_cast<char>(value));
        }
    } else {
        while (peek_byte(file, value) && value != ',' && value != '\r' &&
               value != '\n' && !(stop_at_space && value == ' ')) {
            read_byte(file, value);
            field.text.push_back(static_cast<char>(value));
        }
        while (!field.text.empty() && field.text.back() == ' ') {
            field.text.pop_back();
        }
        if (field.text.size() >= 2 && field.text.front() == '#' &&
            field.text.back() == '#') {
            field.hashed = true;
            field.text = field.text.substr(1, field.text.size() - 2);
        }
    }
    while (peek_byte(file, value) && (value == ' ' || value == '\t')) {
        read_byte(file, value);
    }
    if (peek_byte(file, value)) {
        if (value == ',') {
            read_byte(file, value);
        } else if (value == '\r') {
            read_byte(file, value);
            if (peek_byte(file, value) && value == '\n') {
                read_byte(file, value);
            }
        } else if (value == '\n') {
            read_byte(file, value);
        }
    }
    file.hit_end = false;
    return true;
}

bool is_number_text(const std::wstring& text, double& value) {
    if (text.empty()) {
        return false;
    }
    return SUCCEEDED(
        VarR8FromStr(text.c_str(), LOCALE_USER_DEFAULT, 0, &value));
}

// The Variant an Input # field makes.
void field_variant(const Field& field, VARIANT* const target) {
    VariantClear(target);
    const std::wstring wide = widen(field.text.data(), field.text.size());
    if (field.quoted) {
        target->vt = VT_BSTR;
        target->bstrVal =
            SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
        return;
    }
    if (field.hashed) {
        if (field.text == "TRUE" || field.text == "FALSE") {
            target->vt = VT_BOOL;
            target->boolVal =
                field.text == "TRUE" ? VARIANT_TRUE : VARIANT_FALSE;
        } else if (field.text == "NULL") {
            target->vt = VT_NULL;
        } else if (field.text.rfind("ERROR ", 0) == 0) {
            target->vt = VT_ERROR;
            target->scode = static_cast<SCODE>(
                0x800A0000U |
                static_cast<unsigned>(std::atoi(field.text.c_str() + 6)));
        } else {
            double date = 0.0;
            if (SUCCEEDED(VarDateFromStr(wide.c_str(), LOCALE_USER_DEFAULT, 0,
                                         &date))) {
                target->vt = VT_DATE;
                target->date = date;
            } else {
                target->vt = VT_BSTR;
                target->bstrVal = SysAllocStringLen(
                    wide.data(), static_cast<UINT>(wide.size()));
            }
        }
        return;
    }
    double number = 0.0;
    if (field.text.empty()) {
        return;
    }
    if (is_number_text(wide, number)) {
        if (number == std::floor(number) && number >= -32768.0 &&
            number <= 32767.0 &&
            field.text.find_first_of(".eE") == std::string::npos) {
            target->vt = VT_I2;
            target->iVal = static_cast<SHORT>(number);
        } else if (number == std::floor(number) && number >= -2147483648.0 &&
                   number <= 2147483647.0 &&
                   field.text.find_first_of(".eE") == std::string::npos) {
            target->vt = VT_I4;
            target->lVal = static_cast<LONG>(number);
        } else {
            target->vt = VT_R8;
            target->dblVal = number;
        }
        return;
    }
    target->vt = VT_BSTR;
    target->bstrVal =
        SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
}

// A field into a typed target (through the Variant it makes).
void field_into(const int type, const Field& field, void* const target) {
    VARIANT source;
    VariantInit(&source);
    const std::wstring wide = widen(field.text.data(), field.text.size());
    if (field.hashed) {
        field_variant(field, &source);
    } else {
        source.vt = VT_BSTR;
        source.bstrVal =
            SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
    }
    VARIANT result;
    VariantInit(&result);
    HRESULT status = S_OK;
    if (type == 8) {
        if (source.vt == VT_BSTR) {
            BSTR* const text = static_cast<BSTR*>(target);
            SysFreeString(*text);
            *text = source.bstrVal;
            source.bstrVal = nullptr;
        } else {
            status = VariantChangeTypeEx(&result, &source, LOCALE_USER_DEFAULT,
                                         VARIANT_ALPHABOOL, VT_BSTR);
            if (SUCCEEDED(status)) {
                BSTR* const text = static_cast<BSTR*>(target);
                SysFreeString(*text);
                *text = result.bstrVal;
                result.bstrVal = nullptr;
            }
        }
    } else {
        VARTYPE wanted = VT_EMPTY;
        switch (type) {
            case 2:
                wanted = VT_I2;
                break;
            case 3:
                wanted = VT_I4;
                break;
            case 4:
                wanted = VT_R4;
                break;
            case 5:
                wanted = VT_R8;
                break;
            case 6:
                wanted = VT_CY;
                break;
            case 7:
                wanted = VT_DATE;
                break;
            case 11:
                wanted = VT_BOOL;
                break;
            case 17:
                wanted = VT_UI1;
                break;
            default:
                break;
        }
        if (type == 11 && !field.hashed) {
            // Only #TRUE# is True; other text counts by its number.
            double flag = 0.0;
            *static_cast<SHORT*>(target) =
                is_number_text(wide, flag) && flag != 0.0 ? -1 : 0;
        } else if (field.text.empty() && !field.hashed) {
            std::memset(target, 0,
                        type == 5 || type == 6 || type == 7 ? 8
                        : type == 2 || type == 11           ? 2
                        : type == 17                        ? 1
                                                            : 4);
        } else {
            status = VariantChangeTypeEx(&result, &source, LOCALE_USER_DEFAULT,
                                         0, wanted);
            if (SUCCEEDED(status)) {
                std::memcpy(target, &result.lVal,
                            type == 5 || type == 6 || type == 7 ? 8
                            : type == 2 || type == 11           ? 2
                            : type == 17                        ? 1
                                                                : 4);
            } else if (status == DISP_E_OVERFLOW) {
                static_cast<void>(vbrun::raise(kOverflow));
            }
        }
    }
    VariantClear(&source);
    VariantClear(&result);
}

void input_list(const int number, const unsigned char* const descriptor,
                ValueReader& reader) {
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return;
    }
    if (file->mode != kModeInput && file->mode != kModeBinary) {
        static_cast<void>(vbrun::raise(kBadFileMode));
        return;
    }
    const int count = descriptor[0] | (descriptor[1] << 8);
    for (int index = 0; index < count; ++index) {
        const int type = descriptor[2 + index] & 0x3F;
        void* const target = const_cast<void*>(reader.pointer());
        Field field;
        if (!read_field(*file, field, type != 8 && type != 12)) {
            static_cast<void>(vbrun::raise(kInputPastEnd));
            return;
        }
        if (type == 12) {
            field_variant(field, static_cast<VARIANT*>(target));
        } else {
            field_into(type, field, target);
        }
        if (vbrun::last_error() != 0) {
            // A value that does not fit: the rest of the line is not read.
            return;
        }
    }
}

}  // namespace

extern "C" {

// Open name For mode [Access] [Lock] As #number [Len = length]. The mode
// flags: the mode in the low byte, the access in 0xF00, the lock in 0xF000.
void __cdecl __vbaFileOpen(const DWORD flags, const int record_length,
                           const int number, const BSTR name) {
    const int mode = static_cast<int>(flags & 0xFFU);
    const DWORD access_bits = flags & 0xF00U;
    const DWORD lock_bits = flags & 0xF000U;
    if (number < 1 || number > kFileCount) {
        static_cast<void>(vbrun::raise(kBadFileName));
        return;
    }
    if (g_files[number].used) {
        static_cast<void>(vbrun::raise(kFileAlreadyOpen));
        return;
    }
    DWORD access = GENERIC_READ;
    DWORD disposition = OPEN_EXISTING;
    if (mode == kModeOutput) {
        access = GENERIC_WRITE;
        disposition = CREATE_ALWAYS;
    } else if (mode == kModeAppend) {
        access = GENERIC_WRITE;
        disposition = OPEN_ALWAYS;
    } else if (mode == kModeRandom || mode == kModeBinary) {
        access = GENERIC_READ | GENERIC_WRITE;
        disposition = OPEN_ALWAYS;
        if (access_bits == 0x100U) {
            access = GENERIC_READ;
        } else if (access_bits == 0x200U) {
            access = GENERIC_WRITE;
        }
    }
    // The lock keyword: 0x1000 Lock Read Write, 0x2000 Lock Write, 0x3000
    // Lock Read, anything else (Shared) denies nothing. No keyword opens the
    // file exclusively.
    DWORD share = 0U;
    if (lock_bits == 0x2000U) {
        share = FILE_SHARE_READ;
    } else if (lock_bits == 0x3000U) {
        share = FILE_SHARE_WRITE;
    } else if (lock_bits != 0U && lock_bits != 0x1000U) {
        share = FILE_SHARE_READ | FILE_SHARE_WRITE;
    }
    const wchar_t* const path = name == nullptr ? L"" : name;
    HANDLE handle = CreateFileW(path, access, share, nullptr, disposition,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        static_cast<void>(vbrun::raise(open_error(GetLastError())));
        return;
    }
    FileSlot& file = g_files[number];
    file.used = true;
    file.mode = mode;
    file.handle = handle;
    file.record_length =
        mode == kModeRandom ? (record_length == -1 ? 128 : record_length) : 1;
    if (mode == kModeRandom && file.record_length < 1) {
        close_entry(file);
        static_cast<void>(vbrun::raise(kInvalidProcedureCall));
    }
}

// Close #number (0 closes nothing).
void WINAPI __vbaFileClose(const SHORT number) {
    if (number == 0) {
        return;
    }
    // Closing a number that is not open is not an error.
    if (number >= 1 && number <= kFileCount && g_files[number].used) {
        close_entry(g_files[number]);
    }
}

// Close with no file numbers.
void __cdecl __vbaFileCloseAll() {
    for (FileSlot& file : g_files) {
        if (file.used) {
            close_entry(file);
        }
    }
}

// FreeFile([range]): the lowest unused number of 1..255 (range 0) or
// 256..511 (range 1).
SHORT WINAPI rtcFreeFile(const VARIANT* const range) {
    int first = 1;
    int last = 255;
    if (range != nullptr &&
        !(range->vt == VT_ERROR &&
          static_cast<unsigned>(range->scode) == kMissingScode)) {
        VARIANT copy;
        VariantInit(&copy);
        const HRESULT status =
            VariantChangeType(&copy, const_cast<VARIANT*>(range), 0, VT_I2);
        if (FAILED(status)) {
            static_cast<void>(vbrun::raise_hresult(status));
            return 0;
        }
        const int value = copy.iVal;
        if (value == 1) {
            first = 256;
            last = 511;
        } else if (value != 0) {
            static_cast<void>(vbrun::raise(kInvalidProcedureCall));
            return 0;
        }
    }
    for (int number = first; number <= last; ++number) {
        if (!g_files[number].used) {
            return static_cast<SHORT>(number);
        }
    }
    static_cast<void>(vbrun::raise(kTooManyFiles));
    return 0;
}

// LOF(number): the length in bytes.
LONG WINAPI rtcFileLength(const SHORT number) {
    const FileSlot* const file = file_of(number);
    return file == nullptr ? 0 : static_cast<LONG>(size_of(*file));
}

// Seek(number): the next byte (the next record of a Random file).
LONG WINAPI rtcFileSeek(const SHORT number) {
    const FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return 0;
    }
    if (file->mode == kModeRandom) {
        return static_cast<LONG>(file->record + 1);
    }
    if (file->mode == kModeOutput || file->mode == kModeAppend) {
        return static_cast<LONG>(file->written + 1);
    }
    return static_cast<LONG>(file->position + 1);
}

// Loc(number).
LONG WINAPI rtcFileLocation(const SHORT number) {
    const FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return 0;
    }
    switch (file->mode) {
        case kModeRandom:
            return static_cast<LONG>(file->record);
        case kModeBinary:
            return static_cast<LONG>(file->position);
        case kModeInput: {
            const long long read = file->position;
            return static_cast<LONG>(read == 0 ? 1 : (read + 127) / 128);
        }
        default:
            return static_cast<LONG>(file->written / 128);
    }
}

// Seek #number, position.
void WINAPI __vbaFileSeek(const LONG position, const SHORT number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return;
    }
    if (position < 1) {
        static_cast<void>(vbrun::raise(kBadRecordNumber));
        return;
    }
    file->hit_end = false;
    if (file->mode == kModeRandom) {
        file->record = static_cast<long long>(position) - 1;
    } else if (file->mode == kModeOutput || file->mode == kModeAppend) {
        file->written = static_cast<long long>(position) - 1;
    } else {
        file->position = static_cast<long long>(position) - 1;
    }
}

// EOF(number).
SHORT WINAPI rtcEndOfFile(const SHORT number) {
    const FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return 0;
    }
    switch (file->mode) {
        case kModeOutput:
        case kModeAppend:
            return -1;
        case kModeBinary:
        case kModeRandom:
            return file->hit_end ? -1 : 0;
        default:
            return file->position >= size_of(*file) ? -1 : 0;
    }
}

// Line Input #number, text: `text` receives the line.
void WINAPI __vbaLineInputStr(BSTR* const text, const SHORT number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return;
    }
    if (file->mode != kModeInput) {
        static_cast<void>(vbrun::raise(kBadFileMode));
        return;
    }
    std::string line;
    if (!read_line(*file, line)) {
        static_cast<void>(vbrun::raise(kInputPastEnd));
        return;
    }
    const int wide = MultiByteToWideChar(
        CP_ACP, 0, line.data(), static_cast<int>(line.size()), nullptr, 0);
    std::wstring converted(static_cast<std::size_t>(wide), L'\0');
    if (wide > 0) {
        MultiByteToWideChar(CP_ACP, 0, line.data(),
                            static_cast<int>(line.size()), converted.data(),
                            wide);
    }
    SysFreeString(*text);
    *text = SysAllocStringLen(converted.data(),
                              static_cast<UINT>(converted.size()));
}

// Line Input #number, variant.
void WINAPI __vbaLineInputVar(VARIANT* const target, const SHORT number) {
    BSTR text = nullptr;
    vbrun::clear_error();
    __vbaLineInputStr(&text, number);
    if (vbrun::last_error() != 0) {
        SysFreeString(text);
        return;
    }
    VariantClear(target);
    target->vt = VT_BSTR;
    target->bstrVal = text;
}

// Print #number, list. `descriptor` is a count and, for every item, a byte:
// the type in the low six bits (the Visual Basic type, 0x20 Spc, 0x21 Tab, 0
// for no value) and in the top two bits what follows it (0 a comma, 1 a
// semicolon, 2 the end of the line); the values come after the file number.
void __cdecl __vbaPrintFile(const unsigned char* const descriptor,
                            const int number, ...) {
#if defined(_M_IX86)
    StackReader reader(reinterpret_cast<const unsigned char*>(&number) +
                       sizeof(number));
    print_list(static_cast<SHORT>(number), descriptor, reader, false);
#else
    va_list list;
    va_start(list, number);
    ListReader reader(list);
    print_list(static_cast<SHORT>(number), descriptor, reader, false);
    va_end(list);
#endif
}

// Write #number, list.
void __cdecl __vbaWriteFile(const unsigned char* const descriptor,
                            const int number, ...) {
#if defined(_M_IX86)
    StackReader reader(reinterpret_cast<const unsigned char*>(&number) +
                       sizeof(number));
    print_list(static_cast<SHORT>(number), descriptor, reader, true);
#else
    va_list list;
    va_start(list, number);
    ListReader reader(list);
    print_list(static_cast<SHORT>(number), descriptor, reader, true);
    va_end(list);
#endif
}

// Input #number, targets: every target is a pointer to a variable of the
// item's type.
void __cdecl __vbaInputFile(const unsigned char* const descriptor,
                            const int number, ...) {
    vbrun::clear_error();
#if defined(_M_IX86)
    StackReader reader(reinterpret_cast<const unsigned char*>(&number) +
                       sizeof(number));
    input_list(static_cast<SHORT>(number), descriptor, reader);
#else
    va_list list;
    va_start(list, number);
    ListReader reader(list);
    input_list(static_cast<SHORT>(number), descriptor, reader);
    va_end(list);
#endif
}

// Put #number, [record], value. `code` is the size of a number, 0 for a
// String (the address of the String variable), -1 for a Variant.
void WINAPI __vbaPut3(const LONG code, const void* const value,
                      const SHORT number) {
    put_value(code, value, 0, number);
}

void WINAPI __vbaPut4(const LONG code, const void* const value,
                      const LONG record, const SHORT number) {
    put_value(code, value, record, number);
}

void WINAPI __vbaGet3(const LONG code, void* const value, const SHORT number) {
    get_value(code, value, 0, number);
}

void WINAPI __vbaGet4(const LONG code, void* const value, const LONG record,
                      const SHORT number) {
    get_value(code, value, record, number);
}

// A fixed length String: `length` characters at `text`.
void WINAPI __vbaPutFxStr4(const LONG length, const wchar_t* const text,
                           const LONG record, const SHORT number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr || !data_position(*file, record)) {
        return;
    }
    const std::string bytes = narrow(text, static_cast<std::size_t>(length));
    if (file->mode == kModeRandom &&
        static_cast<long long>(bytes.size()) > file->record_length) {
        static_cast<void>(vbrun::raise(kBadRecordLength));
        return;
    }
    if (put_bytes(*file, bytes) && file->mode == kModeRandom) {
        ++file->record;
    }
}

void WINAPI __vbaPutFxStr3(const LONG length, const wchar_t* const text,
                           const SHORT number) {
    __vbaPutFxStr4(length, text, 0, number);
}

void WINAPI __vbaGetFxStr4(const LONG length, wchar_t* const text,
                           const LONG record, const SHORT number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr || !data_position(*file, record)) {
        return;
    }
    if (file->mode == kModeRandom && length > file->record_length) {
        static_cast<void>(vbrun::raise(kBadRecordLength));
        return;
    }
    const std::string bytes =
        get_bytes(*file, static_cast<std::size_t>(length));
    const std::wstring wide = widen(bytes.data(), bytes.size());
    for (std::size_t i = 0; i < static_cast<std::size_t>(length); ++i) {
        text[i] = i < wide.size() ? wide[i] : L'\0';
    }
    if (file->mode == kModeRandom) {
        ++file->record;
    }
}

void WINAPI __vbaGetFxStr3(const LONG length, wchar_t* const text,
                           const SHORT number) {
    __vbaGetFxStr4(length, text, 0, number);
}

// Reset: closes every open file.
void __cdecl rtcFileReset() {
    __vbaFileCloseAll();
}

// Width #number, width: the line width of Print # (0 for no limit).
void WINAPI rtcFileWidth(const SHORT number, const SHORT width) {
    if (number < 1 || number > kFileCount) {
        static_cast<void>(vbrun::raise(kBadFileName));
        return;
    }
    if (width < 0 || width > 255) {
        static_cast<void>(vbrun::raise(kInvalidProcedureCall));
        return;
    }
    FileSlot* const file = file_of(number);
    if (file != nullptr) {
        file->width = width;
    }
}

// FileAttr(number, kind): the mode of the file (kind 1). The handle (kind 2)
// is refused, as it is by the reference.
LONG WINAPI rtcFileAttributes(const SHORT number, const SHORT kind) {
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return 0;
    }
    if (kind != 1) {
        static_cast<void>(vbrun::raise(kInvalidProcedureCall));
        return 0;
    }
    return file->mode;
}

}  // extern "C"

namespace {

// The characters (or bytes) of Input$ and InputB$. A sequential file raises
// 62 when it ends early; a Binary file returns what it found.
BSTR input_text(const LONG count, const SHORT number, const bool bytes) {
    if (count < 0 || count > 0x3FFFFFFF) {
        static_cast<void>(vbrun::raise(kInvalidProcedureCall));
        return nullptr;
    }
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return nullptr;
    }
    if (file->mode != kModeInput && file->mode != kModeBinary) {
        static_cast<void>(vbrun::raise(kBadFileMode));
        return nullptr;
    }
    std::string raw;
    std::wstring chars;
    bool ended = false;
    for (LONG index = 0; index < count && !ended; ++index) {
        unsigned char value = 0;
        if (!read_byte(*file, value)) {
            ended = true;
            break;
        }
        raw.push_back(static_cast<char>(value));
        if (!bytes) {
            std::string unit(1, static_cast<char>(value));
            if (IsDBCSLeadByte(value) != 0) {
                unsigned char trail = 0;
                if (read_byte(*file, trail)) {
                    unit.push_back(static_cast<char>(trail));
                }
            }
            chars += widen(unit.data(), unit.size());
        }
    }
    if (file->denied) {
        file->denied = false;
        return nullptr;
    }
    if (ended && file->mode != kModeBinary) {
        static_cast<void>(vbrun::raise(kInputPastEnd));
        return nullptr;
    }
    if (bytes) {
        return SysAllocStringByteLen(raw.data(), static_cast<UINT>(raw.size()));
    }
    return SysAllocStringLen(chars.data(), static_cast<UINT>(chars.size()));
}

void input_variant(VARIANT* const result, BSTR text) {
    VariantInit(result);
    if (text != nullptr) {
        V_VT(result) = VT_BSTR;
        V_BSTR(result) = text;
    }
}

}  // namespace

extern "C" {

BSTR WINAPI rtcInputCount(const LONG count, const SHORT number) {
    return input_text(count, number, true);
}

BSTR WINAPI rtcInputCharCount(const LONG count, const SHORT number) {
    return input_text(count, number, false);
}

VARIANT* WINAPI rtcInputCountVar(VARIANT* const result, const LONG count,
                                 const SHORT number) {
    input_variant(result, input_text(count, number, true));
    return result;
}

VARIANT* WINAPI rtcInputCharCountVar(VARIANT* const result, const LONG count,
                                     const SHORT number) {
    input_variant(result, input_text(count, number, false));
    return result;
}

// Name old As new: a rename.
void WINAPI __vbaNameFile(const BSTR old_name, const BSTR new_name) {
    const wchar_t* const from = old_name == nullptr ? L"" : old_name;
    const wchar_t* const to = new_name == nullptr ? L"" : new_name;
    if (to[0] == 0) {
        // An empty new name is the current folder, which exists.
        static_cast<void>(vbrun::raise(58));
        return;
    }
    if (MoveFileW(from, to) != 0) {
        return;
    }
    switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            static_cast<void>(vbrun::raise(kFileNotFound));
            break;
        case ERROR_SHARING_VIOLATION:
            static_cast<void>(vbrun::raise(kFileAlreadyOpen));
            break;
        case ERROR_ACCESS_DENIED:
            static_cast<void>(vbrun::raise(75));
            break;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            static_cast<void>(vbrun::raise(58));
            break;
        case ERROR_NOT_SAME_DEVICE:
            static_cast<void>(vbrun::raise(74));
            break;
        default:
            static_cast<void>(vbrun::raise(kInvalidProcedureCall));
            break;
    }
}

// Lock and Unlock. `flags`: bit 0 set for Unlock, bit 1 for a From To range,
// bit 2 for one record (in `from`); neither means the whole file. Only a
// Random or Binary file honours a record range.
void WINAPI __vbaFileLock(const DWORD flags, const LONG to, const LONG from,
                          const SHORT number) {
    FileSlot* const file = file_of(number);
    if (file == nullptr) {
        return;
    }
    unsigned long long start = 0;
    unsigned long long length = 0xFFFFFFFFULL;
    if ((file->mode == kModeRandom || file->mode == kModeBinary) &&
        (flags & 6U) != 0) {
        const LONG last = (flags & 4U) != 0 ? from : to;
        if (from < 1 || last < 1 || from > last) {
            static_cast<void>(vbrun::raise(kBadRecordNumber));
            return;
        }
        const unsigned long long size =
            static_cast<unsigned long long>(file->record_length);
        start = static_cast<unsigned long long>(from - 1) * size;
        length = static_cast<unsigned long long>(last - from + 1) * size;
    }
    const DWORD low = static_cast<DWORD>(start & 0xFFFFFFFFULL);
    const DWORD high = static_cast<DWORD>(start >> 32);
    const DWORD length_low = static_cast<DWORD>(length & 0xFFFFFFFFULL);
    const DWORD length_high = static_cast<DWORD>(length >> 32);
    const BOOL done =
        (flags & 1U) != 0
            ? UnlockFile(file->handle, low, high, length_low, length_high)
            : LockFile(file->handle, low, high, length_low, length_high);
    if (done == 0) {
        static_cast<void>(vbrun::raise(kPermissionDenied));
    }
}

}  // extern "C"
