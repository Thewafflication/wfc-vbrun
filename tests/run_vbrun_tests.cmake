# Verifies the VBRUN library against the retained MSVBVM60 inventory.
# Usage: cmake -DTESTER=<vbrun_tests> -DDLL=<wfcvbrun.dll>
#   -DEVIDENCE=<msvbvm60-6.0-runtime.json> -DWORK=<dir> -P run_vbrun_tests.cmake
file(MAKE_DIRECTORY "${WORK}")
file(READ "${EVIDENCE}" json)
string(JSON exports GET "${json}" exportDirectory exports)
string(JSON count LENGTH "${exports}")
math(EXPR last "${count} - 1")
set(csv "")
foreach(index RANGE ${last})
  string(JSON present GET "${exports}" ${index} present)
  if(present)
    string(JSON ordinal GET "${exports}" ${index} ordinal)
    string(JSON name GET "${exports}" ${index} name)
    string(APPEND csv "${ordinal},${name}\n")
  endif()
endforeach()
file(WRITE "${WORK}/exports.csv" "${csv}")

function(run_step label expected_result)
  execute_process(
    COMMAND "${TESTER}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
  )
  if(NOT result EQUAL ${expected_result})
    message(
      FATAL_ERROR
      "${label}: exit ${result}, expected ${expected_result}\n${out}${err}"
    )
  endif()
  set(step_output "${out}${err}" PARENT_SCOPE)
endfunction()

run_step("layout" 0 layout "${DLL}" "${WORK}/exports.csv")
run_step("load" 0 load "${DLL}")
# An unimplemented export names itself and stops the process with 0x5642.
run_step("stub" 22082 call "${DLL}" "__vbaI4Var")
if(NOT step_output MATCHES "__vbaI4Var \\(ordinal [0-9]+\\) is not implemented")
  message(FATAL_ERROR "stub diagnostic missing: ${step_output}")
endif()
