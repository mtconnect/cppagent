# Copyright Tomas Zeman 2019.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set_property(GLOBAL PROPERTY CLANGFORMAT_FILES "")

macro(target_clangformat_setup target)
  set(sources "")
  get_target_property(target_sources ${target} SOURCES)
  foreach(clangformat_source ${target_sources})
    if (${clangformat_source} MATCHES "\.?pp$")
      get_filename_component(clangformat_source ${clangformat_source} ABSOLUTE)
      list(APPEND sources ${clangformat_source})
    endif()
  endforeach()
  
  get_property(clangformat_files GLOBAL PROPERTY CLANGFORMAT_FILES)
  set_property(GLOBAL PROPERTY CLANGFORMAT_FILES "${clangformat_files};${sources}")
endmacro()

macro(create_clangformat_target)
  get_property(clangformat_files GLOBAL PROPERTY CLANGFORMAT_FILES)

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
      message(INFO ": ClangFormat executable not found, skipping code format targets.")
    endif()
  endif()

  if(NOT ("${CLANGFORMAT_EXECUTABLE}" STREQUAL "NotFound"))
    add_custom_target(clangformat
        COMMAND
        ${CLANGFORMAT_EXECUTABLE}
        -style=file
        -i
        ${clangformat_files}
	COMMENT
	"Formating with ${CLANGFORMAT_EXECUTABLE} ..."
      )
  endif()

   #set_property(TARGET clangformat PROPERTY SOURCES ${clangformat_files})
endmacro()
