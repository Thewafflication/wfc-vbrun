// Unit tests of the smaller exports: Erl and LastDllError, fixed strings,
// Erase, ReDim, Variant array access, For loops of Currency, MacID, the
// calendar, Shell, StrConv with a locale, MsgBox, InputBox and AppActivate.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
LONG WINAPI rtcGetErl();
void WINAPI __vbaSetSystemError();
BSTR WINAPI __vbaStrFixstr(LONG length, const wchar_t* text);
wchar_t* WINAPI __vbaLsetFixstr(LONG length, wchar_t* target, BSTR source);
wchar_t* WINAPI __vbaLsetFixstrFree(LONG length, wchar_t* target, BSTR source);
wchar_t* WINAPI __vbaRsetFixstr(LONG length, wchar_t* target, BSTR source);
wchar_t* WINAPI __vbaRsetFixstrFree(LONG length, wchar_t* target, BSTR source);
LONG WINAPI __vbaAptOffset(LONG value);
BSTR WINAPI __vbaStrVarCopy(VARIANT* value);
LONG WINAPI __vbaErase(const void* elements, SAFEARRAY** slot);
void WINAPI __vbaEraseKeepData(const void* elements, SAFEARRAY** slot);
void WINAPI __vbaAryConstruct(SAFEARRAY* destination,
                              const SAFEARRAY* declaration);
LONG WINAPI __vbaCyForInit(LONGLONG* counter, LONGLONG start, LONGLONG limit,
                           LONGLONG step);
LONG WINAPI __vbaCyForNext(LONGLONG* counter, LONGLONG limit, LONGLONG step);
LONG WINAPI rtcMacId(BSTR text);
void WINAPI rtcSetCurrentCalendar(LONG calendar);
SHORT WINAPI rtcGetCurrentCalendar();
VARIANT* WINAPI __vbaVarSetVar(VARIANT* destination, VARIANT* source);
VARIANT* WINAPI __vbaVarSetVarAddref(VARIANT* destination, VARIANT* source);
double WINAPI rtcShell(const VARIANT* command, LONG style);
HRESULT WINAPI __vbaHresultCheckObj(HRESULT result, IUnknown* object,
                                    const IID* iid, LONG extra);
LONG WINAPI rtcIMEStatus();
void WINAPI __vbaFailedFriend();
VARIANT* __cdecl __vbaVarIndexLoadRef(VARIANT* result, const VARIANT* holder,
                                      LONG count, ...);
VARIANT* __cdecl __vbaVarIndexLoadRefLock(VARIANT* result,
                                          const VARIANT* holder,
                                          SAFEARRAY** locked, LONG count,
                                          ...);
LONG WINAPI __vbaVarForInit(VARIANT* counter, VARIANT* step, VARIANT* limit,
                            const VARIANT* start, const VARIANT* limit_in,
                            const VARIANT* step_in);
LONG WINAPI __vbaVarForNext(VARIANT* counter, VARIANT* step, VARIANT* limit);
void WINAPI rtcSetDateVar(const VARIANT* value);
void WINAPI rtcSetDateBstr(BSTR text);
void WINAPI rtcSetTimeVar(const VARIANT* value);
void WINAPI rtcSetTimeBstr(BSTR text);
VARIANT* WINAPI rtcStrConvVar2(VARIANT* result, VARIANT* operand,
                               LONG conversion, LCID locale);
SAFEARRAY* __cdecl __vbaRedim(ULONG features, LONG element_size,
                              SAFEARRAY** slot, const void* info,
                              LONG dimensions, ...);
SAFEARRAY* __cdecl __vbaRedimPreserve(ULONG features, LONG element_size,
                                      SAFEARRAY** slot, const void* info,
                                      LONG dimensions, ...);
VARIANT* __cdecl __vbaRedimVar(LONG type, VARIANT* holder, LONG dimensions,
                               ...);
VARIANT* __cdecl __vbaRedimPreserveVar(LONG type, VARIANT* holder,
                                       LONG dimensions, ...);
VARIANT* __cdecl __vbaRedimVar2(LONG type, VARIANT* holder, LONG dimensions,
                                const void* info, ...);
VARIANT* __cdecl __vbaRedimPreserveVar2(LONG type, VARIANT* holder,
                                        LONG dimensions, const void* info,
                                        ...);
VARIANT* __cdecl __vbaVarIndexLoad(VARIANT* result, const VARIANT* holder,
                                   LONG count, ...);
VARIANT* __cdecl __vbaVarIndexStore(VARIANT* holder, LONG count, ...);
SHORT WINAPI rtcMsgBox(const VARIANT* prompt, LONG buttons,
                       const VARIANT* title, const VARIANT* help_file,
                       const VARIANT* context);
BSTR WINAPI rtcInputBox(const VARIANT* prompt, const VARIANT* title,
                        const VARIANT* initial, const VARIANT* x,
                        const VARIANT* y, const VARIANT* help_file,
                        const VARIANT* context);
void WINAPI rtcAppActivate(const VARIANT* title, const VARIANT* wait);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

VARIANT absent() {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_ERROR;
    value.scode = DISP_E_PARAMNOTFOUND;
    return value;
}

VARIANT text(const wchar_t* const characters) {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = SysAllocString(characters);
    return value;
}

bool under_wine() {
    return GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                          "wine_get_version") != nullptr;
}

// Closes the dialog of the given title after it shows (exit code 0).
struct Closer {
    const wchar_t* title;
    HANDLE thread;
};

DWORD WINAPI close_dialog(LPVOID parameter) {
    const auto* const closer = static_cast<const Closer*>(parameter);
    for (int attempt = 0; attempt < 100; ++attempt) {
        Sleep(50);
        const HWND box = FindWindowW(L"#32770", closer->title);
        if (box != nullptr) {
            PostMessageW(box, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    return 1;
}

DWORD finish(Closer& closer) {
    DWORD code = 1;
    WaitForSingleObject(closer.thread, 6000);
    GetExitCodeThread(closer.thread, &code);
    CloseHandle(closer.thread);
    return code;
}

void start(Closer& closer, const wchar_t* const title) {
    closer.title = title;
    closer.thread = CreateThread(nullptr, 0, close_dialog, &closer, 0,
                                 nullptr);
}

void test_state() {
    expect(rtcGetErl() == 0, "Erl starts at zero");
    vbrun::set_erl(120);
    expect(rtcGetErl() == 120, "Erl of the failing line");
    vbrun::set_erl(0);
    SetLastError(5);
    __vbaSetSystemError();
    expect(vbrun::last_dll_error() == 5, "LastDllError");
}

void test_strings() {
    BSTR fixed = __vbaStrFixstr(3, L"abcdef");
    expect(fixed != nullptr && SysStringLen(fixed) == 3 && fixed[2] == L'c',
           "fixed string");
    SysFreeString(fixed);
    expect(__vbaAptOffset(0) == 0, "apartment offset");
    {
        // LSet and RSet: cut or pad to the length.
        wchar_t buffer[6] = L"xxxxx";
        BSTR source = SysAllocString(L"ab");
        __vbaLsetFixstr(5, buffer, source);
        expect(std::wcsncmp(buffer, L"ab   ", 5) == 0, "LSet pads right");
        __vbaRsetFixstr(5, buffer, source);
        expect(std::wcsncmp(buffer, L"   ab", 5) == 0, "RSet pads left");
        SysFreeString(source);
        source = SysAllocString(L"abcdefg");
        __vbaLsetFixstr(5, buffer, source);
        expect(std::wcsncmp(buffer, L"abcde", 5) == 0, "LSet cuts");
        __vbaRsetFixstr(5, buffer, source);
        expect(std::wcsncmp(buffer, L"abcde", 5) == 0, "RSet cuts the end");
        SysFreeString(source);
        // Into a String variable, in place, the source freed.
        BSTR target = SysAllocString(L"12345678");
        VARIANT holder;
        VariantInit(&holder);
        holder.vt = VT_BYREF | VT_BSTR;
        holder.pbstrVal = &target;
        __vbaRsetFixstrFree(0xFFFF, reinterpret_cast<wchar_t*>(&holder),
                            SysAllocString(L"xyz"));
        expect(std::wcsncmp(target, L"     xyz", 8) == 0,
               "RSet into a String");
        __vbaLsetFixstrFree(0xFFFF, reinterpret_cast<wchar_t*>(&holder),
                            SysAllocString(L"hi"));
        expect(std::wcsncmp(target, L"hi      ", 8) == 0,
               "LSet into a String");
        SysFreeString(target);
    }
    VARIANT value = text(L"copy");
    BSTR copy = __vbaStrVarCopy(&value);
    expect(copy != value.bstrVal && std::wcscmp(copy, L"copy") == 0,
           "string of a Variant");
    SysFreeString(copy);
    VariantClear(&value);
    value.vt = VT_I4;
    value.lVal = 42;
    copy = __vbaStrVarCopy(&value);
    expect(copy != nullptr && std::wcscmp(copy, L"42") == 0,
           "number as a string");
    SysFreeString(copy);
    BSTR code = SysAllocString(L"TEXT");
    expect(rtcMacId(code) == 0x54455854, "MacID of TEXT");
    SysFreeString(code);
    code = SysAllocString(L"AB");
    expect(rtcMacId(code) == 0x41422020, "MacID pads with spaces");
    SysFreeString(code);
    expect(rtcMacId(nullptr) == 0, "MacID of nothing");

    VARIANT source = text(L"Abc");
    VARIANT converted;
    rtcStrConvVar2(&converted, &source, 1, 0x409);
    expect(converted.vt == VT_BSTR &&
               std::wcscmp(converted.bstrVal, L"ABC") == 0,
           "StrConv upper case with a locale");
    VariantClear(&converted);
    rtcStrConvVar2(&converted, &source, 128, 0x409);
    expect(converted.vt == VT_BSTR &&
               SysStringByteLen(converted.bstrVal) == 3 &&
               std::memcmp(converted.bstrVal, "Abc", 3) == 0,
           "StrConv to bytes");
    VariantClear(&converted);
    vbrun::clear_error();
    rtcStrConvVar2(&converted, &source, 65, 0x409);
    expect(vbrun::last_error() == 5, "Unicode with another conversion");
    vbrun::clear_error();
    VariantClear(&source);
}

void test_arrays() {
    SAFEARRAY* dynamic = SafeArrayCreateVector(VT_I4, 0, 3);
    expect(__vbaErase(nullptr, &dynamic) == 0 && dynamic == nullptr,
           "Erase frees a dynamic array");
    SAFEARRAY* locked = SafeArrayCreateVector(VT_I4, 0, 3);
    SafeArrayLock(locked);
    vbrun::clear_error();
    expect(__vbaErase(nullptr, &locked) == 10 && locked != nullptr &&
               vbrun::last_error() == 10,
           "Erase of a locked array is error 10");
    vbrun::clear_error();
    SafeArrayUnlock(locked);
    SafeArrayDestroy(locked);
    SAFEARRAY* kept = SafeArrayCreateVector(VT_BSTR, 0, 2);
    __vbaEraseKeepData(nullptr, &kept);
    expect(kept != nullptr && (kept->fFeatures & FADF_STATIC) == 0,
           "Erase keeps the descriptor of a fixed-size array");
    SafeArrayDestroyDescriptor(kept);

    SAFEARRAY* declaration = SafeArrayCreateVector(VT_I4, 0, 5);
    SAFEARRAY built = {};
    __vbaAryConstruct(&built, declaration);
    expect(built.cDims == 1 && built.rgsabound[0].cElements == 5 &&
               built.pvData != nullptr && built.pvData != declaration->pvData,
           "array built from a declaration");
    SafeArrayDestroyData(&built);
    SafeArrayDestroy(declaration);
}

void test_redim() {
    const ULONG features = FADF_HAVEVARTYPE;
    const void* const long_type =
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(VT_I4));
    SAFEARRAY* array = nullptr;
    __vbaRedim(features, 4, &array, long_type, 1, 3, 1);
    expect(array != nullptr && array->cDims == 1 &&
               array->rgsabound[0].cElements == 3 &&
               array->rgsabound[0].lLbound == 1 && array->pvData != nullptr,
           "ReDim creates the array");
    LONG at = 2;
    LONG value = 77;
    SafeArrayPutElement(array, &at, &value);
    __vbaRedimPreserve(features, 4, &array, nullptr, 1, 5, 1);
    LONG got = 0;
    SafeArrayGetElement(array, &at, &got);
    expect(array->rgsabound[0].cElements == 5 && got == 77,
           "ReDim Preserve keeps the data");
    vbrun::clear_error();
    __vbaRedim(features, 4, &array, long_type, 1, 0, 1);
    expect(vbrun::last_error() == 9, "upper below lower is error 9");
    vbrun::clear_error();
    SafeArrayLock(array);
    __vbaRedim(features, 4, &array, nullptr, 1, 4, 1);
    expect(vbrun::last_error() == 10, "a locked array is error 10");
    vbrun::clear_error();
    SafeArrayUnlock(array);
    SafeArrayDestroy(array);

    VARIANT holder;
    VariantInit(&holder);
    __vbaRedimVar(VT_I4, &holder, 1, 2, 0);
    expect(holder.vt == (VT_ARRAY | VT_I4) && holder.parray != nullptr &&
               holder.parray->rgsabound[0].cElements == 3,
           "ReDim of a Variant");
    at = 1;
    value = 9;
    SafeArrayPutElement(holder.parray, &at, &value);
    __vbaRedimPreserveVar(VT_I4, &holder, 1, 4, 0);
    got = 0;
    SafeArrayGetElement(holder.parray, &at, &got);
    expect(holder.parray->rgsabound[0].cElements == 5 && got == 9,
           "ReDim Preserve of a Variant");
    vbrun::clear_error();
    __vbaRedimVar(VT_R8, &holder, 1, 2, 0);
    expect(vbrun::last_error() == 13, "another element type is error 13");
    vbrun::clear_error();
    VariantClear(&holder);
    __vbaRedimVar2(VT_I4, &holder, 1, nullptr, 3, 0);
    expect(holder.vt == (VT_ARRAY | VT_I4) && holder.parray != nullptr &&
               holder.parray->rgsabound[0].cElements == 4,
           "ReDim of a Variant with a record");
    at = 2;
    value = 5;
    SafeArrayPutElement(holder.parray, &at, &value);
    __vbaRedimPreserveVar2(VT_I4, &holder, 1, nullptr, 6, 0);
    got = 0;
    SafeArrayGetElement(holder.parray, &at, &got);
    expect(holder.parray->rgsabound[0].cElements == 7 && got == 5,
           "ReDim Preserve of a Variant with a record");
    VariantClear(&holder);
}

void test_index() {
    VARIANT holder;
    VariantInit(&holder);
    holder.vt = VT_ARRAY | VT_I4;
    holder.parray = SafeArrayCreateVector(VT_I4, 0, 4);
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = 7;
    VARIANT subscript;
    VariantInit(&subscript);
    subscript.vt = VT_R8;
    subscript.dblVal = 2.0;
    __vbaVarIndexStore(&holder, 1, value, subscript);
    VARIANT loaded;
    __vbaVarIndexLoad(&loaded, &holder, 1, subscript);
    expect(loaded.vt == VT_I4 && loaded.lVal == 7, "store and load");
    subscript.dblVal = 9.0;
    vbrun::clear_error();
    __vbaVarIndexLoad(&loaded, &holder, 1, subscript);
    expect(vbrun::last_error() == 9, "subscript out of range");
    vbrun::clear_error();
    __vbaVarIndexLoad(&loaded, &holder, 2, subscript, subscript);
    expect(vbrun::last_error() == 9, "wrong number of subscripts");
    vbrun::clear_error();
    VariantClear(&holder);
}

void test_loops_and_state() {
    LONGLONG counter = 0;
    int rounds = 0;
    LONG runs = __vbaCyForInit(&counter, 10000, 30000, 10000);
    while (runs != 0) {
        ++rounds;
        runs = __vbaCyForNext(&counter, 30000, 10000);
    }
    expect(rounds == 3 && counter == 40000, "Currency For loop");
    expect(__vbaCyForInit(&counter, 50000, 30000, 10000) == 0,
           "Currency loop that does not run");
    expect(__vbaCyForInit(&counter, 50000, 30000, -10000) == 1,
           "Currency loop counting down");

    rtcSetCurrentCalendar(1);
    expect(rtcGetCurrentCalendar() == 1, "Calendar set to Hijri");
    rtcSetCurrentCalendar(0);
    vbrun::clear_error();
    rtcSetCurrentCalendar(2);
    expect(vbrun::last_error() == 5 && rtcGetCurrentCalendar() == 0,
           "a bad calendar is error 5");
    vbrun::clear_error();

    IStream* stream = nullptr;
    CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    VARIANT target;
    VariantInit(&target);
    VARIANT temporary;
    VariantInit(&temporary);
    temporary.vt = VT_UNKNOWN;
    temporary.punkVal = stream;
    stream->AddRef();
    __vbaVarSetVar(&target, &temporary);
    expect(target.vt == VT_UNKNOWN && target.punkVal == stream &&
               temporary.vt == VT_EMPTY,
           "SetVar takes the object");
    VARIANT other;
    VariantInit(&other);
    other.vt = VT_UNKNOWN;
    other.punkVal = stream;
    __vbaVarSetVarAddref(&target, &other);
    expect(target.punkVal == stream && other.vt == VT_UNKNOWN,
           "SetVarAddref keeps the source");
    VariantClear(&target);
    vbrun::clear_error();
    VARIANT number;
    VariantInit(&number);
    number.vt = VT_I4;
    __vbaVarSetVar(&target, &number);
    expect(vbrun::last_error() == 13, "SetVar of a number is error 13");
    vbrun::clear_error();
    stream->Release();
}

void test_shell() {
    VARIANT command = text(L"cmd.exe /c exit 0");
    const double process = rtcShell(&command, 0);
    expect(process > 0.0 && vbrun::last_error() == 0, "Shell gives a PID");
    VariantClear(&command);
    command = text(L"wfc_no_such_program_xyz.exe");
    rtcShell(&command, 0);
    expect(vbrun::last_error() == 53, "Shell of a missing program");
    vbrun::clear_error();
    rtcShell(&command, 12);
    expect(vbrun::last_error() == 5, "Shell with a bad window style");
    vbrun::clear_error();
    VariantClear(&command);
}

void test_dialogs() {
    VARIANT prompt = text(L"Wfc dialog test");
    VARIANT caption = text(L"WfcMsgBoxTitle");
    VARIANT none = absent();
    Closer closer = {};
    start(closer, L"WfcMsgBoxTitle");
    const SHORT pressed = rtcMsgBox(&prompt, 0, &caption, &none, &none);
    const DWORD shown = finish(closer);
    expect(under_wine() || (shown == 0 && pressed == 1),
           "MsgBox closed with OK");
    VARIANT null_prompt;
    VariantInit(&null_prompt);
    null_prompt.vt = VT_NULL;
    vbrun::clear_error();
    rtcMsgBox(&null_prompt, 0, &caption, &none, &none);
    expect(vbrun::last_error() == 94, "MsgBox of Null is error 94");
    vbrun::clear_error();
    VariantClear(&caption);

    caption = text(L"WfcInputBoxTitle");
    start(closer, L"WfcInputBoxTitle");
    BSTR typed = rtcInputBox(&prompt, &caption, &none, &none, &none, &none,
                             &none);
    const DWORD input_shown = finish(closer);
    expect(under_wine() || input_shown == 0, "InputBox dialog was shown");
    expect(typed != nullptr && SysStringLen(typed) == 0,
           "InputBox cancelled is empty");
    SysFreeString(typed);
    VariantClear(&caption);
    VariantClear(&prompt);
}

void test_app_activate() {
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"WfcAppActivateTest";
    RegisterClassW(&window_class);
    const HWND window = CreateWindowW(
        L"WfcAppActivateTest", L"Wfc AppActivate Window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100, nullptr, nullptr,
        window_class.hInstance, nullptr);
    VARIANT title = text(L"wfc appactivate");
    VARIANT none = absent();
    vbrun::clear_error();
    if (window != nullptr) {
        rtcAppActivate(&title, &none);
        expect(vbrun::last_error() == 0, "AppActivate by title prefix");
    }
    VariantClear(&title);
    title = text(L"No such window wfc xyz");
    vbrun::clear_error();
    rtcAppActivate(&title, &none);
    expect(vbrun::last_error() == 5, "AppActivate of nothing is error 5");
    vbrun::clear_error();
    VariantClear(&title);
    if (window != nullptr) {
        DestroyWindow(window);
    }
}

}  // namespace

void test_clock() {
    // Setting the clock to the reading it has is harmless; without the
    // privilege it is error 70.
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t text[64] = {};
    std::swprintf(text, 64, L"%04d-%02d-%02d", now.wYear, now.wMonth,
                  now.wDay);
    BSTR date_text = SysAllocString(text);
    vbrun::clear_error();
    rtcSetDateBstr(date_text);
    const int date_error = vbrun::last_error();
    expect(date_error == 0 || date_error == 70, "Date = text");
    SysFreeString(date_text);
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = 5;
    vbrun::clear_error();
    rtcSetTimeVar(&value);
    expect(vbrun::last_error() == 13, "Time = number is error 13");
    vbrun::clear_error();
    rtcSetDateBstr(nullptr);
    expect(vbrun::last_error() == 5, "Date = nothing is error 5");
    vbrun::clear_error();
    BSTR junk = SysAllocString(L"not a time");
    rtcSetTimeBstr(junk);
    expect(vbrun::last_error() == 13, "Time = junk is error 13");
    vbrun::clear_error();
    SysFreeString(junk);
}

VARIANT number(const VARTYPE type, const double value) {
    VARIANT result;
    VariantInit(&result);
    result.vt = type;
    switch (type) {
        case VT_I2:
            result.iVal = static_cast<SHORT>(value);
            break;
        case VT_I4:
            result.lVal = static_cast<LONG>(value);
            break;
        case VT_UI1:
            result.bVal = static_cast<BYTE>(value);
            break;
        default:
            result.dblVal = value;
            break;
    }
    return result;
}

void test_index_reference() {
    // v(1) of an array of Long as a reference: the element itself.
    SAFEARRAYBOUND bound = {3, 0};
    SAFEARRAY* array = SafeArrayCreate(VT_I4, 1, &bound);
    LONG at = 1;
    LONG value = 77;
    SafeArrayPutElement(array, &at, &value);
    VARIANT holder;
    VariantInit(&holder);
    holder.vt = VT_ARRAY | VT_I4;
    holder.parray = array;
    VARIANT subscript = number(VT_I4, 1);
    VARIANT result;
    VariantInit(&result);
    VARIANT* got = __vbaVarIndexLoadRef(&result, &holder, 1, subscript);
    expect(got == &result && result.vt == (VT_BYREF | VT_I4) &&
               *result.plVal == 77,
           "element as a reference");
    *result.plVal = 5;
    LONG read = 0;
    SafeArrayGetElement(array, &at, &read);
    expect(read == 5, "the reference writes the element");
    // The Lock form locks the array.
    SAFEARRAY* locked = nullptr;
    VariantInit(&result);
    __vbaVarIndexLoadRefLock(&result, &holder, &locked, 1, subscript);
    expect(locked == array && array->cLocks == 1, "reference with a lock");
    SafeArrayUnlock(array);
    // An array of Variants gives the Variant element itself.
    SAFEARRAY* variants = SafeArrayCreate(VT_VARIANT, 1, &bound);
    VARIANT element = number(VT_I4, 9);
    SafeArrayPutElement(variants, &at, &element);
    holder.parray = variants;
    holder.vt = VT_ARRAY | VT_VARIANT;
    VariantInit(&result);
    got = __vbaVarIndexLoadRef(&result, &holder, 1, subscript);
    expect(got != &result && got->vt == VT_I4 && got->lVal == 9,
           "Variant element as a reference");
    // A subscript out of range is error 9.
    VARIANT far_away = number(VT_I4, 40);
    vbrun::clear_error();
    __vbaVarIndexLoadRef(&result, &holder, 1, far_away);
    expect(vbrun::last_error() == 9, "reference out of range is error 9");
    vbrun::clear_error();
    SafeArrayDestroy(variants);
    SafeArrayDestroy(array);
}

void test_variant_for() {
    // For i = 1 To 3 (Integers): three turns, then the counter is 4.
    VARIANT counter;
    VARIANT step;
    VARIANT limit;
    VariantInit(&counter);
    VariantInit(&step);
    VariantInit(&limit);
    VARIANT start = number(VT_I2, 1);
    VARIANT end = number(VT_I2, 3);
    VARIANT by = number(VT_I2, 1);
    int turns = 0;
    LONG go = __vbaVarForInit(&counter, &step, &limit, &start, &end, &by);
    while (go != 0 && turns < 10) {
        ++turns;
        go = __vbaVarForNext(&counter, &step, &limit);
    }
    expect(turns == 3 && counter.vt == VT_I2 && counter.iVal == 4,
           "Variant For of Integers");
    // Mixed types go to the widest: Integer start, Double step.
    VARIANT half = number(VT_R8, 0.5);
    turns = 0;
    go = __vbaVarForInit(&counter, &step, &limit, &start, &end, &half);
    while (go != 0 && turns < 20) {
        ++turns;
        go = __vbaVarForNext(&counter, &step, &limit);
    }
    expect(turns == 5 && counter.vt == VT_R8, "Variant For with a Double");
    // A negative step counts down: 3 To 1 Step -1.
    VARIANT down = number(VT_I2, -1);
    turns = 0;
    go = __vbaVarForInit(&counter, &step, &limit, &end, &start, &down);
    while (go != 0 && turns < 10) {
        ++turns;
        go = __vbaVarForNext(&counter, &step, &limit);
    }
    expect(turns == 3 && counter.iVal == 0, "Variant For counting down");
    // A loop that does not run.
    go = __vbaVarForInit(&counter, &step, &limit, &end, &start, &by);
    expect(go == 0, "Variant For that does not run");
    // A Byte counter past 255 is an overflow (error 6).
    VARIANT byte_start = number(VT_UI1, 254);
    VARIANT byte_end = number(VT_UI1, 255);
    VARIANT byte_by = number(VT_UI1, 1);
    vbrun::clear_error();
    go = __vbaVarForInit(&counter, &step, &limit, &byte_start, &byte_end,
                         &byte_by);
    expect(counter.vt == VT_UI1 && go != 0, "Variant For of Bytes");
    go = __vbaVarForNext(&counter, &step, &limit);
    expect(go != 0 && counter.bVal == 255, "Byte at the limit");
    go = __vbaVarForNext(&counter, &step, &limit);
    expect(go == 0 && vbrun::last_error() == 6, "Byte overflow is error 6");
    vbrun::clear_error();
    // Null is error 94; text that is not a number is error 13.
    VARIANT null_value;
    VariantInit(&null_value);
    null_value.vt = VT_NULL;
    __vbaVarForInit(&counter, &step, &limit, &null_value, &end, &by);
    expect(vbrun::last_error() == 94, "Variant For of Null is error 94");
    vbrun::clear_error();
    VARIANT word = text(L"abc");
    __vbaVarForInit(&counter, &step, &limit, &word, &end, &by);
    expect(vbrun::last_error() == 13, "Variant For of text is error 13");
    vbrun::clear_error();
    VariantClear(&word);
    VARIANT digits = text(L"2");
    turns = 0;
    go = __vbaVarForInit(&counter, &step, &limit, &digits, &end, &by);
    while (go != 0 && turns < 10) {
        ++turns;
        go = __vbaVarForNext(&counter, &step, &limit);
    }
    expect(turns == 2 && counter.vt == VT_R8, "Variant For of text");
    VariantClear(&digits);
    VariantClear(&counter);
}

void test_ime_and_friend() {
    // No input method is needed: the answer is one of the modes or -1.
    const LONG mode = rtcIMEStatus();
    expect(mode >= -1 && mode <= 10, "IMEStatus is a mode");
    vbrun::clear_error();
    vbrun::clear_error();
    expect(__vbaHresultCheckObj(S_OK, nullptr, nullptr, 0) == S_OK &&
               vbrun::last_error() == 0,
           "a good HRESULT of an object is no error");
    __vbaHresultCheckObj(DISP_E_TYPEMISMATCH, nullptr, nullptr, 0);
    expect(vbrun::last_error() == 13, "a failed call on an object");
    vbrun::clear_error();
    __vbaFailedFriend();
    expect(vbrun::last_error() == 97, "a failed Friend call is error 97");
    vbrun::clear_error();
}

int main() {
    test_state();
    test_strings();
    test_arrays();
    test_redim();
    test_index();
    test_loops_and_state();
    test_shell();
    test_dialogs();
    test_app_activate();
    test_variant_for();
    test_index_reference();
    test_ime_and_friend();
    test_clock();
    if (failures == 0) {
        std::puts("system more ok");
    }
    return failures == 0 ? 0 : 1;
}
