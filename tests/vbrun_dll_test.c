/* Black-box check of wfcvbrun.dll: exports, version resource and the
 * registration round trip (see dll_check.h). Run by CTest with the DLL,
 * the version and the target as arguments. The library has no COM
 * classes. */
#include "dll_check.h"

int main(int argc, char** argv) {
    return wfc_check_dll(argc, argv, L"vbrun", NULL, 0U);
}
