cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
set(CMAKE_FIND_APPBUNDLE NEVER)

# The version number.
set (AGENT_VERSION_MAJOR 1)
set (AGENT_VERSION_MINOR 2)
set (AGENT_VERSION_PATCH 0)
set (AGENT_VERSION_BUILD 18)

if(WIN32)
  if(CMAKE_CL_64)
    foreach(flag_var
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${flag_var} MATCHES "WIN32")
        string(REGEX REPLACE "32" "64" ${flag_var} "${${flag_var}}")
      endif(${flag_var} MATCHES "WIN32")
    endforeach(flag_var)
          
    file(GLOB LibXML2_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2_64.lib)
    file(GLOB LibXML2_DEBUG_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2_64d.lib)
  else(CMAKE_CL_64)
    file(GLOB LibXML2_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2.lib)
    file(GLOB LibXML2_DEBUG_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2d.lib)
  endif(CMAKE_CL_64)
  add_library(libxml2 STATIC IMPORTED)
  set(LibXML2_INCLUDE_DIRS ../win32/libxml2-2.7.7/include )
  set_property(TARGET libxml2 PROPERTY IMPORTED_LOCATION_RELEASE ${LibXML2_LIBRARIES})
  set_property(TARGET libxml2 PROPERTY IMPORTED_LOCATION_DEBUG ${LibXML2_DEBUG_LIBRARIES})
endif(WIN32)

if(UNIX)
  execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(LINUX_LIBRARIES pthread)
  endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)
project (agent)

set(agent_srcs cppagent.cpp 
	       adapter.cpp
	       asset.cpp
	       agent.cpp 
	       checkpoint.cpp
	       component.cpp 
	       component_event.cpp 
	       change_observer.cpp
	       connector.cpp
           cutting_tool.cpp
	       data_item.cpp 
	       device.cpp 
	       globals.cpp 
	       options.cpp
	       service.cpp
	       config.cpp
           ref_counted.cpp
           version.cpp
	       xml_parser.cpp 
	       xml_printer.cpp)

file(GLOB agent_headers *.hpp)

# configure a header file to pass some of the CMake settings
# to the source code
add_custom_target(UPDATE_VERSION ALL
                  COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${PROJECT_SOURCE_DIR}/version.cpp")

configure_file (
  "${PROJECT_SOURCE_DIR}/version.h.in"
  "${PROJECT_BINARY_DIR}/version.h"
  )
  
include_directories(../lib)

find_package(LibXML2 REQUIRED)
if(UNIX)
  add_library(libxml2 UNKNOWN IMPORTED)
  set_property(TARGET libxml2 PROPERTY IMPORTED_LOCATION ${LibXML2_LIBRARIES})
endif(UNIX)


add_definitions(-DDLIB_NO_GUI_SUPPORT ${LibXML2_DEFINITIONS})
include_directories(${LibXML2_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})

add_executable(agent ${agent_srcs} ${agent_headers})
target_link_libraries(agent ${LINUX_LIBRARIES} libxml2)

if(WIN32)
  foreach(flag_var
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
     if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
     endif(${flag_var} MATCHES "/MD")
  endforeach(flag_var)
  if(NOT CMAKE_CL_64)
    string(REGEX REPLACE "/STACK:[0-9]+" "/STACK:65536" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  endif(NOT CMAKE_CL_64)
endif(WIN32)

INSTALL(TARGETS agent RUNTIME DESTINATION bin)
add_dependencies(agent UPDATE_VERSION)
