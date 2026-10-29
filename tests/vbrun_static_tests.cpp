// The static form of the VBRUN library: the helpers are linked into the
// program (target wfc_vbrun_static) instead of loaded from wfcvbrun.dll.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <oleauto.h>

#include <cstdio>

extern "C" LONG WINAPI __vbaLenBstr(const BSTR text);
extern "C" LONG WINAPI __vbaLenBstrB(const BSTR text);

int main() {
    BSTR text = SysAllocString(L"hello");
    const LONG length = __vbaLenBstr(text);
    const LONG bytes = __vbaLenBstrB(text);
    SysFreeString(text);
    if (length != 5 || bytes != 10 || __vbaLenBstr(nullptr) != 0) {
        std::fprintf(stderr, "static wfcvbrun: unexpected lengths %ld %ld\n",
                     length, bytes);
        return 1;
    }
    return 0;
}
