// StrComp on Variants, Command$, Chr and the small host exports, with the
// results measured from a Visual Basic 6 program (experiment cmdLine).
//
// usage: vbrun_system_misc_tests

#include <windows.h>

#include <oleauto.h>

#include <cstdio>
#include <string>

#include "../src/vbrun_helpers.hpp"

extern "C" {
VARIANT* WINAPI __vbaStrCompVar(VARIANT* result, LONG mode, VARIANT* right,
                                VARIANT* left);
VARIANT* WINAPI rtcCompareBstr(VARIANT* result, VARIANT* left, VARIANT* right,
                               LONG mode);
VARIANT* WINAPI __vbaInStrVar(VARIANT* result, LONG mode, VARIANT* needle,
                              VARIANT* haystack, LONG start);
VARIANT* WINAPI __vbaInStrVarB(VARIANT* result, LONG mode, VARIANT* needle,
                               VARIANT* haystack, LONG start);
VARIANT* WINAPI __vbaVarLikeVar(VARIANT* result, const VARIANT* pattern,
                                const VARIANT* text);
VARIANT* WINAPI __vbaVarTextLikeVar(VARIANT* result, const VARIANT* pattern,
                                    const VARIANT* text);
SHORT WINAPI __vbaBoolStr(BSTR text);
VARIANT* WINAPI rtcArray(VARIANT* result, SAFEARRAY** arguments);
float WINAPI __vbaR4Str(BSTR text);
SHORT WINAPI __vbaR4Sgn(float value);
SHORT WINAPI __vbaR8Sgn(double value);
LONG WINAPI rtDecFromVar(VARIANT* source, DECIMAL* target);
double WINAPI __vbaPowerR8(double base, double exponent);
void* WINAPI __vbaCopyBytes(LONG count, void* target, const void* source);
void* WINAPI __vbaCopyBytesZero(LONG count, void* target, void* source);
void WINAPI __vbaFixstrConstruct(LONG count, BSTR* slot);
LONG WINAPI __vbaObjIs(IUnknown* left, IUnknown* right);
IUnknown* WINAPI __vbaCastObj(IUnknown* object, const IID* iid);
BSTR WINAPI __vbaStrVarMove(VARIANT* value);
SAFEARRAY* WINAPI __vbaAryLock(SAFEARRAY** slot, SAFEARRAY* array);
void WINAPI __vbaAryUnlock(SAFEARRAY** slot);
VARIANT* WINAPI __vbaVarErrI4(VARIANT* result, LONG number);
void __cdecl __vbaFreeStrList(LONG count, ...);
void __cdecl __vbaFreeObjList(LONG count, ...);
void __cdecl __vbaFreeVarList(LONG count, ...);
SHORT WINAPI __vbaBoolVarNull(const VARIANT* value);
VARIANT* WINAPI __vbaVarSetUnk(VARIANT* destination, IUnknown* source);
VARIANT* WINAPI __vbaVarSetUnkAddref(VARIANT* destination, IUnknown* source);
VARIANT* WINAPI __vbaVarSetObj(VARIANT* destination, IUnknown* source);
VARIANT* WINAPI __vbaVarSetObjAddref(VARIANT* destination, IUnknown* source);
BSTR WINAPI __vbaStrVarVal(BSTR* slot, const VARIANT* value);
LONG WINAPI __vbaLbound(SHORT dimension, const SAFEARRAY* array);
LONG WINAPI __vbaUbound(SHORT dimension, const SAFEARRAY* array);
IUnknown* WINAPI __vbaCastObjVar(VARIANT* value, const IID* iid);
HRESULT WINAPI __vbaHresultCheck(HRESULT result);
BSTR WINAPI __vbaStrToAnsi(BSTR* destination, BSTR source);
BSTR WINAPI __vbaStrToUnicode(BSTR* destination, BSTR source);
IUnknown* WINAPI __vbaObjSet(IUnknown** destination, IUnknown* source);
IUnknown* WINAPI __vbaObjSetAddref(IUnknown** destination, IUnknown* source);
IUnknown* WINAPI __vbaObjAddref(IUnknown* object);
VARIANT* WINAPI __vbaVerifyVarObj(VARIANT* value);
IUnknown* WINAPI __vbaObjVar(VARIANT* value);
IUnknown* WINAPI __vbaUnkVar(VARIANT* value);
LONG WINAPI __vbaI2ForNextCheck(SHORT counter, SHORT limit, SHORT step);
LONG WINAPI __vbaI4ForNextCheck(LONG counter, LONG limit, LONG step);
LONG WINAPI __vbaR4ForNextCheck(float counter, float limit, float step);
LONG WINAPI __vbaR8ForNextCheck(double counter, double limit,
                                double step);
SHORT WINAPI __vbaOnGoCheck(SHORT index);
BSTR WINAPI rtcStringBstr(LONG count, VARIANT* character);
BSTR WINAPI rtcCommandBstr();
VARIANT* WINAPI rtcCommandVar(VARIANT* result);
SHORT WINAPI rtcDoEvents();
BSTR WINAPI rtcBstrFromAnsi(LONG code);
VARIANT* WINAPI rtcVarBstrFromAnsi(VARIANT* result, LONG code);
VARIANT* WINAPI rtcVarBstrFromByte(VARIANT* result, LONG code);
}

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

VARIANT text_variant(const wchar_t* const text) {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = SysAllocString(text);
    return value;
}

// StrComp(left, right, mode) as "type:value" ("I2:-1", "Null", "err:5").
std::string compare(VARIANT left, VARIANT right, const LONG mode) {
    VARIANT result;
    vbrun::clear_error();
    __vbaStrCompVar(&result, mode, &right, &left);
    std::string text;
    if (vbrun::last_error() != 0) {
        text = "err:" + std::to_string(vbrun::last_error());
    } else if (result.vt == VT_NULL) {
        text = "Null";
    } else {
        text = "I2:" + std::to_string(result.iVal);
    }
    VariantClear(&result);
    VariantClear(&left);
    VariantClear(&right);
    return text;
}

VARIANT number_variant(const LONG value) {
    VARIANT result;
    VariantInit(&result);
    result.vt = VT_I4;
    result.lVal = value;
    return result;
}

VARIANT null_variant() {
    VARIANT result;
    VariantInit(&result);
    result.vt = VT_NULL;
    return result;
}

}  // namespace

int main() {
    {
        VARIANT moved = text_variant(L"mv");
        BSTR taken = __vbaStrVarMove(&moved);
        expect(moved.vt == VT_EMPTY && std::wstring(taken) == L"mv",
               "str var move");
        SysFreeString(taken);
        VARIANT number;
        VariantInit(&number);
        number.vt = VT_I4;
        number.lVal = 12;
        taken = __vbaStrVarMove(&number);
        expect(std::wstring(taken) == L"12" && number.vt == VT_EMPTY,
               "str var move converts");
        SysFreeString(taken);
        SAFEARRAYBOUND one = {2, 0};
        SAFEARRAY* array = SafeArrayCreate(VT_I4, 1, &one);
        SAFEARRAY* slot = nullptr;
        expect(__vbaAryLock(&slot, array) == array && slot == array &&
                   array->cLocks == 1,
               "array lock");
        __vbaAryUnlock(&slot);
        expect(slot == nullptr && array->cLocks == 0, "array unlock");
        SafeArrayDestroy(array);
        VARIANT error;
        VariantInit(&error);
        __vbaVarErrI4(&error, 11);
        expect(error.vt == VT_ERROR &&
                   static_cast<ULONG>(error.scode) == 0x800A000BUL,
               "cverr");
        __vbaVarErrI4(&error, 0);
        expect(error.scode == 0, "cverr zero");
    }
    {
        BSTR one = SysAllocString(L"a");
        BSTR two = SysAllocString(L"b");
        __vbaFreeStrList(2, &one, &two);
        expect(one == nullptr && two == nullptr, "free str list");
        VARIANT va = text_variant(L"x");
        VARIANT vb = text_variant(L"y");
        __vbaFreeVarList(2, &va, &vb);
        expect(va.vt == VT_EMPTY && vb.vt == VT_EMPTY, "free var list");
        IStream* stream = nullptr;
        CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        stream->AddRef();
        IUnknown* held = stream;
        __vbaFreeObjList(1, &held);
        expect(held == nullptr, "free obj list");
        VARIANT holder;
        VariantInit(&holder);
        __vbaVarSetUnkAddref(&holder, stream);
        expect(holder.vt == VT_UNKNOWN && holder.punkVal == stream,
               "var set unk");
        __vbaVarSetObj(&holder, nullptr);
        expect(holder.vt == VT_DISPATCH && holder.pdispVal == nullptr,
               "var set obj clears the old one");
        stream->Release();
        VARIANT nul = null_variant();
        VARIANT truth;
        VariantInit(&truth);
        truth.vt = VT_I4;
        truth.lVal = 5;
        expect(__vbaBoolVarNull(&nul) == 0 && __vbaBoolVarNull(&truth) == -1,
               "bool of null");
        BSTR slot = nullptr;
        VARIANT text = text_variant(L"hi");
        expect(__vbaStrVarVal(&slot, &text) == text.bstrVal && slot == nullptr,
               "str var val borrows");
        BSTR converted = __vbaStrVarVal(&slot, &truth);
        expect(converted != nullptr && std::wstring(converted) == L"5" &&
                   slot == converted,
               "str var val converts");
        SysFreeString(slot);
        VariantClear(&text);
    }
    {
        SAFEARRAYBOUND bounds[2] = {{4, 1}, {3, 5}};
        SAFEARRAY* grid = SafeArrayCreate(VT_I4, 2, bounds);
        expect(__vbaLbound(2, grid) == 5 && __vbaUbound(2, grid) == 7,
               "bounds of dimension 1");
        expect(__vbaLbound(1, grid) == 1 && __vbaUbound(1, grid) == 4,
               "bounds of dimension 2");
        vbrun::clear_error();
        expect(__vbaUbound(3, grid) == 0 && vbrun::last_error() == 9,
               "bound out of range");
        vbrun::clear_error();
        expect(__vbaLbound(1, nullptr) == 0 && vbrun::last_error() == 9,
               "bound of nothing");
        vbrun::clear_error();
        SafeArrayDestroy(grid);
    }
    {
        expect(__vbaCastObj(nullptr, &IID_IUnknown) == nullptr, "cast nothing");
        IStream* stream = nullptr;
        CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        IUnknown* same = __vbaCastObj(stream, &IID_IUnknown);
        expect(same != nullptr, "cast to unknown");
        if (same != nullptr) {
            same->Release();
        }
        vbrun::clear_error();
        expect(__vbaCastObj(stream, &IID_IDispatch) == nullptr &&
                   vbrun::last_error() == 13,
               "cast without interface");
        VARIANT holder;
        VariantInit(&holder);
        holder.vt = VT_UNKNOWN;
        holder.punkVal = stream;
        IUnknown* again = __vbaCastObjVar(&holder, &IID_IUnknown);
        expect(again != nullptr, "cast variant");
        if (again != nullptr) {
            again->Release();
        }
        holder.vt = VT_I4;
        expect(__vbaCastObjVar(&holder, &IID_IUnknown) == nullptr &&
                   vbrun::last_error() == 424,
               "cast variant not object");
        vbrun::clear_error();
        expect(__vbaHresultCheck(S_OK) == S_OK && vbrun::last_error() == 0,
               "hresult ok");
        static_cast<void>(__vbaHresultCheck(E_OUTOFMEMORY));
        expect(vbrun::last_error() == 7, "hresult out of memory");
        vbrun::clear_error();
        vbrun::clear_error();
        stream->Release();
    }
    {
        VARIANT subject = text_variant(L"Hello");
        VARIANT pattern = text_variant(L"h*o");
        VARIANT out;
        __vbaVarLikeVar(&out, &pattern, &subject);
        expect(out.vt == VT_BOOL && out.boolVal == VARIANT_FALSE, "like case");
        __vbaVarTextLikeVar(&out, &pattern, &subject);
        expect(out.vt == VT_BOOL && out.boolVal == VARIANT_TRUE, "like text");
        VARIANT nul = null_variant();
        __vbaVarLikeVar(&out, &pattern, &nul);
        expect(out.vt == VT_NULL, "like null");
    }
    {
        VARIANT ch = text_variant(L"xy");
        BSTR made = rtcStringBstr(3, &ch);
        expect(made != nullptr && std::wstring(made) == L"xxx", "string$ text");
        SysFreeString(made);
        VARIANT code;
        VariantInit(&code);
        code.vt = VT_I4;
        code.lVal = 65;
        made = rtcStringBstr(2, &code);
        expect(made != nullptr && std::wstring(made) == L"AA", "string$ code");
        SysFreeString(made);
        made = rtcStringBstr(-1, &code);
        expect(made == nullptr, "string$ negative");
    }
    {
        VARIANT hay = text_variant(L"Hello World");
        VARIANT pin = text_variant(L"WORLD");
        VARIANT out;
        __vbaInStrVar(&out, 1, &pin, &hay, 1);
        expect(out.vt == VT_I4 && out.lVal == 7, "instr variant text");
        __vbaInStrVar(&out, 0, &pin, &hay, 1);
        expect(out.vt == VT_I4 && out.lVal == 0, "instr variant binary");
        __vbaInStrVarB(&out, 0, &pin, &hay, 1);
        expect(out.vt == VT_I4 && out.lVal == 0, "instr variant bytes");
        VARIANT nul = null_variant();
        __vbaInStrVar(&out, 0, &pin, &nul, 1);
        expect(out.vt == VT_NULL, "instr variant null");
        VARIANT num;
        VariantInit(&num);
        num.vt = VT_I4;
        num.lVal = 12345;
        VARIANT three = text_variant(L"34");
        __vbaInStrVar(&out, 0, &three, &num, 1);
        expect(out.vt == VT_I4 && out.lVal == 3, "instr variant number");
    }
    expect(compare(text_variant(L"abc"), text_variant(L"ABC"), 0) == "I2:1",
           "binary");
    expect(compare(text_variant(L"abc"), text_variant(L"ABC"), 1) == "I2:0",
           "text");
    expect(compare(text_variant(L"ABC"), text_variant(L"abc"), 0) == "I2:-1",
           "reversed");
    expect(compare(text_variant(L"abc"), text_variant(L"ABC"), 2) == "err:5",
           "database mode");
    expect(compare(null_variant(), text_variant(L"x"), 0) == "Null",
           "null left");
    expect(compare(text_variant(L"x"), null_variant(), 0) == "Null",
           "null right");
    VARIANT empty;
    VariantInit(&empty);
    expect(compare(empty, text_variant(L""), 0) == "I2:0", "empty");
    expect(compare(number_variant(12), text_variant(L"12"), 0) == "I2:0",
           "number");
    expect(compare(number_variant(2), number_variant(10), 0) == "I2:1",
           "numbers as text");
    expect(compare(text_variant(L"a"), text_variant(L"b"), 0) == "I2:-1", "lt");
    expect(compare(text_variant(L"b"), text_variant(L"a"), 0) == "I2:1", "gt");
    {
        VARIANT result;
        VARIANT left = text_variant(L"a");
        VARIANT right = text_variant(L"b");
        __vbaStrCompVar(&result, 0, &right, &left);
        VARIANT again;
        rtcCompareBstr(&again, &left, &right, 0);
        expect(result.iVal == -1 && again.iVal == -1, "compare bstr order");
        VariantClear(&left);
        VariantClear(&right);
    }
    // Command$ is the text after the program name.
    {
        BSTR command = rtcCommandBstr();
        expect(command != nullptr && SysStringLen(command) == 0,
               "command line is empty");
        SysFreeString(command);
        VARIANT value;
        rtcCommandVar(&value);
        expect(value.vt == VT_BSTR, "command variant");
        VariantClear(&value);
    }
    expect(rtcDoEvents() == 0, "DoEvents");
    {
        BSTR one = rtcBstrFromAnsi(65);
        expect(one != nullptr && SysStringLen(one) == 1 && one[0] == L'A',
               "ansi 65");
        SysFreeString(one);
        vbrun::clear_error();
        expect(rtcBstrFromAnsi(300) == nullptr && vbrun::last_error() == 5,
               "ansi 300");
        VARIANT value;
        rtcVarBstrFromByte(&value, 200);
        expect(value.vt == VT_BSTR && SysStringByteLen(value.bstrVal) == 1 &&
                   reinterpret_cast<unsigned char*>(value.bstrVal)[0] == 200,
               "byte string");
        VariantClear(&value);
        rtcVarBstrFromAnsi(&value, 66);
        expect(value.vt == VT_BSTR && value.bstrVal[0] == L'B', "variant ansi");
        VariantClear(&value);
    }
    if (failures == 0) {
        std::printf("system misc ok\n");
    }
    return failures == 0 ? 0 : 1;
}
