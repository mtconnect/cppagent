cmake_minimum_required (VERSION 2.6) 

project(cppagent)

if(MSVC)
  # Default winver to Vista and Later
  set(WINVER "0x0600" CACHE STRING "Windows Target Version: 0x0400 - 95 & NT 4.0, 0x0500 - 2000, 0x0501 - XP, 0x0600 - Vista, 0x0601 - 7, 0x0602 - 8")
    
  add_subdirectory(libxml2_make "${CMAKE_BINARY_DIR}/libxml2")
  add_subdirectory(cppunit_make "${CMAKE_BINARY_DIR}/cppunit")
endif(MSVC)

add_subdirectory(agent)

add_subdirectory(test)

