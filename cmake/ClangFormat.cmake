# Copyright Tomas Zeman 2019.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

function(clangformat_setup target)
  if(NOT CLANGFORMAT_EXECUTABLE)
    set(CLANGFORMAT_EXECUTABLE clang-format)
  endif()

  if(NOT EXISTS ${CLANGFORMAT_EXECUTABLE})
    find_program(clangformat_executable_tmp ${CLANGFORMAT_EXECUTABLE})
    if(clangformat_executable_tmp)
      set(CLANGFORMAT_EXECUTABLE ${clangformat_executable_tmp})
      unset(clangformat_executable_tmp)
    else()
      set(CLANGFORMAT_EXECUTABLE "NotFound")
      message(INFO "ClangFormat executable not found, skipping code format targets.")
    endif()
  endif()

  foreach(clangformat_source ${ARGV})
    if (${clangformat_source} MATCHES "\.?pp$")
      get_filename_component(clangformat_source ${clangformat_source} ABSOLUTE)
      list(APPEND clangformat_sources ${clangformat_source})
    endif()
  endforeach()

  if(NOT ("${CLANGFORMAT_EXECUTABLE}" STREQUAL "NotFound"))
    add_custom_target(${target}_clangformat
        COMMAND
        ${CLANGFORMAT_EXECUTABLE}
        -style=file
        -i
        ${clangformat_sources}
	COMMENT
	"Formating with ${CLANGFORMAT_EXECUTABLE} ..."
      )
    
    if(TARGET clangformat)
      add_dependencies(clangformat ${target}_clangformat)
    else()
      add_custom_target(clangformat DEPENDS ${target}_clangformat)
    endif()
  endif()
endfunction()

function(target_clangformat_setup target)
  get_target_property(target_sources ${target} SOURCES)
  clangformat_setup(${target} ${target_sources})
endfunction()
