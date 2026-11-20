# wfc-vbrun: the VBRUN compatibility library (ADR-0007), wfcvbrun.dll.
#
# Built as a DLL (target wfc_vbrun) and as a static library (target
# wfc_vbrun_static) so a program can link it either way. The version is the
# one in VERSION and is stamped into the DLL's version resource.
#
# Parent project variables used when present: WFC_BUILD_TESTS, BUILD_TESTING,
# WFC_VBRUN_EVIDENCE (the retained MSVBVM60 inventory), and the functions
# wfc_enable_strict_warnings and wsp_enable_hardening.

# Standalone build: the library outside WFC. WFC itself adds this directory
# with add_subdirectory and supplies the project, CTest and the WSP hardening.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.28)
  project(wfc-vbrun LANGUAGES CXX)
  if(WFC_WINELIB)
    # winegcc writes name.exe (a launcher) and name.exe.so; the platform module
    # resets the suffix when the project is declared, so it is set here.
    set(CMAKE_EXECUTABLE_SUFFIX ".exe")
  endif()
  option(WFC_BUILD_TESTS "Build the tests" ON)
  include(CTest)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  include(WfcWarnings)
  # WSP-SEC-0015: the pinned WSP baseline supplies the hardening flags (not for
  # the WebAssembly and Winelib targets, whose toolchains do not take them).
  if(
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wsp/tools/cmake/WspBuild.cmake"
    AND NOT EMSCRIPTEN
    AND NOT WFC_WINELIB
  )
    include("${CMAKE_CURRENT_SOURCE_DIR}/wsp/tools/cmake/WspBuild.cmake")
  endif()
endif()

# WFC_WINELIB: a Linux build with Wine's winegcc (cmake/winelib.cmake); it
# builds the static library and its test, not the DLL.
if(NOT WIN32 AND NOT WFC_WINELIB)
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
  src/helpers_language.cpp
  src/helpers_varfuncs.cpp
  src/helpers_varstrings.cpp
  src/helpers_numeric.cpp
  src/helpers_scalars.cpp
  src/helpers_late.cpp
  src/helpers_foreach.cpp
  src/helpers_settings.cpp
  src/helpers_objects.cpp
  src/helpers_errobj.cpp
  src/helpers_errors.cpp
  src/helpers_finance.cpp
  src/helpers_dates.cpp
  src/helpers_arrays.cpp
  src/helpers_lists.cpp
  src/helpers_system.cpp
  src/helpers_files.cpp
  src/helpers_openfiles.cpp
  src/helpers_varops.cpp
  src/helpers_regcall.cpp
  src/vbrun_regcall.hpp
  src/helpers_x87.cpp
  src/helpers_x87math.cpp
  src/helpers_members.cpp
  src/helpers_frames.cpp
  src/helpers_records.cpp
  src/helpers_engine.cpp
  src/x87_core.hpp
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

if(WIN32)
  # --- The DLL --------------------------------------------------------------
  enable_language(RC)
  string(REPLACE "." ";" version_parts "${WFC_VBRUN_VERSION}")
  list(GET version_parts 0 WFC_LIB_VERSION_MAJOR)
  list(GET version_parts 1 WFC_LIB_VERSION_MINOR)
  list(GET version_parts 2 WFC_LIB_VERSION_PATCH)
  set(WFC_LIB_VERSION "${WFC_VBRUN_VERSION}")
  file(
    STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/TARGET"
    WFC_LIB_TARGET
    LIMIT_COUNT 1
  )
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
  target_link_libraries(wfc_vbrun PRIVATE oleaut32 ole32 advapi32 user32)
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
endif()

# --- The static library ---------------------------------------------------
add_library(wfc_vbrun_static STATIC ${wfc_vbrun_sources})
target_link_libraries(wfc_vbrun_static PUBLIC oleaut32 ole32 advapi32 user32)
set_target_properties(wfc_vbrun_static PROPERTIES OUTPUT_NAME wfcvbrun_static)
wfc_vbrun_harden(wfc_vbrun_static)

# --- Tests ----------------------------------------------------------------
if(BUILD_TESTING AND WFC_BUILD_TESTS)
  if(WIN32)
    if(NOT DEFINED WFC_VBRUN_EVIDENCE)
      set(
        WFC_VBRUN_EVIDENCE
        "${CMAKE_SOURCE_DIR}/evidence/reference/msvbvm60-6.0-runtime.json"
      )
    endif()
    # The inventory of the reference runtime ships with WFC, not alone.
    if(EXISTS "${WFC_VBRUN_EVIDENCE}")
      add_executable(vbrun_tests tests/vbrun_tests.cpp)
      target_compile_features(vbrun_tests PRIVATE cxx_std_20)
      wfc_vbrun_harden(vbrun_tests)
      add_dependencies(vbrun_tests wfc_vbrun)
      add_test(
        NAME TC-0001-library
        COMMAND
          "${CMAKE_COMMAND}" "-DTESTER=$<TARGET_FILE:vbrun_tests>"
          "-DDLL=$<TARGET_FILE:wfc_vbrun>" "-DEVIDENCE=${WFC_VBRUN_EVIDENCE}"
          "-DWORK=${CMAKE_BINARY_DIR}/vbrun-test" -P
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_vbrun_tests.cmake"
      )
      set_tests_properties(TC-0001-library PROPERTIES LABELS "unit;vbrun")
    endif()
    # The helpers (REQ-0002 to REQ-0006): behavior of the implemented exports.
    add_executable(vbrun_helper_tests tests/vbrun_helper_tests.cpp)
    target_link_libraries(vbrun_helper_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_helper_tests)
    add_dependencies(vbrun_helper_tests wfc_vbrun)
    # Each entry is <test case number>:<family>.
    foreach(vbrun_entry 0002:memory 0003:clock)
      string(REPLACE ":" ";" vbrun_entry "${vbrun_entry}")
      list(GET vbrun_entry 0 vbrun_number)
      list(GET vbrun_entry 1 vbrun_family)
      add_test(
        NAME TC-${vbrun_number}-helpers-${vbrun_family}
        COMMAND vbrun_helper_tests "$<TARGET_FILE:wfc_vbrun>" ${vbrun_family}
      )
      set_tests_properties(
        TC-${vbrun_number}-helpers-${vbrun_family}
        PROPERTIES LABELS "unit;vbrun"
      )
    endforeach()
    # The open-file table: a sequence of file statements against the results
    # of a Visual Basic 6 program.
    add_executable(
      vbrun_file_tests
      tests/vbrun_file_tests.cpp
      src/helpers_openfiles.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_file_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_file_tests)
    # Its own working directory: the file tests make files of the same names
    # and run side by side.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-openfiles")
    add_test(
      NAME TC-0004-openfiles
      COMMAND
        vbrun_file_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/openfiles.expected"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-openfiles"
    )
    set_tests_properties(TC-0004-openfiles PROPERTIES LABELS "unit;vbrun")
    # Print, Write, Input, Get and Put against a Visual Basic 6 program.
    add_executable(
      vbrun_file_data_tests
      tests/vbrun_file_data_tests.cpp
      src/helpers_openfiles.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_file_data_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_file_data_tests)
    # Its own working directory: the file tests make files of the same names
    # and run side by side.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-filedata")
    add_test(
      NAME TC-0005-filedata
      COMMAND
        vbrun_file_data_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/fileprint.expected"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/fileinput.expected"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/filenumbers.expected"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-filedata"
    )
    set_tests_properties(TC-0005-filedata PROPERTIES LABELS "unit;vbrun")
    # StrComp on Variants, Command$, DoEvents and the small host exports.
    add_executable(
      vbrun_system_misc_tests
      tests/vbrun_system_misc_tests.cpp
      src/helpers_scalars.cpp
      src/helpers_system.cpp
      src/helpers_varstrings.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_system_misc_tests PRIVATE oleaut32 user32)
    wfc_vbrun_harden(vbrun_system_misc_tests)
    add_test(NAME TC-0006-systemmisc COMMAND vbrun_system_misc_tests)
    set_tests_properties(TC-0006-systemmisc PROPERTIES LABELS "unit;vbrun")
    # Late binding.
    add_executable(
      vbrun_late_tests
      tests/vbrun_late_tests.cpp
      src/helpers_late.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_late_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_late_tests)
    add_test(NAME TC-0007-late COMMAND vbrun_late_tests)
    # For Each over arrays, Variants and a collection.
    add_executable(
      vbrun_foreach_tests
      tests/vbrun_foreach_tests.cpp
      src/helpers_foreach.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_foreach_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_foreach_tests)
    add_test(NAME TC-0008-foreach COMMAND vbrun_foreach_tests)
    # The smaller exports: state, strings, arrays, loops, Shell and dialogs.
    add_executable(
      vbrun_system_more_tests
      tests/vbrun_system_more_tests.cpp
      src/helpers_scalars.cpp
      src/helpers_system.cpp
      src/helpers_varstrings.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(
      vbrun_system_more_tests
      PRIVATE oleaut32 user32 advapi32
    )
    wfc_vbrun_harden(vbrun_system_more_tests)
    add_test(NAME TC-0009-systemmore COMMAND vbrun_system_more_tests)
    set_tests_properties(TC-0009-systemmore PROPERTIES LABELS "unit;vbrun")
    # CreateObject and GetObject.
    add_executable(
      vbrun_objects_tests
      tests/vbrun_objects_tests.cpp
      src/helpers_objects.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_objects_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_objects_tests)
    add_test(NAME TC-0010-objects COMMAND vbrun_objects_tests)
    set_tests_properties(TC-0010-objects PROPERTIES LABELS "unit;vbrun")
    # The Err object.
    add_executable(
      vbrun_errobj_tests
      tests/vbrun_errobj_tests.cpp
      src/helpers_errobj.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_errobj_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_errobj_tests)
    add_test(NAME TC-0011-errobj COMMAND vbrun_errobj_tests)
    set_tests_properties(TC-0011-errobj PROPERTIES LABELS "unit;vbrun")
    # The helpers called with their operands in registers, and the Zombie
    # interface.
    add_executable(
      vbrun_regcall_tests
      tests/vbrun_regcall_tests.cpp
      src/helpers_regcall.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_regcall_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_regcall_tests)
    add_test(NAME TC-0012-regcall COMMAND vbrun_regcall_tests)
    set_tests_properties(TC-0012-regcall PROPERTIES LABELS "unit;vbrun")
    # The helpers that work on the x87 stack.
    add_executable(
      vbrun_x87_tests
      tests/vbrun_x87_tests.cpp
      src/helpers_x87.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_x87_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_x87_tests)
    add_test(NAME TC-0013-x87 COMMAND vbrun_x87_tests)
    set_tests_properties(TC-0013-x87 PROPERTIES LABELS "unit;vbrun")
    # Sin, Cos, Tan, Atn, Exp, Log, Sqr, the _adj_ divides and _allmul.
    add_executable(
      vbrun_x87math_tests
      tests/vbrun_x87math_tests.cpp
      src/helpers_x87math.cpp
    )
    wfc_vbrun_harden(vbrun_x87math_tests)
    add_test(NAME TC-0014-x87math COMMAND vbrun_x87math_tests)
    set_tests_properties(TC-0014-x87math PROPERTIES LABELS "unit;vbrun")
    # PutMemVar, PutMemObj, SetMemVar, the sinks and the record-array move.
    add_executable(
      vbrun_members_tests
      tests/vbrun_members_tests.cpp
      src/helpers_members.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_members_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_members_tests)
    add_test(NAME TC-0015-members COMMAND vbrun_members_tests)
    set_tests_properties(TC-0015-members PROPERTIES LABELS "unit;vbrun")
    # Record copy and destroy from a descriptor.
    add_executable(
      vbrun_records_tests
      tests/vbrun_records_tests.cpp
      src/helpers_records.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_records_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_records_tests)
    add_test(NAME TC-0016-records COMMAND vbrun_records_tests)
    set_tests_properties(TC-0016-records PROPERTIES LABELS "unit;vbrun")
    # The GoSub list and the error-handler frame helpers.
    add_executable(
      vbrun_frames_tests
      tests/vbrun_frames_tests.cpp
      src/helpers_frames.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_frames_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_frames_tests)
    add_test(NAME TC-0017-frames COMMAND vbrun_frames_tests)
    set_tests_properties(TC-0017-frames PROPERTIES LABELS "unit;vbrun")
    # Get # and Put # of records and arrays, Print on an object.
    add_executable(
      vbrun_owner_tests
      tests/vbrun_owner_tests.cpp
      src/helpers_openfiles.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_owner_tests PRIVATE oleaut32 ole32)
    wfc_vbrun_harden(vbrun_owner_tests)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-owner")
    add_test(
      NAME TC-0018-owner
      COMMAND vbrun_owner_tests
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-owner"
    )
    set_tests_properties(TC-0018-owner PROPERTIES LABELS "unit;vbrun")
    # Declare, End, Stop, the program and DLL entries, contexts, projects.
    add_executable(
      vbrun_engine_tests
      tests/vbrun_engine_tests.cpp
      src/helpers_engine.cpp
      src/helpers_openfiles.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(
      vbrun_engine_tests
      PRIVATE oleaut32 ole32 advapi32 user32
    )
    wfc_vbrun_harden(vbrun_engine_tests)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-engine")
    add_test(
      NAME TC-0019-engine
      COMMAND vbrun_engine_tests
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-engine"
    )
    set_tests_properties(TC-0019-engine PROPERTIES LABELS "unit;vbrun")
    # SaveSetting, GetSetting, GetAllSettings and DeleteSetting.
    add_executable(
      vbrun_settings_tests
      tests/vbrun_settings_tests.cpp
      src/helpers_settings.cpp
      src/helpers_errors.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_settings_tests PRIVATE oleaut32 advapi32)
    wfc_vbrun_harden(vbrun_settings_tests)
    add_test(NAME TC-0020-settings COMMAND vbrun_settings_tests)
    set_tests_properties(TC-0020-settings PROPERTIES LABELS "unit;vbrun")
    set_tests_properties(TC-0008-foreach PROPERTIES LABELS "unit;vbrun")
    set_tests_properties(TC-0007-late PROPERTIES LABELS "unit;vbrun")
    # The Mid$ statement.
    add_executable(
      vbrun_midstmt_tests
      tests/vbrun_midstmt_tests.cpp
      src/helpers_language.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_midstmt_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_midstmt_tests)
    add_test(NAME TC-0021-midstmt COMMAND vbrun_midstmt_tests)
    set_tests_properties(TC-0021-midstmt PROPERTIES LABELS "unit;vbrun")
    # Width, FileAttr, Name, Lock, Input$ and fixed length strings.
    add_executable(
      vbrun_file_misc_tests
      tests/vbrun_file_misc_tests.cpp
      src/helpers_openfiles.cpp
      src/vbrun_state.cpp
    )
    target_link_libraries(vbrun_file_misc_tests PRIVATE oleaut32)
    wfc_vbrun_harden(vbrun_file_misc_tests)
    # Its own working directory: the file tests make files of the same names
    # and run side by side.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-filemisc")
    add_test(
      NAME TC-0022-filemisc
      COMMAND
        vbrun_file_misc_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/filemisc.expected"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/work-filemisc"
    )
    set_tests_properties(TC-0022-filemisc PROPERTIES LABELS "unit;vbrun")
    # Call cases: the results of the reference runtime are in
    # tests/cases/<family>.expected.
    set(vbrun_case_files "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases")
    # Each entry is <test case number>:<family>.
    foreach(
      vbrun_entry
      0023:strings
      0024:variants
      0025:convert
      0026:varops
      0027:language
      0028:varfuncs
      0029:varstrings
      0030:scalars
      0031:scalars2
      0032:scalars3
      0033:random
      0034:random2
      0035:numeric
      0036:finance
      0037:dates
      0038:dates2
      0039:payments
      0040:arrays
      0041:lists
      0042:system
      0043:files
    )
      string(REPLACE ":" ";" vbrun_entry "${vbrun_entry}")
      list(GET vbrun_entry 0 vbrun_number)
      list(GET vbrun_entry 1 vbrun_cases)
      add_test(
        NAME TC-${vbrun_number}-cases-${vbrun_cases}
        COMMAND
          vbrun_helper_tests "$<TARGET_FILE:wfc_vbrun>" cases
          "${vbrun_case_files}/${vbrun_cases}.cases"
          "${vbrun_case_files}/${vbrun_cases}.expected"
      )
      set_tests_properties(
        TC-${vbrun_number}-cases-${vbrun_cases}
        PROPERTIES LABELS "unit;vbrun"
      )
    endforeach()
  endif()
  # The static library links and answers (a program can link it either way).
  add_executable(vbrun_static_tests tests/vbrun_static_tests.cpp)
  target_link_libraries(vbrun_static_tests PRIVATE wfc_vbrun_static)
  wfc_vbrun_harden(vbrun_static_tests)
  add_test(NAME TC-0044-static COMMAND vbrun_static_tests)
  set_tests_properties(
    TC-0044-static
    PROPERTIES LABELS "unit;vbrun" TIMEOUT 120
  )
  if(WIN32)
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
      NAME TC-0045-dll
      COMMAND
        vbrun_dll_test "$<TARGET_FILE:wfc_vbrun>" "${WFC_VBRUN_VERSION}"
        "${WFC_LIB_TARGET}"
    )
    set_tests_properties(
      TC-0045-dll
      PROPERTIES
        LABELS "integration;vbrun"
        RESOURCE_LOCK registry
        SKIP_RETURN_CODE 77
    )
  endif()
endif()
