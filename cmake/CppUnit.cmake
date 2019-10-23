# This module allows the addition of CppUnit to a target.
# Depending on or compiler this is either via our sub-module or searching for a package on the
# system

if(MSVC)
  add_subdirectory("${CMAKE_SOURCE_DIR}/cppunit_make" "${CMAKE_BINARY_DIR}/cppunit")
endif()

####################################################################################################
#                                                                                                  #
#                                            Functions                                             #
#                                                                                                  #
####################################################################################################

function(AddCppUnitSupport projectTarget)
  if(WIN32)
    # For Windows we compile our own version of CppUnit from our Git submodule source
    # Define a dependency between the targets and link them.
    set(CPPUNIT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/cppunit2/include")
    set(CPPUNIT_LIBRARY cppunit)
    add_dependencies(${projectTarget} cppunit)
  elseif(UNIX)
    # We will use the system's installed packages; which we have to look for.
    find_package(CppUnit REQUIRED)
  endif()

  target_include_directories(${projectTarget} PRIVATE "${CPPUNIT_INCLUDE_DIR}")
  target_link_libraries(${projectTarget} PRIVATE "${CPPUNIT_LIBRARY}")
endfunction()
