# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../agent/CMake;${CMAKE_MODULE_PATH}")

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
set(CMAKE_FIND_APPBUNDLE NEVER)

project (test)

set(test_srcs test.cpp
	       ../agent/adapter.cpp 
	       ../agent/agent.cpp 
	       ../agent/component.cpp 
	       ../agent/component_event.cpp 
	       ../agent/connector.cpp
	       ../agent/data_item.cpp 
	       ../agent/device.cpp 
	       ../agent/globals.cpp 
	       ../agent/options.cpp
	       ../agent/xml_parser.cpp 
	       ../agent/xml_printer.cpp
               ../agent/checkpoint.cpp
	       agent_test.cpp
	       component_test.cpp
	       component_event_test.cpp
	       data_item_test.cpp
	       device_test.cpp
	       globals_test.cpp
	       xml_parser_test.cpp
	       test_globals.cpp
	       xml_printer_test.cpp
               checkpoint_test.cpp)


include_directories(../lib ../agent)

find_package(CppUnit REQUIRED)
find_package(LibXML++ REQUIRED)

add_definitions(-DDLIB_NO_GUI_SUPPORT ${LibXML2_DEFINITIONS})
include_directories(${LibXML++_INCLUDE_DIRS} ${Glib_INCLUDE_DIRS} ${Glibmm_INCLUDE_DIRS} 
                    			  ${LibXML2_INCLUDE_DIRS} ${SigC++_INCLUDE_DIRS}
					  ${CPPUNIT_INCLUDE_DIR})

add_executable(test ${test_srcs})
target_link_libraries(test ${LibXML++_LIBRARIES} ${Glibmm_LIBRARIES} 
			    ${LibXML2_LIBRARIES} ${CPPUNIT_LIBRARY})


