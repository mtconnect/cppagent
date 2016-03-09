cmake_minimum_required (VERSION 2.6) 

project(cppagent)

if(MSVC)
    add_subdirectory(libxml2_make "${CMAKE_BINARY_DIR}/libxml2")
    add_subdirectory(cppunit_make "${CMAKE_BINARY_DIR}/cppunit")
endif(MSVC)

add_subdirectory(agent)

add_subdirectory(test)

