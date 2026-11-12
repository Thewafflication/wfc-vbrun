// The Mid$ statement (__vbaMidStmt*): in-place overwrite of a String, a
// fixed-length String buffer and a Variant, with the error cases.
//
// usage: vbrun_midstmt_tests

#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <cwchar>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
void WINAPI __vbaMidStmtBstr(LONG fixed, BSTR replacement, LONG length,
                             LONG start, void* target);
void WINAPI __vbaMidStmtBstrB(LONG fixed, BSTR replacement, LONG length,
                              LONG start, void* target);
void WINAPI __vbaMidStmtVar(VARIANT* replacement, LONG length, LONG start,
                            VARIANT* target);
void WINAPI __vbaMidStmtVarB(VARIANT* replacement, LONG length, LONG start,
                             VARIANT* target);
}

namespace {

constexpr LONG kNone = 0x3FFFFFFF;
int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

std::wstring after_bstr(const wchar_t* const target, const wchar_t* const repl,
                        const LONG length, const LONG start, const bool bytes,
                        int& error) {
    BSTR text = SysAllocString(target);
    BSTR replacement = SysAllocString(repl);
    vbrun::clear_error();
    if (bytes) {
        __vbaMidStmtBstrB(0, replacement, length, start, &text);
    } else {
        __vbaMidStmtBstr(0, replacement, length, start, &text);
    }
    error = vbrun::last_error();
    const std::wstring result(text == nullptr ? L"" : text, SysStringLen(text));
    SysFreeString(text);
    SysFreeString(replacement);
    return result;
}

void check(const wchar_t* const target, const wchar_t* const repl,
           const LONG length, const LONG start, const bool bytes,
           const wchar_t* const want, const int want_error,
           const char* const what) {
    int error = 0;
    const std::wstring got =
        after_bstr(target, repl, length, start, bytes, error);
    expect(got == want && error == want_error, what);
}

}  // namespace

int main() {
    check(L"hello world", L"XY", kNone, 1, false, L"XYllo world", 0, "head");
    check(L"hello world", L"XYZ", 2, 7, false, L"hello XYrld", 0, "length cap");
    check(L"hello", L"abcdefgh", kNone, 4, false, L"helab", 0, "target cap");
    check(L"hello", L"X", kNone, 5, false, L"hellX", 0, "last char");
    check(L"hello", L"", kNone, 2, false, L"hello", 0, "empty replacement");
    check(L"hello", L"XY", 0, 2, false, L"hello", 0, "zero length");
    check(L"hello", L"X", kNone, 6, false, L"hello", 5, "start past end");
    check(L"hello", L"X", kNone, 0, false, L"hello", 5, "start zero");
    check(L"hello", L"X", -1, 1, false, L"hello", 5, "negative length");
    check(L"", L"X", kNone, 1, false, L"", 5, "empty target");
    // Bytes: the offsets and lengths count bytes.
    check(L"abcd", L"WX", 2, 3, true, L"aWcd", 0, "byte form");
    check(L"abcd", L"WX", kNone, 3, true, L"aWXd", 0, "byte whole");

    // A null target String.
    {
        BSTR text = nullptr;
        BSTR replacement = SysAllocString(L"X");
        vbrun::clear_error();
        __vbaMidStmtBstr(0, replacement, kNone, 1, &text);
        expect(vbrun::last_error() == 5, "null target");
        SysFreeString(replacement);
    }
    // A fixed-length buffer of 5 characters.
    {
        wchar_t buffer[5] = {L'a', L'b', L'c', L'd', L'e'};
        BSTR replacement = SysAllocString(L"12345678");
        vbrun::clear_error();
        __vbaMidStmtBstr(5, replacement, kNone, 3, buffer);
        expect(vbrun::last_error() == 0 && std::wstring(buffer, 5) == L"ab123",
               "fixed buffer");
        SysFreeString(replacement);
    }
    // A Variant holding a String, and one holding a number.
    {
        VARIANT target;
        VARIANT replacement;
        VariantInit(&target);
        VariantInit(&replacement);
        target.vt = VT_BSTR;
        target.bstrVal = SysAllocString(L"12345");
        replacement.vt = VT_I4;
        replacement.lVal = 99;
        vbrun::clear_error();
        __vbaMidStmtVar(&replacement, kNone, 2, &target);
        expect(vbrun::last_error() == 0 && target.vt == VT_BSTR &&
                   std::wstring(target.bstrVal) == L"19945",
               "variant string");
        VariantClear(&target);
        target.vt = VT_I4;
        target.lVal = 12345;
        replacement.vt = VT_BSTR;
        replacement.bstrVal = SysAllocString(L"ab");
        __vbaMidStmtVar(&replacement, kNone, 4, &target);
        expect(target.vt == VT_BSTR && std::wstring(target.bstrVal) == L"123ab",
               "variant converted");
        VariantClear(&target);
        VariantClear(&replacement);
        // A byref String, and a byref of another type.
        BSTR text = SysAllocString(L"hello");
        VARIANT ref;
        VariantInit(&ref);
        ref.vt = VT_BYREF | VT_BSTR;
        ref.pbstrVal = &text;
        replacement.vt = VT_BSTR;
        replacement.bstrVal = SysAllocString(L"J");
        __vbaMidStmtVarB(&replacement, kNone, 1, &ref);
        expect(std::wstring(text) == L"Jello", "variant byref bytes");
        LONG number = 3;
        ref.vt = VT_BYREF | VT_I4;
        ref.plVal = &number;
        vbrun::clear_error();
        __vbaMidStmtVar(&replacement, kNone, 1, &ref);
        expect(vbrun::last_error() == 13, "variant byref mismatch");
        SysFreeString(text);
        VariantClear(&replacement);
    }
    {
        VARIANT target;
        VARIANT replacement;
        VariantInit(&target);
        VariantInit(&replacement);
        replacement.vt = VT_BSTR;
        replacement.bstrVal = SysAllocString(L"ab");
        vbrun::clear_error();
        __vbaMidStmtVar(&replacement, kNone, 1, &target);
        expect(vbrun::last_error() == 5, "variant empty target");
        target.vt = VT_NULL;
        vbrun::clear_error();
        __vbaMidStmtVar(&replacement, kNone, 1, &target);
        expect(vbrun::last_error() == 94, "variant null target");
        VariantClear(&replacement);
        replacement.vt = VT_NULL;
        target.vt = VT_BSTR;
        target.bstrVal = SysAllocString(L"abc");
        vbrun::clear_error();
        __vbaMidStmtVar(&replacement, kNone, 1, &target);
        expect(
            vbrun::last_error() == 94 && std::wstring(target.bstrVal) == L"abc",
            "variant null replacement");
        VariantClear(&target);
    }
    if (failures == 0) {
        std::printf("midstmt ok\n");
    }
    return failures == 0 ? 0 : 1;
}
