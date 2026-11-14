// REQ-0103: Environ.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/system.cases). A name that is not set gives an empty string; a
// number selects the entry "NAME=value" of that position (a number below 1 is
// error 5).

#include <cwctype>
#include <string>
#include <vector>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kInvalidUseOfNull = 94;

// The text of Environ(name or number); false after raising an error.
bool environment_text(const VARIANT* const argument, std::wstring& text) {
    const unsigned vt = argument->vt & ~static_cast<unsigned>(VT_BYREF);
    if (vt == VT_NULL) {
        static_cast<void>(vbrun::raise(kInvalidUseOfNull));
        return false;
    }
    VARIANT converted;
    VariantInit(&converted);
    if (vt == VT_BSTR) {
        VARIANT copy;
        VariantInit(&copy);
        VariantCopyInd(&copy, const_cast<VARIANT*>(argument));
        const std::wstring name(copy.bstrVal == nullptr ? L"" : copy.bstrVal,
                                SysStringLen(copy.bstrVal));
        VariantClear(&copy);
        text.clear();
        if (name.empty()) {
            return true;
        }
        const DWORD needed = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
        if (needed > 1U) {
            std::vector<wchar_t> buffer(needed);
            const DWORD length =
                GetEnvironmentVariableW(name.c_str(), buffer.data(), needed);
            text.assign(buffer.data(), length);
        }
        return true;
    }
    const HRESULT status =
        VariantChangeType(&converted, const_cast<VARIANT*>(argument), 0, VT_I4);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    if (converted.lVal < 1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return false;
    }
    text.clear();
    wchar_t* const block = GetEnvironmentStringsW();
    LONG position = 1;
    for (const wchar_t* at = block; *at != L'\0'; at += wcslen(at) + 1U) {
        if (*at == L'=') {
            continue;  // the entries of the drives' current directories
        }
        if (position++ == converted.lVal) {
            text = at;
            break;
        }
    }
    FreeEnvironmentStringsW(block);
    return true;
}

}  // namespace

namespace {

struct WindowSearch {
    std::wstring wanted;
    DWORD process = 0;
    bool by_process = false;
    bool prefix = false;
    HWND found = nullptr;
};

BOOL CALLBACK match_window(HWND window, LPARAM parameter) {
    auto* const search = reinterpret_cast<WindowSearch*>(parameter);
    if (IsWindowVisible(window) == 0 || IsWindowEnabled(window) == 0) {
        return TRUE;
    }
    if (search->by_process) {
        DWORD owner = 0;
        GetWindowThreadProcessId(window, &owner);
        if (owner == search->process &&
            GetWindow(window, GW_OWNER) == nullptr) {
            search->found = window;
            return FALSE;
        }
        return TRUE;
    }
    wchar_t title[512] = {};
    const int length = GetWindowTextW(window, title, 511);
    if (length <= 0) {
        return TRUE;
    }
    const std::size_t wanted = search->wanted.size();
    const std::size_t compared =
        search->prefix ? wanted : static_cast<std::size_t>(length);
    if (static_cast<std::size_t>(length) >= wanted && wanted != 0 &&
        (search->prefix || static_cast<std::size_t>(length) == wanted) &&
        CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, title,
                       static_cast<int>(compared), search->wanted.c_str(),
                       static_cast<int>(wanted)) == CSTR_EQUAL) {
        search->found = window;
        return FALSE;
    }
    return TRUE;
}

}  // namespace

namespace {

// What the InputBox dialog shows and gives back.
struct InputDialog {
    std::wstring prompt;
    std::wstring caption;
    std::wstring text;
    bool accepted = false;
};

constexpr int kPromptId = 100;
constexpr int kEditId = 101;

INT_PTR CALLBACK input_dialog_proc(HWND dialog, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
    auto* const state = reinterpret_cast<InputDialog*>(
        GetWindowLongPtrW(dialog, GWLP_USERDATA));
    switch (message) {
        case WM_INITDIALOG: {
            SetWindowLongPtrW(dialog, GWLP_USERDATA, lparam);
            auto* const initial = reinterpret_cast<InputDialog*>(lparam);
            SetWindowTextW(dialog, initial->caption.c_str());
            SetDlgItemTextW(dialog, kPromptId, initial->prompt.c_str());
            SetDlgItemTextW(dialog, kEditId, initial->text.c_str());
            SendDlgItemMessageW(dialog, kEditId, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(dialog, kEditId));
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == IDOK && state != nullptr) {
                wchar_t buffer[4096] = {};
                GetDlgItemTextW(dialog, kEditId, buffer, 4095);
                state->text = buffer;
                state->accepted = true;
                EndDialog(dialog, IDOK);
                return TRUE;
            }
            if (LOWORD(wparam) == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            return FALSE;
        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            return FALSE;
    }
}

void add_word(std::vector<WORD>& data, const WORD value) {
    data.push_back(value);
}

void add_text(std::vector<WORD>& data, const wchar_t* const text) {
    for (const wchar_t* at = text; *at != L'\0'; ++at) {
        data.push_back(static_cast<WORD>(*at));
    }
    data.push_back(0);
}

void align_dword(std::vector<WORD>& data) {
    if ((data.size() & 1U) != 0) {
        data.push_back(0);
    }
}

void add_item(std::vector<WORD>& data, const DWORD style, const short x,
              const short y, const short width, const short height,
              const WORD id, const WORD class_atom) {
    align_dword(data);
    add_word(data, static_cast<WORD>(style & 0xFFFFU));
    add_word(data, static_cast<WORD>(style >> 16U));
    add_word(data, 0);
    add_word(data, 0);
    add_word(data, static_cast<WORD>(x));
    add_word(data, static_cast<WORD>(y));
    add_word(data, static_cast<WORD>(width));
    add_word(data, static_cast<WORD>(height));
    add_word(data, id);
    add_word(data, 0xFFFF);
    add_word(data, class_atom);
    add_word(data, 0);  // no initial text
    add_word(data, 0);  // no creation data
}

// The dialog template: a prompt, an edit box, OK and Cancel.
std::vector<WORD> input_template() {
    std::vector<WORD> data;
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME |
                        DS_CENTER | DS_SETFONT;
    add_word(data, static_cast<WORD>(style & 0xFFFFU));
    add_word(data, static_cast<WORD>(style >> 16U));
    add_word(data, 0);
    add_word(data, 0);
    add_word(data, 4);  // items
    add_word(data, 0);
    add_word(data, 0);
    add_word(data, 260);
    add_word(data, 96);
    add_word(data, 0);  // no menu
    add_word(data, 0);  // default class
    add_word(data, 0);  // title set at run time
    add_word(data, 8);  // font size
    add_text(data, L"MS Shell Dlg");
    add_item(data, WS_CHILD | WS_VISIBLE | SS_LEFT, 8, 8, 190, 48,
             kPromptId, 0x0082);
    add_item(data, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
             206, 8, 46, 14, IDOK, 0x0080);
    add_item(data, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 206,
             26, 46, 14, IDCANCEL, 0x0080);
    add_item(data,
             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
             8, 72, 244, 14, kEditId, 0x0081);
    return data;
}

bool variant_text(const VARIANT* const value, std::wstring& text) {
    if (value == nullptr || value->vt == VT_ERROR || value->vt == VT_EMPTY) {
        return false;
    }
    VARIANT converted;
    VariantInit(&converted);
    if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(value), 0,
                                 VT_BSTR))) {
        return false;
    }
    text.assign(converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                SysStringLen(converted.bstrVal));
    VariantClear(&converted);
    return true;
}

}  // namespace

namespace {

// Sets the local date and/or time; the part not given keeps its value. Setting
// the clock needs the privilege for it: without it, error 70 (permission
// denied).
int set_clock(const SYSTEMTIME& wanted, const bool date, const bool time) {
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    if (date) {
        now.wYear = wanted.wYear;
        now.wMonth = wanted.wMonth;
        now.wDay = wanted.wDay;
    }
    if (time) {
        now.wHour = wanted.wHour;
        now.wMinute = wanted.wMinute;
        now.wSecond = wanted.wSecond;
        now.wMilliseconds = 0;
    }
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token) == 0) {
        return 70;
    }
    TOKEN_PRIVILEGES privileges = {};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueW(nullptr, L"SeSystemtimePrivilege",
                              &privileges.Privileges[0].Luid) == 0) {
        CloseHandle(token);
        return 70;
    }
    AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
    const bool adjusted = GetLastError() == ERROR_SUCCESS;
    CloseHandle(token);
    if (!adjusted || SetLocalTime(&now) == 0) {
        return 70;
    }
    return 0;
}

// Date or Time of a text or a Date Variant as a clock reading; false after
// raising error 13 (type mismatch) or 5.
bool clock_of(const VARIANT* const value, SYSTEMTIME& reading) {
    if (value == nullptr) {
        static_cast<void>(vbrun::raise(5));
        return false;
    }
    VARIANT plain;
    VariantInit(&plain);
    VARIANT date;
    VariantInit(&date);
    bool ok = false;
    if (SUCCEEDED(VariantCopyInd(&plain, const_cast<VARIANT*>(value))) &&
        (plain.vt == VT_BSTR || plain.vt == VT_DATE)) {
        ok = SUCCEEDED(VariantChangeType(&date, &plain, 0, VT_DATE)) &&
             VariantTimeToSystemTime(date.date, &reading) != 0;
    }
    VariantClear(&plain);
    VariantClear(&date);
    if (!ok) {
        static_cast<void>(vbrun::raise(13));
    }
    return ok;
}

}  // namespace

namespace {

using ImeContextFn = HANDLE(WINAPI*)(HWND);
using ImeReleaseFn = BOOL(WINAPI*)(HWND, HANDLE);
using ImeIsFn = BOOL(WINAPI*)(HKL);
using ImeOpenFn = BOOL(WINAPI*)(HANDLE);
using ImeConversionFn = BOOL(WINAPI*)(HANDLE, DWORD*, DWORD*);

// The state of the input method editor of the window that has the focus, in
// the numbers of the vbIMEModes of Visual Basic (-1 when there is none to ask).
LONG ime_state() {
    const HMODULE imm = LoadLibraryW(L"imm32.dll");
    if (imm == nullptr) {
        return -1;
    }
    const auto context = reinterpret_cast<ImeContextFn>(
        GetProcAddress(imm, "ImmGetContext"));
    const auto release = reinterpret_cast<ImeReleaseFn>(
        GetProcAddress(imm, "ImmReleaseContext"));
    const auto is_ime =
        reinterpret_cast<ImeIsFn>(GetProcAddress(imm, "ImmIsIME"));
    const auto is_open = reinterpret_cast<ImeOpenFn>(
        GetProcAddress(imm, "ImmGetOpenStatus"));
    const auto conversion = reinterpret_cast<ImeConversionFn>(
        GetProcAddress(imm, "ImmGetConversionStatus"));
    if (context == nullptr || release == nullptr || is_ime == nullptr ||
        is_open == nullptr || conversion == nullptr) {
        return -1;
    }
    const HWND window = GetFocus();
    const HANDLE input = context(window);
    const WORD language = PRIMARYLANGID(GetSystemDefaultLangID());
    LONG state = -1;
    DWORD mode = 0;
    DWORD sentence = 0;
    const bool japanese = language == LANG_JAPANESE;
    const bool korean = language == LANG_KOREAN;
    if (input == nullptr) {
        state = japanese ? 3 : -1;
    } else if (!is_ime(GetKeyboardLayout(0)) || !is_open(input)) {
        state = japanese ? 2 : (korean ? 8 : 2);
    } else if (conversion(input, &mode, &sentence) == 0) {
        state = -1;
    } else if (japanese) {
        if ((mode & 1U) != 0) {
            state = (mode & 2U) != 0 ? ((mode & 8U) != 0 ? 5 : 6) : 4;
        } else {
            state = (mode & 8U) != 0 ? 7 : 8;
        }
    } else if (korean) {
        state = (mode & 1U) != 0 ? ((mode & 8U) != 0 ? 9 : 10)
                                 : ((mode & 8U) != 0 ? 7 : 8);
    } else {
        state = (mode & 1U) != 0 ? 1 : 2;
    }
    if (input != nullptr) {
        release(window, input);
    }
    return state;
}

}  // namespace

namespace {

struct NamedKey {
    const wchar_t* name;
    WORD key;
};

const NamedKey kNamedKeys[] = {
    {L"BACKSPACE", VK_BACK}, {L"BS", VK_BACK},        {L"BKSP", VK_BACK},
    {L"BREAK", VK_CANCEL},   {L"CAPSLOCK", VK_CAPITAL},
    {L"DELETE", VK_DELETE},  {L"DEL", VK_DELETE},     {L"DOWN", VK_DOWN},
    {L"END", VK_END},        {L"ENTER", VK_RETURN},   {L"ESC", VK_ESCAPE},
    {L"HELP", VK_HELP},      {L"HOME", VK_HOME},      {L"INSERT", VK_INSERT},
    {L"INS", VK_INSERT},     {L"LEFT", VK_LEFT},      {L"NUMLOCK", VK_NUMLOCK},
    {L"PGDN", VK_NEXT},      {L"PGUP", VK_PRIOR},     {L"PRTSC", VK_SNAPSHOT},
    {L"RIGHT", VK_RIGHT},    {L"SCROLLLOCK", VK_SCROLL},
    {L"TAB", VK_TAB},        {L"UP", VK_UP},
};

constexpr unsigned kShift = 1;
constexpr unsigned kControl = 2;
constexpr unsigned kAlt = 4;

struct KeyParser {
    const std::wstring& text;
    std::vector<vbrun::KeyEvent>& events;
    std::size_t at = 0;

    void modifiers_down(const unsigned modifiers) {
        const WORD keys[3] = {VK_SHIFT, VK_CONTROL, VK_MENU};
        for (int bit = 0; bit < 3; ++bit) {
            if ((modifiers & (1U << bit)) != 0) {
                events.push_back({keys[bit], false, false});
            }
        }
    }

    void modifiers_up(const unsigned modifiers) {
        const WORD keys[3] = {VK_SHIFT, VK_CONTROL, VK_MENU};
        for (int bit = 2; bit >= 0; --bit) {
            if ((modifiers & (1U << bit)) != 0) {
                events.push_back({keys[bit], true, false});
            }
        }
    }

    void press(const WORD key, const bool unicode, const unsigned modifiers,
               const int count) {
        modifiers_down(modifiers);
        for (int again = 0; again < count; ++again) {
            events.push_back({key, false, unicode});
            events.push_back({key, true, unicode});
        }
        modifiers_up(modifiers);
    }

    void literal(const wchar_t character, const unsigned modifiers,
                 const int count) {
        const SHORT scan = VkKeyScanW(character);
        if (scan == -1) {
            press(static_cast<WORD>(character), true, modifiers, count);
            return;
        }
        const unsigned shifts = static_cast<unsigned>(scan >> 8) & 7U;
        press(static_cast<WORD>(scan & 0xFF), false, modifiers | shifts,
              count);
    }

    // {NAME}, {NAME count}, {character}, {character count}.
    bool braced(const unsigned modifiers) {
        std::size_t close = at + 1;
        if (close < text.size() && text[close] == L'}') {
            ++close;  // the character } itself
        }
        close = text.find(L'}', close);
        if (close == std::wstring::npos) {
            return false;
        }
        std::wstring body = text.substr(at + 1, close - at - 1);
        at = close + 1;
        int count = 1;
        const std::size_t blank = body.find(L' ', 1);
        if (blank != std::wstring::npos) {
            const std::wstring digits = body.substr(blank + 1);
            if (digits.empty() ||
                digits.find_first_not_of(L"0123456789") !=
                    std::wstring::npos) {
                return false;
            }
            count = std::stoi(digits);
            body.erase(blank);
        }
        if (body.size() == 1) {
            literal(body[0], modifiers, count);
            return true;
        }
        for (wchar_t& character : body) {
            character = static_cast<wchar_t>(std::towupper(character));
        }
        if (body.size() >= 2 && body[0] == L'F') {
            const long number = std::wcstol(body.c_str() + 1, nullptr, 10);
            if (number >= 1 && number <= 16) {
                press(static_cast<WORD>(VK_F1 + number - 1), false, modifiers,
                      count);
                return true;
            }
        }
        for (const NamedKey& named : kNamedKeys) {
            if (body == named.name) {
                press(named.key, false, modifiers, count);
                return true;
            }
        }
        return false;
    }

    // Keys up to the end, or up to the ) that closes a group.
    bool sequence(const bool in_group) {
        unsigned pending = 0;
        while (at < text.size()) {
            const wchar_t character = text[at];
            if (character == L'+' || character == L'^' || character == L'%') {
                pending |= character == L'+'   ? kShift
                           : character == L'^' ? kControl
                                               : kAlt;
                ++at;
                continue;
            }
            if (character == L')') {
                return in_group && pending == 0;
            }
            if (character == L'(') {
                ++at;
                std::vector<vbrun::KeyEvent> inner;
                KeyParser group{text, inner, at};
                if (!group.sequence(true) || group.at >= text.size()) {
                    return false;
                }
                at = group.at + 1;
                modifiers_down(pending);
                events.insert(events.end(), inner.begin(), inner.end());
                modifiers_up(pending);
            } else if (character == L'{') {
                if (!braced(pending)) {
                    return false;
                }
            } else if (character == L'~') {
                press(VK_RETURN, false, pending, 1);
                ++at;
            } else {
                literal(character, pending, 1);
                ++at;
            }
            pending = 0;
        }
        return !in_group && pending == 0;
    }
};

}  // namespace

namespace vbrun {

bool parse_send_keys(const std::wstring& text, std::vector<KeyEvent>& events) {
    events.clear();
    KeyParser parser{text, events, 0};
    if (!parser.sequence(false)) {
        events.clear();
        return false;
    }
    return true;
}

}  // namespace vbrun

extern "C" {

// Environ$(name or number).
BSTR WINAPI rtcEnvironBstr(VARIANT* const argument) {
    std::wstring text;
    if (!environment_text(argument, text)) {
        return nullptr;
    }
    return SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
}

// Environ(name or number) as a Variant.
VARIANT* WINAPI rtcEnvironVar(VARIANT* const result, VARIANT* const argument) {
    std::wstring text;
    if (!environment_text(argument, text)) {
        return result;
    }
    VariantClear(result);
    result->vt = VT_BSTR;
    result->bstrVal =
        SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    return result;
}

// Beep.
void WINAPI rtcBeep() {
    MessageBeep(0);
}

// DoEvents: lets the program's windows handle their messages; the result is
// the number of loaded forms, which a library does not have.
SHORT WINAPI rtcDoEvents() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}

// The locale of the host (the user's).
LONG WINAPI rtcGetHostLCID() {
    return static_cast<LONG>(GetUserDefaultLCID());
}

// The calendar: 0 for the Gregorian one, 1 for the Hijri one. The date
// functions of this library work in the Gregorian calendar whichever is set.
static SHORT g_calendar = 0;

SHORT WINAPI rtcGetCurrentCalendar() {
    return g_calendar;
}

// Calendar = 0 or 1 (error 5 for any other value).
void WINAPI rtcSetCurrentCalendar(const LONG calendar) {
    if (calendar < 0 || calendar > 1) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    g_calendar = static_cast<SHORT>(calendar);
}

// A String of one character from an ANSI code (0 to 255), and its Variant
// form.
BSTR WINAPI rtcBstrFromAnsi(const LONG code) {
    if ((code & ~0xFF) != 0) {
        static_cast<void>(vbrun::raise(5));
        return nullptr;
    }
    const char byte = static_cast<char>(code);
    wchar_t wide = 0;
    if (MultiByteToWideChar(CP_ACP, 0, &byte, 1, &wide, 1) != 1) {
        wide = static_cast<wchar_t>(static_cast<unsigned char>(byte));
    }
    return SysAllocStringLen(&wide, 1);
}

VARIANT* WINAPI rtcVarBstrFromAnsi(VARIANT* const result, const LONG code) {
    VariantInit(result);
    BSTR text = rtcBstrFromAnsi(code);
    if (text != nullptr) {
        result->vt = VT_BSTR;
        result->bstrVal = text;
    }
    return result;
}

// A String of one byte (a length of one byte, not one character).
VARIANT* WINAPI rtcVarBstrFromByte(VARIANT* const result, const LONG code) {
    VariantInit(result);
    const char byte = static_cast<char>(code);
    result->bstrVal = SysAllocStringByteLen(&byte, 1);
    if (result->bstrVal == nullptr) {
        static_cast<void>(vbrun::raise(14));
        return result;
    }
    result->vt = VT_BSTR;
    return result;
}

// Command$: the command line after the program name, without the spaces
// that follow the name and otherwise as typed.
BSTR WINAPI rtcCommandBstr() {
    const wchar_t* at = GetCommandLineW();
    if (at == nullptr) {
        return SysAllocStringLen(L"", 0);
    }
    if (*at == L'"') {
        ++at;
        while (*at != 0 && *at != L'"') {
            ++at;
        }
        if (*at == L'"') {
            ++at;
        }
    } else {
        while (*at != 0 && *at != L' ' && *at != L'	') {
            ++at;
        }
    }
    while (*at == L' ' || *at == L'	') {
        ++at;
    }
    return SysAllocString(at);
}

VARIANT* WINAPI rtcCommandVar(VARIANT* const result) {
    VariantInit(result);
    result->bstrVal = rtcCommandBstr();
    result->vt = VT_BSTR;
    return result;
}

// Shell(command, style): starts the program and gives its process id. The
// window style is 0 to 9 (error 5 otherwise); a program that cannot be
// started is error 53 (not found), 76 (path not found) or 7 (memory).
double WINAPI rtcShell(const VARIANT* const command, const LONG style) {
    if (static_cast<unsigned>(style) > 9U) {
        static_cast<void>(vbrun::raise(5));
        return 0.0;
    }
    std::wstring line;
    if (command != nullptr) {
        VARIANT text;
        VariantInit(&text);
        if (FAILED(VariantChangeType(&text, const_cast<VARIANT*>(command), 0,
                                     VT_BSTR))) {
            static_cast<void>(vbrun::raise(13));
            return 0.0;
        }
        line.assign(text.bstrVal == nullptr ? L"" : text.bstrVal,
                    SysStringLen(text.bstrVal));
        VariantClear(&text);
    }
    line.push_back(L'\0');
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = static_cast<WORD>(style);
    PROCESS_INFORMATION process = {};
    SetLastError(0);
    if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0,
                       nullptr, nullptr, &startup, &process) == 0) {
        switch (GetLastError()) {
            case ERROR_FILE_NOT_FOUND:
                static_cast<void>(vbrun::raise(53));
                break;
            case ERROR_PATH_NOT_FOUND:
                static_cast<void>(vbrun::raise(76));
                break;
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY:
                static_cast<void>(vbrun::raise(7));
                break;
            default:
                static_cast<void>(vbrun::raise(5));
                break;
        }
        return 0.0;
    }
    WaitForInputIdle(process.hProcess, 10000);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<double>(process.dwProcessId);
}

// MsgBox(prompt, buttons, title): the dialog and the number of the button
// pressed. Buttons outside the Visual Basic constants count as vbOKOnly. A
// missing title is the name of the program.
SHORT WINAPI rtcMsgBox(const VARIANT* const prompt, LONG buttons,
                       const VARIANT* const title,
                       const VARIANT* const help_file,
                       const VARIANT* const context) {
    static_cast<void>(help_file);
    static_cast<void>(context);
    if ((buttons & 0xF) > 5 || (buttons & 0xF0) > 0x40 ||
        (buttons & 0xF00) > 0x300) {
        buttons = 0;
    }
    if (prompt != nullptr && prompt->vt == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return 0;
    }
    std::wstring text;
    if (prompt != nullptr && prompt->vt != VT_EMPTY) {
        VARIANT converted;
        VariantInit(&converted);
        if (FAILED(VariantChangeType(&converted,
                                     const_cast<VARIANT*>(prompt), 0,
                                     VT_BSTR))) {
            static_cast<void>(vbrun::raise(13));
            return 0;
        }
        text.assign(converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                    SysStringLen(converted.bstrVal));
        VariantClear(&converted);
    }
    std::wstring caption;
    const bool titled = title != nullptr && title->vt != VT_ERROR;
    if (titled) {
        VARIANT converted;
        VariantInit(&converted);
        if (SUCCEEDED(VariantChangeType(&converted,
                                        const_cast<VARIANT*>(title), 0,
                                        VT_BSTR))) {
            caption.assign(
                converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                SysStringLen(converted.bstrVal));
        }
        VariantClear(&converted);
    } else {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        caption = path;
        const auto slash = caption.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            caption.erase(0, slash + 1);
        }
        const auto dot = caption.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            caption.erase(dot);
        }
    }
    return static_cast<SHORT>(MessageBoxW(GetActiveWindow(), text.c_str(),
                                          caption.c_str(),
                                          static_cast<UINT>(buttons)));
}

// AppActivate(title) or AppActivate(process id): brings the window of an
// application to the front. A window whose title is the text is preferred to
// one whose title begins with it; no such window is error 5.
void WINAPI rtcAppActivate(const VARIANT* const title,
                           const VARIANT* const wait) {
    static_cast<void>(wait);
    WindowSearch search;
    if (title == nullptr || title->vt == VT_ERROR) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    if (title->vt == VT_BSTR) {
        search.wanted.assign(title->bstrVal == nullptr ? L"" : title->bstrVal,
                             SysStringLen(title->bstrVal));
    } else {
        VARIANT number;
        VariantInit(&number);
        if (FAILED(VariantChangeType(&number, const_cast<VARIANT*>(title), 0,
                                     VT_I4))) {
            static_cast<void>(vbrun::raise(13));
            return;
        }
        search.by_process = true;
        search.process = static_cast<DWORD>(number.lVal);
    }
    EnumWindows(match_window, reinterpret_cast<LPARAM>(&search));
    if (search.found == nullptr && !search.by_process) {
        search.prefix = true;
        EnumWindows(match_window, reinterpret_cast<LPARAM>(&search));
    }
    if (search.found == nullptr) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    if (IsIconic(search.found) != 0) {
        ShowWindow(search.found, SW_RESTORE);
    }
    SetForegroundWindow(search.found);
}

// InputBox(prompt, title, default, x, y, helpfile, context): the dialog and
// the text typed; an empty string when the dialog is cancelled. A Null prompt
// is error 94.
BSTR WINAPI rtcInputBox(const VARIANT* const prompt, const VARIANT* const title,
                        const VARIANT* const initial, const VARIANT* const x,
                        const VARIANT* const y, const VARIANT* const help_file,
                        const VARIANT* const context) {
    static_cast<void>(x);
    static_cast<void>(y);
    static_cast<void>(help_file);
    static_cast<void>(context);
    if (prompt != nullptr && prompt->vt == VT_NULL) {
        static_cast<void>(vbrun::raise(94));
        return nullptr;
    }
    InputDialog state;
    variant_text(prompt, state.prompt);
    if (!variant_text(title, state.caption)) {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        state.caption = path;
        const auto slash = state.caption.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            state.caption.erase(0, slash + 1);
        }
        const auto dot = state.caption.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            state.caption.erase(dot);
        }
    }
    variant_text(initial, state.text);
    const std::vector<WORD> data = input_template();
    DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        reinterpret_cast<const DLGTEMPLATE*>(data.data()), GetActiveWindow(),
        input_dialog_proc, reinterpret_cast<LPARAM>(&state));
    return SysAllocString(state.accepted ? state.text.c_str() : L"");
}


// Date = value and Time = value as statements: the clock of the computer.
void WINAPI rtcSetDateVar(const VARIANT* const value) {
    SYSTEMTIME reading = {};
    if (clock_of(value, reading)) {
        const int failure = set_clock(reading, true, false);
        if (failure != 0) {
            static_cast<void>(vbrun::raise(failure));
        }
    }
}

void WINAPI rtcSetDateBstr(const BSTR text) {
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = text;
    rtcSetDateVar(&value);
}

void WINAPI rtcSetTimeVar(const VARIANT* const value) {
    SYSTEMTIME reading = {};
    if (clock_of(value, reading)) {
        const int failure = set_clock(reading, false, true);
        if (failure != 0) {
            static_cast<void>(vbrun::raise(failure));
        }
    }
}

void WINAPI rtcSetTimeBstr(const BSTR text) {
    if (text == nullptr) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = text;
    rtcSetTimeVar(&value);
}


// IMEStatus: the mode of the input method editor (-1 when there is none).
LONG WINAPI rtcIMEStatus() {
    return ime_state();
}

// A Friend member called on an object that is not an instance of the class
// that defines it is error 97.
void WINAPI __vbaFailedFriend() {
    static_cast<void>(vbrun::raise(97));
}


// SendKeys string, wait: the keys go to the window that has the focus.
void WINAPI rtcSendKeys(const BSTR keys, const VARIANT* const wait) {
    const std::wstring text(keys == nullptr ? L"" : keys, SysStringLen(keys));
    std::vector<vbrun::KeyEvent> events;
    if (!vbrun::parse_send_keys(text, events)) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    bool waiting = false;
    if (wait != nullptr && wait->vt != VT_ERROR) {
        VARIANT flag;
        VariantInit(&flag);
        if (SUCCEEDED(VariantChangeType(&flag, const_cast<VARIANT*>(wait), 0,
                                        VT_BOOL))) {
            waiting = flag.boolVal != VARIANT_FALSE;
        }
    }
    std::vector<INPUT> inputs;
    for (const vbrun::KeyEvent& event : events) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        if (event.unicode) {
            input.ki.wScan = event.key;
            input.ki.dwFlags = KEYEVENTF_UNICODE;
        } else {
            input.ki.wVk = event.key;
        }
        if (event.up) {
            input.ki.dwFlags |= KEYEVENTF_KEYUP;
        }
        inputs.push_back(input);
    }
    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(),
                  sizeof(INPUT));
        if (waiting) {
            Sleep(10);
        }
    }
}


// AppleScript is a Macintosh feature: on Windows it is error 5.
void WINAPI rtcAppleScript() {
    static_cast<void>(vbrun::raise(5));
}

}  // extern "C"
