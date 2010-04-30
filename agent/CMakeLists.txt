# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
set(CMAKE_FIND_APPBUNDLE NEVER)

if(WIN32)
  set(LibXML2_INCLUDE_DIRS ../win32/libxml2-2.7.3/include )
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
	       xml_parser.cpp 
	       xml_printer.cpp)

include_directories(../lib)

find_package(LibXML2 REQUIRED)

add_definitions(-DDLIB_NO_GUI_SUPPORT ${LibXML2_DEFINITIONS})
include_directories(${LibXML2_INCLUDE_DIRS})

add_executable(agent ${agent_srcs})
target_link_libraries(agent ${LibXML2_LIBRARIES})
