// REQ-0103: Environ.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/system.cases). A name that is not set gives an empty string; a
// number selects the entry "NAME=value" of that position (a number below 1 is
// error 5).

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
        if (owner == search->process && GetWindow(window, GW_OWNER) == nullptr) {
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

}  // extern "C"
