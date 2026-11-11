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
    bool hit_end = false;    // a read ran into the end
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
    const DWORD share =
        lock_bits == 0U ? 0U : (FILE_SHARE_READ | FILE_SHARE_WRITE);
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

}  // extern "C"
