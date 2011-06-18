# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../agent/CMake;${CMAKE_MODULE_PATH}")

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
set(CMAKE_FIND_APPBUNDLE NEVER)

if(WIN32)
  set(LibXML2_INCLUDE_DIRS ../win32/libxml2-2.7.7/include )
  file(GLOB LibXML2_LIBRARIES ../win32/libxml2-2.7.7/lib/libxml2.lib )
  set(CPPUNIT_INCLUDE_DIR ../win32/cppunit-1.12.1/include)
  file(GLOB CPPUNIT_LIBRARY ../win32/cppunit-1.12.1/lib/cppunitd.lib)
endif(WIN32)

if(UNIX)
  execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(LINUX_LIBRARIES pthread)
  endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)

project (test)

set(test_srcs test.cpp
	       ../agent/adapter.cpp 
	       ../agent/agent.cpp 
	       ../agent/checkpoint.cpp
	       ../agent/component.cpp 
	       ../agent/component_event.cpp 
	       ../agent/connector.cpp
	       ../agent/data_item.cpp 
	       ../agent/device.cpp 
	       ../agent/globals.cpp 
	       ../agent/options.cpp
	       ../agent/xml_parser.cpp 
	       ../agent/xml_printer.cpp
	       ../agent/config.cpp
	       ../agent/service.cpp
	       ../agent/ref_counted.cpp
           ../agent/asset.cpp
	       adapter_test.cpp
	       agent_test.cpp
	       checkpoint_test.cpp
	       config_test.cpp
	       component_test.cpp
	       component_event_test.cpp
	       connector_test.cpp
	       data_item_test.cpp
	       device_test.cpp
	       globals_test.cpp
	       xml_parser_test.cpp
	       test_globals.cpp
	       xml_printer_test.cpp
           asset_test.cpp)

file(GLOB test_headers *.hpp ../agent/*.hpp)

include_directories(../lib ../agent)

find_package(CppUnit REQUIRED)
find_package(LibXML2 REQUIRED)

add_definitions(-DDLIB_NO_GUI_SUPPORT ${LibXML2_DEFINITIONS})
include_directories(${LibXML2_INCLUDE_DIRS} ${CPPUNIT_INCLUDE_DIR})

add_executable(test ${test_srcs} ${test_headers})
target_link_libraries(test ${LibXML2_LIBRARIES} ${CPPUNIT_LIBRARY} ${LINUX_LIBRARIES})
