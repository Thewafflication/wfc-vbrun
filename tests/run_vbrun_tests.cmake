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
# Every export is implemented; the p-code entry, which needs an engine WFC
# does not have, stops the process with a diagnostic and 0x5642.
run_step("p-code" 22082 call "${DLL}" "ProcCallEngine")
set(pcode_pattern "p-code \\(ProcCallEngine\\) is not supported")
if(NOT step_output MATCHES "${pcode_pattern}")
  message(FATAL_ERROR "p-code diagnostic missing: ${step_output}")
endif()
