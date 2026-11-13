// SaveSetting, GetSetting, GetAllSettings and DeleteSetting on a registry
// key of the test, removed at the end.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <cwchar>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void WINAPI rtcSaveSetting(BSTR app, BSTR section, BSTR key, BSTR value);
BSTR WINAPI rtcGetSetting(BSTR app, BSTR section, BSTR key, VARIANT fallback);
void WINAPI rtcDeleteSetting(BSTR app, VARIANT section, VARIANT key);
VARIANT* WINAPI rtcGetAllSettings(VARIANT* result, BSTR app, BSTR section);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

VARIANT missing() {
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

bool same(BSTR value, const wchar_t* const expected) {
    const bool ok = value != nullptr && std::wcscmp(value, expected) == 0;
    SysFreeString(value);
    return ok;
}

}  // namespace

int main() {
    const std::wstring name =
        L"WfcVbrunSettingsTest" + std::to_wstring(GetCurrentProcessId());
    BSTR app = SysAllocString(name.c_str());
    BSTR section = SysAllocString(L"Window");
    BSTR other = SysAllocString(L"Colors");
    BSTR left = SysAllocString(L"Left");
    BSTR top = SysAllocString(L"Top");
    BSTR value = SysAllocString(L"120");
    BSTR empty = SysAllocString(L"");

    rtcSaveSetting(app, section, left, value);
    expect(same(rtcGetSetting(app, section, left, missing()), L"120"),
           "saved setting");
    SysFreeString(value);
    value = SysAllocString(L"85");
    rtcSaveSetting(app, section, top, value);
    rtcSaveSetting(app, other, left, value);
    expect(same(rtcGetSetting(app, section, top, missing()), L"85"),
           "second setting");
    expect(same(rtcGetSetting(app, section, SysAllocString(L"Nope"), missing()),
                L""),
           "missing setting is empty");
    expect(same(rtcGetSetting(app, section, SysAllocString(L"Nope"),
                              text(L"none")),
                L"none"),
           "default of a missing setting");
    VARIANT number;
    VariantInit(&number);
    number.vt = VT_I4;
    number.lVal = 7;
    expect(same(rtcGetSetting(app, SysAllocString(L"Absent"),
                              SysAllocString(L"Nope"), number),
                L"7"),
           "number as the default");

    vbrun::clear_error();
    rtcSaveSetting(empty, section, left, value);
    expect(vbrun::last_error() == 5, "empty application name is error 5");
    vbrun::clear_error();

    VARIANT rows;
    VariantInit(&rows);
    rtcGetAllSettings(&rows, app, section);
    expect(rows.vt == (VT_ARRAY | VT_BSTR) &&
               SafeArrayGetDim(rows.parray) == 2,
           "all settings is a two dimensional array");
    bool has_left = false;
    bool has_top = false;
    if (rows.vt == (VT_ARRAY | VT_BSTR)) {
        LONG count = 0;
        SafeArrayGetUBound(rows.parray, 1, &count);
        expect(count == 1, "two settings");
        for (LONG row = 0; row <= count; ++row) {
            LONG at[2] = {row, 0};
            BSTR key = nullptr;
            BSTR data = nullptr;
            SafeArrayGetElement(rows.parray, at, &key);
            at[1] = 1;
            SafeArrayGetElement(rows.parray, at, &data);
            has_left = has_left || (std::wcscmp(key, L"Left") == 0 &&
                                    std::wcscmp(data, L"120") == 0);
            has_top = has_top || (std::wcscmp(key, L"Top") == 0 &&
                                  std::wcscmp(data, L"85") == 0);
            SysFreeString(key);
            SysFreeString(data);
        }
    }
    expect(has_left && has_top, "keys and values");
    VariantClear(&rows);
    VARIANT none;
    VariantInit(&none);
    rtcGetAllSettings(&none, app, SysAllocString(L"Nothing"));
    expect(none.vt == VT_EMPTY, "no section is Empty");

    rtcDeleteSetting(app, text(L"Window"), text(L"Left"));
    expect(same(rtcGetSetting(app, section, left, missing()), L""),
           "deleted setting");
    expect(same(rtcGetSetting(app, section, top, missing()), L"85"),
           "other setting stays");
    vbrun::clear_error();
    rtcDeleteSetting(app, text(L"Window"), text(L"Left"));
    expect(vbrun::last_error() == 5, "deleting a missing setting is error 5");
    vbrun::clear_error();
    rtcDeleteSetting(app, text(L"Window"), missing());
    expect(same(rtcGetSetting(app, section, top, missing()), L""),
           "deleted section");
    expect(same(rtcGetSetting(app, other, left, missing()), L"85"),
           "other section stays");
    rtcDeleteSetting(app, missing(), missing());
    expect(same(rtcGetSetting(app, other, left, missing()), L""),
           "deleted application");
    expect(vbrun::last_error() == 0, "no error");

    if (failures == 0) {
        std::puts("settings ok");
    }
    return failures == 0 ? 0 : 1;
}
