# Toolchain file for a Linux build with Wine's winegcc (Winelib):
#   cmake -S . -B out/winelib -DCMAKE_TOOLCHAIN_FILE=cmake/winelib.cmake
#         -DWFC_WINELIB_ARCH=x64 -DWFC_WARNINGS_AS_ERRORS=OFF
# WFC_WINELIB_ARCH is x64 (default), x86 (-m32, needs the 32-bit Wine
# development packages) or arm64 (an arm64 host). The programs are Linux
# programs linked against Wine's Windows libraries; run them directly.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER winegcc)
set(CMAKE_CXX_COMPILER wineg++)
set(WFC_WINELIB ON CACHE BOOL "Build with Winelib" FORCE)
if(WFC_WINELIB_ARCH STREQUAL "x86")
  set(CMAKE_C_FLAGS_INIT "-m32")
  set(CMAKE_CXX_FLAGS_INIT "-m32")
endif()
