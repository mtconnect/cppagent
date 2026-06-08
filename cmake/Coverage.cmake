# Code coverage instrumentation.
#
# Enable with -DAGENT_ENABLE_COVERAGE=ON (or, via Conan, -o "&:coverage=True").
# Must be included before the agent/test targets are defined so the flags
# propagate to every subsequently-declared target.
#
# Clang/AppleClang -> LLVM source-based coverage (llvm-profdata + llvm-cov)
# GCC              -> gcov instrumentation (gcov/gcovr/lcov)

option(AGENT_ENABLE_COVERAGE "Instrument the agent and tests for code coverage" OFF)

if(AGENT_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Coverage: enabling LLVM source-based coverage instrumentation")
    add_compile_options(
      $<$<COMPILE_LANGUAGE:CXX>:-fprofile-instr-generate>
      $<$<COMPILE_LANGUAGE:CXX>:-fcoverage-mapping>)
    add_link_options(-fprofile-instr-generate)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Coverage: enabling gcov instrumentation")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:--coverage>)
    add_link_options(--coverage)
  else()
    message(WARNING
      "AGENT_ENABLE_COVERAGE requested but compiler '${CMAKE_CXX_COMPILER_ID}' is not supported")
  endif()
endif()
