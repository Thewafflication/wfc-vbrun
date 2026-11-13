// Late binding: the calls a Visual Basic program makes on an object of type
// Object (`obj.Name(args)`, `x = obj.Name`, `obj.Name = value`,
// `Set obj.Name = other`), by member name (Mem) or by dispatch id (Id).
//
// Written from the disassembly of MSVBVM60: every helper is cdecl with the
// arguments as VARIANTs by value after the fixed ones, in the order of the
// Invoke array. The Ld forms take the result Variant first and return it; a
// property assignment passes the value first in the array (it is the named
// argument DISPID_PROPERTYPUT). A failed call is the Visual Basic error of the
// failure.

#include <cstdarg>
#include <cstring>
#include <vector>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kObjectRequired = 424;
constexpr int kNoObject = 91;
constexpr int kNoMember = 438;

LCID language_locale() {
    const LCID user = GetUserDefaultLCID();
    return user != 0 ? user : 0x409;
}

// The error of a failed late call. A member that is not there is 438; an
// exception of the object carries its own code.
void raise_late(const HRESULT result, EXCEPINFO& excep) {
    HRESULT code = result;
    if (result == DISP_E_EXCEPTION) {
        code = excep.scode != 0 ? excep.scode : result;
    } else if (result == DISP_E_MEMBERNOTFOUND ||
               result == DISP_E_UNKNOWNNAME) {
        static_cast<void>(vbrun::raise(kNoMember));
        code = S_OK;
    }
    if (code != S_OK) {
        static_cast<void>(vbrun::raise_hresult(code));
    }
    SysFreeString(excep.bstrSource);
    SysFreeString(excep.bstrDescription);
    SysFreeString(excep.bstrHelpFile);
}

bool dispatch_id(IDispatch* const object, const BSTR name, DISPID& id) {
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kNoObject));
        return false;
    }
    OLECHAR* names[1] = {name};
    const HRESULT result = object->GetIDsOfNames(
        IID_NULL, names, 1, language_locale(), &id);
    if (FAILED(result)) {
        EXCEPINFO none = {};
        raise_late(result, none);
        return false;
    }
    return true;
}

// The invoke every late call ends in.
VARIANT* invoke(IDispatch* const object, const DISPID id, const WORD flags,
                VARIANT* const arguments, const unsigned count,
                DISPID* const named, const unsigned named_count,
                VARIANT* const result) {
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kNoObject));
        return result;
    }
    DISPPARAMS parameters = {};
    parameters.rgvarg = count != 0 ? arguments : nullptr;
    parameters.cArgs = count;
    parameters.rgdispidNamedArgs = named_count != 0 ? named : nullptr;
    parameters.cNamedArgs = named_count;
    EXCEPINFO excep = {};
    UINT bad_argument = 0;
    const HRESULT status =
        object->Invoke(id, IID_NULL, language_locale(), flags, &parameters,
                       result, &excep, &bad_argument);
    if (FAILED(status)) {
        raise_late(status, excep);
    }
    return result;
}

std::vector<VARIANT> take_variants(va_list& list, const unsigned count) {
    std::vector<VARIANT> values;
    values.reserve(count);
    for (unsigned index = 0; index < count; ++index) {
        values.push_back(va_arg(list, VARIANT));
    }
    return values;
}

// The value of `obj.Name = value` that is an object reference needs an
// object (error 424 otherwise).
bool put_value_ok(const WORD flags, const VARIANT& value) {
    if (flags != DISPATCH_PROPERTYPUTREF) {
        return true;
    }
    const unsigned type = value.vt & ~static_cast<unsigned>(VT_BYREF);
    if (type == VT_DISPATCH || type == VT_UNKNOWN) {
        return true;
    }
    static_cast<void>(vbrun::raise(kObjectRequired));
    return false;
}

// Property assignment: `values` holds the new value first, then the
// subscripts.
void put(IDispatch* const object, const DISPID id, const WORD flags,
         std::vector<VARIANT>& values) {
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kNoObject));
        return;
    }
    if (!put_value_ok(flags, values[0])) {
        return;
    }
    DISPID named = DISPID_PROPERTYPUT;
    invoke(object, id, flags, values.data(),
           static_cast<unsigned>(values.size()), &named, 1, nullptr);
}

// Named arguments: the member name first, then the argument names. The
// first of the arguments belong to the names, in order.
VARIANT* invoke_named(IDispatch* const object, const BSTR member,
                      const std::vector<BSTR>& argument_names,
                      const WORD flags, std::vector<VARIANT>& values,
                      VARIANT* const result) {
    if (object == nullptr) {
        static_cast<void>(vbrun::raise(kNoObject));
        return result;
    }
    std::vector<OLECHAR*> names;
    names.push_back(member);
    for (BSTR name : argument_names) {
        names.push_back(name);
    }
    std::vector<DISPID> ids(names.size(), DISPID_UNKNOWN);
    HRESULT status = object->GetIDsOfNames(
        IID_NULL, names.data(), static_cast<UINT>(names.size()),
        language_locale(), ids.data());
    if (status == DISP_E_UNKNOWNNAME && ids[0] != DISPID_UNKNOWN) {
        status = DISP_E_PARAMNOTFOUND;
    }
    if (FAILED(status)) {
        EXCEPINFO none = {};
        raise_late(status, none);
        return result;
    }
    return invoke(object, ids[0], flags, values.data(),
                  static_cast<unsigned>(values.size()), ids.data() + 1,
                  static_cast<unsigned>(argument_names.size()), result);
}

}  // namespace

extern "C" {

// obj.Name(args) as a statement.
void __cdecl __vbaLateMemCall(IDispatch* const object, const BSTR name,
                              const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    DISPID id = 0;
    if (dispatch_id(object, name, id)) {
        invoke(object, id, DISPATCH_METHOD, values.data(), count, nullptr, 0,
               nullptr);
    }
}

// x = obj.Name(args).
VARIANT* __cdecl __vbaLateMemCallLd(VARIANT* const result,
                                    IDispatch* const object, const BSTR name,
                                    const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    DISPID id = 0;
    if (dispatch_id(object, name, id)) {
        invoke(object, id, DISPATCH_METHOD | DISPATCH_PROPERTYGET,
               values.data(), count, nullptr, 0, result);
    }
    return result;
}

// obj.Name(args) = value: the value comes first among the count + 1 values.
void __cdecl __vbaLateMemCallSt(IDispatch* const object, const BSTR name,
                                const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count + 1);
    va_end(list);
    DISPID id = 0;
    if (dispatch_id(object, name, id)) {
        put(object, id, DISPATCH_PROPERTYPUT, values);
    }
}

// Set obj.Name(args) = value.
void __cdecl __vbaLateMemStAd(IDispatch* const object, const BSTR name,
                              const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count + 1);
    va_end(list);
    DISPID id = 0;
    if (dispatch_id(object, name, id)) {
        put(object, id, DISPATCH_PROPERTYPUTREF, values);
    }
}

// obj.Name = value.
void WINAPI __vbaLateMemSt(IDispatch* const object, const BSTR name,
                           VARIANT value) {
    DISPID id = 0;
    if (dispatch_id(object, name, id)) {
        std::vector<VARIANT> values(1, value);
        put(object, id, DISPATCH_PROPERTYPUT, values);
    }
}

void __cdecl __vbaLateIdCall(IDispatch* const object, const LONG id,
                             const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    invoke(object, id, DISPATCH_METHOD, values.data(), count, nullptr, 0,
           nullptr);
}

VARIANT* __cdecl __vbaLateIdCallLd(VARIANT* const result,
                                   IDispatch* const object, const LONG id,
                                   const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    invoke(object, id, DISPATCH_METHOD | DISPATCH_PROPERTYGET, values.data(),
           count, nullptr, 0, result);
    return result;
}

void __cdecl __vbaLateIdCallSt(IDispatch* const object, const LONG id,
                               const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count + 1);
    va_end(list);
    put(object, id, DISPATCH_PROPERTYPUT, values);
}

void __cdecl __vbaLateIdStAd(IDispatch* const object, const LONG id,
                             const LONG count, ...) {
    va_list list;
    va_start(list, count);
    std::vector<VARIANT> values = take_variants(list, count + 1);
    va_end(list);
    put(object, id, DISPATCH_PROPERTYPUTREF, values);
}

void WINAPI __vbaLateIdSt(IDispatch* const object, const LONG id,
                          VARIANT value) {
    std::vector<VARIANT> values(1, value);
    put(object, id, DISPATCH_PROPERTYPUT, values);
}

// With named arguments: after the argument count come the number of names,
// the names, then the arguments (the named ones first).
void __cdecl __vbaLateMemNamedCall(IDispatch* const object, const BSTR name,
                                   const LONG count, const LONG name_count,
                                   ...) {
    va_list list;
    va_start(list, name_count);
    std::vector<BSTR> names;
    for (LONG index = 0; index < name_count; ++index) {
        names.push_back(va_arg(list, BSTR));
    }
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    invoke_named(object, name, names, DISPATCH_METHOD, values, nullptr);
}

VARIANT* __cdecl __vbaLateMemNamedCallLd(VARIANT* const result,
                                         IDispatch* const object,
                                         const BSTR name, const LONG count,
                                         const LONG name_count, ...) {
    va_list list;
    va_start(list, name_count);
    std::vector<BSTR> names;
    for (LONG index = 0; index < name_count; ++index) {
        names.push_back(va_arg(list, BSTR));
    }
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    invoke_named(object, name, names, DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                 values, result);
    return result;
}

// By dispatch id the names are dispatch ids too; one named argument at most.
void __cdecl __vbaLateIdNamedCall(IDispatch* const object, const LONG id,
                                  const LONG count, const LONG name_count,
                                  ...) {
    va_list list;
    va_start(list, name_count);
    std::vector<DISPID> names;
    for (LONG index = 0; index < name_count; ++index) {
        names.push_back(va_arg(list, LONG));
    }
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    if (name_count > 1) {
        static_cast<void>(vbrun::raise(5));
        return;
    }
    invoke(object, id, DISPATCH_METHOD, values.data(), count, names.data(),
           name_count, nullptr);
}

VARIANT* __cdecl __vbaLateIdNamedCallLd(VARIANT* const result,
                                        IDispatch* const object, const LONG id,
                                        const LONG count, const LONG name_count,
                                        ...) {
    va_list list;
    va_start(list, name_count);
    std::vector<DISPID> names;
    for (LONG index = 0; index < name_count; ++index) {
        names.push_back(va_arg(list, LONG));
    }
    std::vector<VARIANT> values = take_variants(list, count);
    va_end(list);
    if (name_count > 1) {
        static_cast<void>(vbrun::raise(5));
        return result;
    }
    invoke(object, id, DISPATCH_METHOD | DISPATCH_PROPERTYGET, values.data(),
           count, names.data(), name_count, result);
    return result;
}

}  // extern "C"
