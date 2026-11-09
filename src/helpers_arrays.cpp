// REQ-0103: Split, Join and Filter, the functions of String arrays.
//
// Measured from a Visual Basic 6 program that declares the exports (see
// tests/cases/arrays.cases). A failure is an error of the thread (94 for a
// Null delimiter of Join, 5 for an array that is not one-dimensional).

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

#include "vbrun_helpers.hpp"

namespace {

constexpr int kInvalidArgument = 5;
constexpr int kInvalidUseOfNull = 94;

bool is_missing(const VARIANT* const value) {
    return value == nullptr ||
           (value->vt == VT_ERROR && value->scode == DISP_E_PARAMNOTFOUND);
}

bool is_null(const VARIANT* const value) {
    return (value->vt & ~static_cast<unsigned>(VT_BYREF)) == VT_NULL;
}

// A Variant as text; false after raising the error of a failed conversion.
bool text_of(const VARIANT* const value, std::wstring& text) {
    VARIANT converted;
    VariantInit(&converted);
    const HRESULT status =
        VariantChangeType(&converted, const_cast<VARIANT*>(value), 0, VT_BSTR);
    if (FAILED(status)) {
        static_cast<void>(vbrun::raise_hresult(status));
        return false;
    }
    text.assign(converted.bstrVal == nullptr ? L"" : converted.bstrVal,
                SysStringLen(converted.bstrVal));
    VariantClear(&converted);
    return true;
}

bool same_at(const std::wstring& text, const std::size_t at,
             const std::wstring& find, const bool fold) {
    if (at + find.size() > text.size()) {
        return false;
    }
    if (!fold) {
        return text.compare(at, find.size(), find) == 0;
    }
    return CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                          text.data() + at, static_cast<int>(find.size()),
                          find.data(), static_cast<int>(find.size())) ==
           CSTR_EQUAL;
}

bool contains(const std::wstring& text, const std::wstring& find,
              const bool fold) {
    if (find.empty()) {
        return true;
    }
    for (std::size_t at = 0; at + find.size() <= text.size(); ++at) {
        if (same_at(text, at, find, fold)) {
            return true;
        }
    }
    return false;
}

VARIANT* give_array(VARIANT* const result,
                    const std::vector<std::wstring>& items) {
    SAFEARRAYBOUND bound = {static_cast<ULONG>(items.size()), 0};
    SAFEARRAY* const array = SafeArrayCreate(VT_BSTR, 1, &bound);
    for (LONG at = 0; at < static_cast<LONG>(items.size()); ++at) {
        const std::wstring& item = items[static_cast<std::size_t>(at)];
        BSTR made = SysAllocStringLen(item.data(),
                                      static_cast<UINT>(item.size()));
        SafeArrayPutElement(array, &at, made);
        SysFreeString(made);
    }
    VariantClear(result);
    result->vt = static_cast<VARTYPE>(VT_ARRAY | VT_BSTR);
    result->parray = array;
    return result;
}

// The elements of a one-dimensional array in a Variant as text.
bool elements_of(const VARIANT* const value, std::vector<std::wstring>& out) {
    if ((value->vt & VT_ARRAY) == 0) {
        static_cast<void>(vbrun::raise(13));
        return false;
    }
    SAFEARRAY* array = (value->vt & VT_BYREF) != 0 ? *value->pparray
                                                    : value->parray;
    if (array == nullptr || SafeArrayGetDim(array) != 1) {
        static_cast<void>(vbrun::raise(array == nullptr ? 0 : kInvalidArgument));
        return array == nullptr;
    }
    const VARTYPE kind = static_cast<VARTYPE>(value->vt & VT_TYPEMASK);
    LONG low = 0;
    LONG high = -1;
    SafeArrayGetLBound(array, 1, &low);
    SafeArrayGetUBound(array, 1, &high);
    for (LONG at = low; at <= high; ++at) {
        if (kind == VT_BSTR) {
            BSTR item = nullptr;
            SafeArrayGetElement(array, &at, &item);
            out.emplace_back(item == nullptr ? L"" : item,
                             SysStringLen(item));
            SysFreeString(item);
        } else if (kind == VT_VARIANT) {
            VARIANT item;
            VariantInit(&item);
            SafeArrayGetElement(array, &at, &item);
            std::wstring text;
            const bool good = text_of(&item, text);
            VariantClear(&item);
            if (!good) {
                return false;
            }
            out.push_back(text);
        } else {
            static_cast<void>(vbrun::raise(13));
            return false;
        }
    }
    return true;
}

}  // namespace

extern "C" {

// Split(expression, delimiter, limit, compare).
VARIANT* WINAPI rtcSplit(VARIANT* const result, const BSTR expression,
                         VARIANT* const delimiter, const LONG limit,
                         const LONG compare) {
    std::wstring find = L" ";
    if (!is_missing(delimiter) && !text_of(delimiter, find)) {
        return result;
    }
    if (limit < -1) {
        static_cast<void>(vbrun::raise(kInvalidArgument));
        return result;
    }
    const std::wstring text(expression == nullptr ? L"" : expression,
                            SysStringLen(expression));
    std::vector<std::wstring> items;
    if (find.empty()) {
        items.push_back(text);
        return give_array(result, items);
    }
    if (text.empty() || limit == 0) {
        return give_array(result, items);
    }
    const bool fold = static_cast<short>(compare) == 1;
    std::size_t from = 0;
    std::size_t at = 0;
    while (at < text.size()) {
        if (limit > 0 && static_cast<LONG>(items.size()) + 1 >= limit) {
            break;
        }
        if (same_at(text, at, find, fold)) {
            items.push_back(text.substr(from, at - from));
            at += find.size();
            from = at;
        } else {
            ++at;
        }
    }
    items.push_back(text.substr(from));
    return give_array(result, items);
}

// Join(array, delimiter).
BSTR WINAPI rtcJoin(VARIANT* const source, VARIANT* const delimiter) {
    std::wstring glue = L" ";
    if (!is_missing(delimiter)) {
        if (is_null(delimiter)) {
            static_cast<void>(vbrun::raise(kInvalidUseOfNull));
            return nullptr;
        }
        if (!text_of(delimiter, glue)) {
            return nullptr;
        }
    }
    std::vector<std::wstring> items;
    if (!elements_of(source, items) || items.empty()) {
        return nullptr;
    }
    std::wstring joined;
    for (std::size_t at = 0; at < items.size(); ++at) {
        if (at != 0) {
            joined += glue;
        }
        joined += items[at];
    }
    return SysAllocStringLen(joined.data(), static_cast<UINT>(joined.size()));
}

// Filter(array, match, include, compare).
VARIANT* WINAPI rtcFilter(VARIANT* const result, VARIANT* const source,
                          const BSTR match, const LONG include,
                          const LONG compare) {
    std::vector<std::wstring> items;
    if (!elements_of(source, items)) {
        return result;
    }
    const std::wstring find(match == nullptr ? L"" : match,
                            SysStringLen(match));
    const bool keep = static_cast<short>(include) != 0;
    const bool fold = static_cast<short>(compare) == 1;
    std::vector<std::wstring> chosen;
    for (const std::wstring& item : items) {
        if (contains(item, find, fold) == keep) {
            chosen.push_back(item);
        }
    }
    return give_array(result, chosen);
}

}  // extern "C"
