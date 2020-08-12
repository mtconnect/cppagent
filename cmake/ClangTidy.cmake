include_guard()

option(AGENT_ENABLE_CLANGTIDY "Runs clang-tidy on all source files" OFF)

find_program(
  CLANG_TIDY_EXECUTABLE
  NAMES clang-tidy-10 clang-tidy-9 clang-tidy-8 clang-tidy-7 clang-tidy-6.0 clang-tidy-5.0 clang-tidy-4.0 clang-tidy
  DOC "Path to clang-tidy executable"
)

function(target_clangtidy_setup _target)
  if(AGENT_ENABLE_CLANGTIDY)
    if(CLANG_TIDY_EXECUTABLE)
      set_target_properties(
        ${_target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}"
      )
    else()
      message(FATAL_ERROR "Could not find clang-tidy")
    endif()
  endif()
endfunction()
