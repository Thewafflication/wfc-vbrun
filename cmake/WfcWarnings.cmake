# Strict warnings for the library targets. WFC_WARNINGS_AS_ERRORS (default ON)
# can be turned off for toolchains whose system headers are not warning-clean
# (Wine's, for one).
option(WFC_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

function(wfc_enable_strict_warnings target)
  if(MSVC)
    target_compile_options(
      "${target}"
      PRIVATE /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor
    )
    if(WFC_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE /WX)
    endif()
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic)
    if(WFC_WARNINGS_AS_ERRORS)
      target_compile_options("${target}" PRIVATE -Werror)
    endif()
  endif()
endfunction()
