# wfc-vbrun: the VBRUN compatibility library (ADR-0007), wfcvbrun.dll.
#
# Built as a DLL (target wfc_vbrun) and as a static library (target
# wfc_vbrun_static) so a program can link it either way. The version is the
# one in VERSION and is stamped into the DLL's version resource.
#
# Parent project variables used when present: WFC_BUILD_TESTS, BUILD_TESTING,
# WFC_VBRUN_EVIDENCE (the retained MSVBVM60 inventory), and the functions
# wfc_enable_strict_warnings and wsp_enable_hardening.

if(NOT WIN32)
  return()
endif()

file(
  STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
  WFC_VBRUN_VERSION
  LIMIT_COUNT 1
)
set(WFC_VBRUN_VERSION "${WFC_VBRUN_VERSION}" CACHE INTERNAL "wfcvbrun version")

set(
  wfc_vbrun_sources
  src/vbrun_stubs.cpp
  src/vbrun_exports.inc
  src/vbrun_helpers.hpp
  src/vbrun_state.cpp
  src/helpers_memory.cpp
  src/helpers_strings.cpp
  src/helpers_variant.cpp
  src/helpers_convert.cpp
  src/helpers_varops.cpp
)

function(wfc_vbrun_harden target)
  target_compile_features(${target} PRIVATE cxx_std_20)
  if(COMMAND wfc_enable_strict_warnings)
    wfc_enable_strict_warnings(${target})
  endif()
  if(COMMAND wsp_enable_hardening)
    wsp_enable_hardening(${target})
  endif()
endfunction()

# --- The DLL --------------------------------------------------------------
enable_language(RC)
string(REPLACE "." ";" version_parts "${WFC_VBRUN_VERSION}")
list(GET version_parts 0 WFC_LIB_VERSION_MAJOR)
list(GET version_parts 1 WFC_LIB_VERSION_MINOR)
list(GET version_parts 2 WFC_LIB_VERSION_PATCH)
set(WFC_LIB_VERSION "${WFC_VBRUN_VERSION}")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/TARGET" WFC_LIB_TARGET LIMIT_COUNT 1)
configure_file(
  src/registration_info.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/registration_info.hpp"
  @ONLY
)
set(WFC_LIB_NAME wfcvbrun)
set(WFC_LIB_DESCRIPTION "WFC VBRUN compatibility library")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/wfcvbrun.rc" @ONLY)

add_library(
  wfc_vbrun
  SHARED
  ${wfc_vbrun_sources}
  src/registration.cpp
  src/wfcvbrun.def
  "${CMAKE_CURRENT_BINARY_DIR}/wfcvbrun.rc"
)
target_link_libraries(wfc_vbrun PRIVATE oleaut32 advapi32)
target_include_directories(wfc_vbrun PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(
  wfc_vbrun
  PROPERTIES
    OUTPUT_NAME wfcvbrun
    VERSION "${WFC_VBRUN_VERSION}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
# The reference assigns ordinals to the COM entry points; keep them.
target_link_options(wfc_vbrun PRIVATE /IGNORE:4104 /IGNORE:4222)
wfc_vbrun_harden(wfc_vbrun)

# --- The static library ---------------------------------------------------
add_library(wfc_vbrun_static STATIC ${wfc_vbrun_sources})
target_link_libraries(wfc_vbrun_static PUBLIC oleaut32)
set_target_properties(wfc_vbrun_static PROPERTIES OUTPUT_NAME wfcvbrun_static)
wfc_vbrun_harden(wfc_vbrun_static)

# --- Tests ----------------------------------------------------------------
if(BUILD_TESTING AND WFC_BUILD_TESTS)
  if(NOT DEFINED WFC_VBRUN_EVIDENCE)
    set(
      WFC_VBRUN_EVIDENCE
      "${CMAKE_SOURCE_DIR}/evidence/reference/msvbvm60-6.0-runtime.json"
    )
  endif()
  add_executable(vbrun_tests tests/vbrun_tests.cpp)
  target_compile_features(vbrun_tests PRIVATE cxx_std_20)
  wfc_vbrun_harden(vbrun_tests)
  add_dependencies(vbrun_tests wfc_vbrun)
  add_test(
    NAME TC-MP0007-vbrun-library
    COMMAND
      "${CMAKE_COMMAND}" "-DTESTER=$<TARGET_FILE:vbrun_tests>"
      "-DDLL=$<TARGET_FILE:wfc_vbrun>" "-DEVIDENCE=${WFC_VBRUN_EVIDENCE}"
      "-DWORK=${CMAKE_BINARY_DIR}/vbrun-test" -P
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_vbrun_tests.cmake"
  )
  set_tests_properties(
    TC-MP0007-vbrun-library
    PROPERTIES LABELS "unit;mp-0007;vbrun"
  )
  # The helpers (REQ-0100 to REQ-0104): behavior of the implemented exports.
  add_executable(vbrun_helper_tests tests/vbrun_helper_tests.cpp)
  target_link_libraries(vbrun_helper_tests PRIVATE oleaut32)
  wfc_vbrun_harden(vbrun_helper_tests)
  add_dependencies(vbrun_helper_tests wfc_vbrun)
  foreach(vbrun_family memory)
    add_test(
      NAME TC-MP0009-vbrun-helpers-${vbrun_family}
      COMMAND vbrun_helper_tests "$<TARGET_FILE:wfc_vbrun>" ${vbrun_family}
    )
    set_tests_properties(
      TC-MP0009-vbrun-helpers-${vbrun_family}
      PROPERTIES LABELS "unit;mp-0009;vbrun"
    )
  endforeach()
  # Call cases: the results of the reference runtime are in
  # tests/cases/<family>.expected.
  set(vbrun_case_files "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases")
  foreach(vbrun_cases strings variants convert varops)
    add_test(
      NAME TC-MP0009-vbrun-cases-${vbrun_cases}
      COMMAND
        vbrun_helper_tests "$<TARGET_FILE:wfc_vbrun>" cases
        "${vbrun_case_files}/${vbrun_cases}.cases"
        "${vbrun_case_files}/${vbrun_cases}.expected"
    )
    set_tests_properties(
      TC-MP0009-vbrun-cases-${vbrun_cases}
      PROPERTIES LABELS "unit;mp-0009;vbrun"
    )
  endforeach()
  # The static library links and answers (a program can link it either way).
  add_executable(vbrun_static_tests tests/vbrun_static_tests.cpp)
  target_link_libraries(vbrun_static_tests PRIVATE wfc_vbrun_static)
  wfc_vbrun_harden(vbrun_static_tests)
  add_test(NAME TC-MP0010-vbrun-static COMMAND vbrun_static_tests)
  set_tests_properties(
    TC-MP0010-vbrun-static
    PROPERTIES LABELS "unit;mp-0010;vbrun"
  )
  # A C program loads wfcvbrun.dll, checks its COM entry points and version
  # resource, and registers and unregisters it for the current user.
  enable_language(C)
  add_executable(vbrun_dll_test tests/vbrun_dll_test.c)
  target_link_libraries(vbrun_dll_test PRIVATE version advapi32)
  add_dependencies(vbrun_dll_test wfc_vbrun)
  if(MSVC)
    target_compile_options(vbrun_dll_test PRIVATE /W4 /WX)
  else()
    target_compile_options(vbrun_dll_test PRIVATE -Wall -Wextra -Werror)
  endif()
  if(COMMAND wsp_enable_hardening)
    wsp_enable_hardening(vbrun_dll_test)
  endif()
  add_test(
    NAME TC-MP0010-vbrun-dll
    COMMAND
      vbrun_dll_test "$<TARGET_FILE:wfc_vbrun>" "${WFC_VBRUN_VERSION}"
      "${WFC_LIB_TARGET}"
  )
  set_tests_properties(
    TC-MP0010-vbrun-dll
    PROPERTIES
      LABELS "integration;mp-0010;vbrun"
      RESOURCE_LOCK registry
      SKIP_RETURN_CODE 77
  )
endif()
