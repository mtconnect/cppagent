# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
set(CMAKE_FIND_APPBUNDLE NEVER)


if(WIN32)
  add_library(libxml2 STATIC IMPORTED)
  set(LibXML2_INCLUDE_DIRS ../win32/libxml2-2.7.7/include )
  file(GLOB LibXML2_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2.lib)
  file(GLOB LibXML2_DEBUG_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2d.lib)
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
	       agent.cpp 
	       checkpoint.cpp
	       component.cpp 
	       component_event.cpp 
	       connector.cpp
	       data_item.cpp 
	       device.cpp 
	       globals.cpp 
	       options.cpp
	       service.cpp
	       config.cpp
	       xml_parser.cpp 
	       xml_printer.cpp)

include_directories(../lib)

find_package(LibXML2 REQUIRED)
if(UNIX)
  add_library(libxml2 UNKNOWN IMPORTED)
  set_property(TARGET libxml2 PROPERTY IMPORTED_LOCATION ${LibXML2_LIBRARIES})
endif(UNIX)


add_definitions(-DDLIB_NO_GUI_SUPPORT ${LibXML2_DEFINITIONS})
include_directories(${LibXML2_INCLUDE_DIRS})

add_executable(agent ${agent_srcs})
target_link_libraries(agent ${LINUX_LIBRARIES} libxml2)

if(WIN32)
  foreach(flag_var
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
     if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
     endif(${flag_var} MATCHES "/MD")
  endforeach(flag_var)
endif(WIN32)

INSTALL(TARGETS agent RUNTIME DESTINATION bin)
