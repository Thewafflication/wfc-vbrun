// The file-system helpers: FileLen, GetAttr, FileDateTime, SetAttr, MkDir,
// RmDir, ChDir, Kill, FileCopy, Dir and CurDir.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/files.cases, which starts every group from the same scratch
// folder). The errors are the ones that program saw:
//   53 file not found, 76 path not found, 52 bad file name or number,
//   75 path/file access error, 70 permission denied, 5 invalid procedure call,
//   13 type mismatch, 94 invalid use of Null.

#include <string>
#include <vector>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidProcedureCall = 5;
constexpr int kTypeMismatch = 13;
constexpr int kBadFileName = 52;
constexpr int kFileNotFound = 53;
constexpr int kAccessDenied = 70;
constexpr int kPathFileAccess = 75;
constexpr int kPathNotFound = 76;
constexpr int kInvalidUseOfNull = 94;
constexpr unsigned kMissingScode = 0x80020004U;  // DISP_E_PARAMNOTFOUND

// The attribute bits that Visual Basic reports and accepts.
constexpr DWORD kVisibleAttributes = 0x37U;

std::wstring text_of(const BSTR text) {
    return text == nullptr ? std::wstring()
                           : std::wstring(text, SysStringLen(text));
}

bool has_wildcard(const std::wstring& name) {
    return name.find_first_of(L"*?") != std::wstring::npos;
}

// A Variant argument without its reference; false (with the error raised)
// when it is not a string.
bool string_argument(const VARIANT* const argument, std::wstring& text) {
    VARIANT copy;
    VariantInit(&copy);
    VariantCopyInd(&copy, const_cast<VARIANT*>(argument));
    bool good = false;
    if (copy.vt == VT_BSTR) {
        text = text_of(copy.bstrVal);
        good = true;
    } else if (copy.vt == VT_NULL) {
        static_cast<void>(vbrun::raise(kInvalidUseOfNull));
    } else {
        static_cast<void>(vbrun::raise(kTypeMismatch));
    }
    VariantClear(&copy);
    return good;
}

bool is_missing(const VARIANT* const argument) {
    return argument == nullptr ||
           (argument->vt == VT_ERROR &&
            static_cast<unsigned>(argument->scode) == kMissingScode);
}

// The error of a failed query of an existing name.
int lookup_error(const DWORD code) {
    return (code == ERROR_PATH_NOT_FOUND || code == ERROR_INVALID_DRIVE)
               ? kPathNotFound
               : kFileNotFound;
}

// The data of one file name (the name of FileLen, GetAttr and FileDateTime);
// false after raising the error.
bool find_one(const BSTR name, WIN32_FIND_DATAW& data) {
    const std::wstring path = text_of(name);
    if (has_wildcard(path)) {
        static_cast<void>(vbrun::raise(kBadFileName));
        return false;
    }
    if (path.empty()) {
        static_cast<void>(vbrun::raise(kFileNotFound));
        return false;
    }
    const HANDLE found = FindFirstFileW(path.c_str(), &data);
    if (found == INVALID_HANDLE_VALUE) {
        static_cast<void>(vbrun::raise(lookup_error(GetLastError())));
        return false;
    }
    FindClose(found);
    return true;
}

// The part of a path up to and including the last separator.
std::wstring folder_of(const std::wstring& path) {
    const auto cut = path.find_last_of(L"\\/:");
    return cut == std::wstring::npos ? std::wstring() : path.substr(0, cut + 1);
}

// The state of Dir between calls (one per thread).
struct DirState {
    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD mask = 0;
    WIN32_FIND_DATAW data{};
};

thread_local DirState g_dir;

void dir_close() {
    if (g_dir.handle != INVALID_HANDLE_VALUE) {
        FindClose(g_dir.handle);
        g_dir.handle = INVALID_HANDLE_VALUE;
    }
}

// Whether Dir with the attribute mask lists the entry: normal files always;
// hidden, system and directory entries only when asked for.
bool dir_accepts(const WIN32_FIND_DATAW& data) {
    const DWORD extra = data.dwFileAttributes & 0x16U;
    return (extra & ~g_dir.mask) == 0U;
}

// The next accepted entry (the one already found when `first`); an empty
// string at the end.
std::wstring dir_next(const bool first) {
    bool have = first;
    while (true) {
        if (!have) {
            if (!FindNextFileW(g_dir.handle, &g_dir.data)) {
                dir_close();
                return std::wstring();
            }
        }
        have = false;
        if (dir_accepts(g_dir.data)) {
            return g_dir.data.cFileName;
        }
    }
}

BSTR new_string(const std::wstring& text) {
    return SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
}

// The current folder of CurDir's drive (empty and missing: the current
// drive); false after raising an error.
bool current_folder(const VARIANT* const drive, std::wstring& folder) {
    std::wstring spec;
    if (!is_missing(drive)) {
        VARIANT copy;
        VariantInit(&copy);
        VariantCopyInd(&copy, const_cast<VARIANT*>(drive));
        const unsigned vt = copy.vt;
        if (vt == VT_BSTR) {
            spec = text_of(copy.bstrVal);
        }
        VariantClear(&copy);
        if (vt != VT_BSTR) {
            static_cast<void>(vbrun::raise(kTypeMismatch));
            return false;
        }
    }
    std::vector<wchar_t> buffer(MAX_PATH * 4U);
    DWORD length = 0;
    if (spec.empty()) {
        length = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()),
                                      buffer.data());
    } else {
        const wchar_t letter = spec.front();
        if (!((letter >= L'A' && letter <= L'Z') ||
              (letter >= L'a' && letter <= L'z'))) {
            static_cast<void>(vbrun::raise(kInvalidProcedureCall));
            return false;
        }
        const wchar_t drive_spec[3] = {letter, L':', L'\0'};
        length = GetFullPathNameW(drive_spec, static_cast<DWORD>(buffer.size()),
                                  buffer.data(), nullptr);
    }
    if (length == 0U || length >= buffer.size()) {
        static_cast<void>(vbrun::raise(kPathNotFound));
        return false;
    }
    folder.assign(buffer.data(), length);
    return true;
}

}  // namespace

extern "C" {

// FileLen(name): the size (the low 32 bits) of a file; a folder has size 0.
LONG WINAPI rtcFileLen(const BSTR name) {
    WIN32_FIND_DATAW data;
    if (!find_one(name, data)) {
        return 0;
    }
    return static_cast<LONG>(data.nFileSizeLow);
}

// GetAttr(name).
LONG WINAPI rtcGetFileAttr(const BSTR name) {
    const std::wstring path = text_of(name);
    if (has_wildcard(path)) {
        static_cast<void>(vbrun::raise(kBadFileName));
        return 0;
    }
    const DWORD attributes = path.empty() ? INVALID_FILE_ATTRIBUTES
                                          : GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        static_cast<void>(vbrun::raise(
            path.empty() ? kFileNotFound : lookup_error(GetLastError())));
        return 0;
    }
    return static_cast<LONG>(attributes & kVisibleAttributes);
}

// FileDateTime(name): the time of the last write in local time, to the second
// (c.txt at 13:45:11 gives 13:45:11).
VARIANT* WINAPI rtcFileDateTime(VARIANT* const result, const BSTR name) {
    WIN32_FIND_DATAW data;
    if (!find_one(name, data)) {
        return result;
    }
    FILETIME local;
    SYSTEMTIME time;
    double date = 0.0;
    if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &local) ||
        !FileTimeToSystemTime(&local, &time) ||
        SystemTimeToVariantTime(&time, &date) == 0) {
        static_cast<void>(vbrun::raise(kInvalidProcedureCall));
        return result;
    }
    VariantClear(result);
    result->vt = VT_DATE;
    result->date = date;
    return result;
}

// SetAttr name, attributes.
HRESULT WINAPI rtcSetFileAttr(const BSTR name, const LONG attributes) {
    if ((attributes & 0x18) != 0) {  // volume label and folder bits
        return vbrun::raise(kInvalidProcedureCall);
    }
    const std::wstring path = text_of(name);
    if (path.empty() || has_wildcard(path)) {
        return vbrun::raise(path.empty() ? kFileNotFound : kBadFileName);
    }
    const DWORD value =
        attributes == 0 ? FILE_ATTRIBUTE_NORMAL
                        : static_cast<DWORD>(attributes) & kVisibleAttributes;
    if (!SetFileAttributesW(path.c_str(), value)) {
        return vbrun::raise(lookup_error(GetLastError()));
    }
    return S_OK;
}

// MkDir name.
HRESULT WINAPI rtcMakeDir(const BSTR name) {
    const std::wstring path = text_of(name);
    if (path.empty()) {
        return vbrun::raise(kPathNotFound);
    }
    if (!CreateDirectoryW(path.c_str(), nullptr)) {
        const DWORD code = GetLastError();
        return vbrun::raise((code == ERROR_PATH_NOT_FOUND) ? kPathNotFound
                                                           : kPathFileAccess);
    }
    return S_OK;
}

// RmDir name.
HRESULT WINAPI rtcRemoveDir(const BSTR name) {
    const std::wstring path = text_of(name);
    if (path.empty()) {
        return vbrun::raise(kPathNotFound);
    }
    if (!RemoveDirectoryW(path.c_str())) {
        const DWORD code = GetLastError();
        const bool missing = code == ERROR_FILE_NOT_FOUND ||
                             code == ERROR_PATH_NOT_FOUND ||
                             code == ERROR_DIRECTORY;
        return vbrun::raise(missing ? kPathNotFound : kPathFileAccess);
    }
    return S_OK;
}

// ChDir name.
HRESULT WINAPI rtcChangeDir(const BSTR name) {
    const std::wstring path = text_of(name);
    if (path.empty() || !SetCurrentDirectoryW(path.c_str())) {
        return vbrun::raise(kPathNotFound);
    }
    return S_OK;
}

// Kill pattern: deletes the normal and read-only files that match; no match
// is error 53, a file that cannot be deleted is error 75.
HRESULT WINAPI rtcKillFiles(VARIANT* const pattern) {
    std::wstring path;
    if (!string_argument(pattern, path)) {
        return vbrun::raise(vbrun::last_error());
    }
    if (path.empty()) {
        return vbrun::raise(kFileNotFound);
    }
    WIN32_FIND_DATAW data;
    const HANDLE found = FindFirstFileW(path.c_str(), &data);
    if (found == INVALID_HANDLE_VALUE) {
        return vbrun::raise(kFileNotFound);
    }
    const std::wstring folder = folder_of(path);
    bool any = false;
    int error = 0;
    do {
        if ((data.dwFileAttributes & 0x16U) != 0U) {
            continue;  // hidden, system and folders are not matched
        }
        any = true;
        if (!DeleteFileW((folder + data.cFileName).c_str())) {
            error = kPathFileAccess;
            break;
        }
    } while (FindNextFileW(found, &data));
    FindClose(found);
    if (error != 0) {
        return vbrun::raise(error);
    }
    return any ? S_OK : vbrun::raise(kFileNotFound);
}

// FileCopy source, destination: copies the contents (the copy is a normal
// file, whatever the attributes of the source).
HRESULT WINAPI rtcFileCopy(const BSTR source, const BSTR destination) {
    const std::wstring from = text_of(source);
    const std::wstring to = text_of(destination);
    if (has_wildcard(from) || has_wildcard(to)) {
        return vbrun::raise(kBadFileName);
    }
    if (from.empty() || to.empty()) {
        return vbrun::raise(kPathFileAccess);
    }
    // Reading shares only reading, so copying a file onto itself fails the
    // second open with a sharing violation (error 70).
    const HANDLE in =
        CreateFileW(from.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (in == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        return vbrun::raise(code == ERROR_FILE_NOT_FOUND
                                ? kFileNotFound
                                : (code == ERROR_PATH_NOT_FOUND
                                       ? kPathNotFound
                                       : kPathFileAccess));
    }
    const HANDLE out =
        CreateFileW(to.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        CloseHandle(in);
        return vbrun::raise(code == ERROR_SHARING_VIOLATION
                                ? kAccessDenied
                                : (code == ERROR_PATH_NOT_FOUND
                                       ? kPathNotFound
                                       : kPathFileAccess));
    }
    std::vector<char> buffer(65536U);
    int error = 0;
    DWORD got = 0;
    while (ReadFile(in, buffer.data(), static_cast<DWORD>(buffer.size()), &got,
                    nullptr) &&
           got != 0U) {
        DWORD put = 0;
        if (!WriteFile(out, buffer.data(), got, &put, nullptr) || put != got) {
            error = kPathFileAccess;
            break;
        }
    }
    CloseHandle(out);
    CloseHandle(in);
    return error == 0 ? S_OK : vbrun::raise(error);
}

// Dir$(pattern, attributes) and the continuation Dir$ (a missing pattern).
BSTR WINAPI rtcDir(VARIANT* const pattern, const LONG attributes) {
    if (is_missing(pattern)) {
        if (g_dir.handle == INVALID_HANDLE_VALUE) {
            static_cast<void>(vbrun::raise(kInvalidProcedureCall));
            return nullptr;
        }
        return new_string(dir_next(false));
    }
    std::wstring path;
    if (!string_argument(pattern, path)) {
        return nullptr;
    }
    dir_close();
    g_dir.mask = static_cast<DWORD>(attributes) & 0x16U;
    if (path.empty() || path.back() == L'\\' || path.back() == L'/') {
        path += L"*";  // a folder with a separator lists its contents
    }
    g_dir.handle = FindFirstFileW(path.c_str(), &g_dir.data);
    if (g_dir.handle == INVALID_HANDLE_VALUE) {
        return new_string(std::wstring());
    }
    return new_string(dir_next(true));
}

// CurDir$(drive).
BSTR WINAPI rtcCurrentDirBstr(VARIANT* const drive) {
    std::wstring folder;
    if (!current_folder(drive, folder)) {
        return nullptr;
    }
    return new_string(folder);
}

// CurDir(drive) as a Variant.
VARIANT* WINAPI rtcCurrentDir(VARIANT* const result, VARIANT* const drive) {
    std::wstring folder;
    if (!current_folder(drive, folder)) {
        return result;
    }
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal = new_string(folder);
    return result;
}

}  // extern "C"
