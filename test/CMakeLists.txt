cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../agent/CMake;${CMAKE_MODULE_PATH}")

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
set(CMAKE_FIND_APPBUNDLE NEVER)

set(GCOV_COMPILE_FLAGS  "-fprofile-arcs -ftest-coverage")
set(GCOV_LINK_FLAGS "-lgcov")

if(WIN32)
  set(LibXML2_INCLUDE_DIRS ../win32/libxml2-2.9/include )
  
  if(CMAKE_CL_64)
    set(bits 64)
  else(CMAKE_CL_64)
  set(bits 32)
  endif(CMAKE_CL_64)
  
  file(GLOB LibXML2_LIBRARIES "../win32/libxml2-2.9/lib/libxml2_a_v120_${bits}.lib")
  file(GLOB LibXML2_DEBUG_LIBRARIES ../win32/libxml2-2.9/lib/libxml2d_a_v120_${bits}.lib)

  set(CPPUNIT_INCLUDE_DIR ../win32/cppunit-1.12.1/include)
  file(GLOB CPPUNIT_LIBRARY ../win32/cppunit-1.12.1/lib/cppunitd_v120_a.lib)
endif(WIN32)

if(UNIX)
  execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(LINUX_LIBRARIES pthread)
  endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)

project (test)

set(test_srcs test.cpp
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
 	       asset_test.cpp
	       change_observer_test.cpp
           cutting_tool_test.cpp
           )

set(agent_srcs ../agent/adapter.cpp 
	       ../agent/agent.cpp 
	       ../agent/checkpoint.cpp
	       ../agent/component.cpp 
	       ../agent/component_event.cpp 
	       ../agent/change_observer.cpp
	       ../agent/connector.cpp
           ../agent/cutting_tool.cpp
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
          ../agent/version.cpp
          ../agent/rolling_file_logger.cpp
          )

set_property(SOURCE ${agent_srcs} APPEND PROPERTY COMPILE_FLAGS ${GCOV_COMPILE_FLAGS})


file(GLOB test_headers *.hpp ../agent/*.hpp)

include_directories(../lib ../agent .)

find_package(CppUnit REQUIRED)
find_package(LibXML2 REQUIRED)

add_definitions(-DDLIB_NO_GUI_SUPPORT ${LibXML2_DEFINITIONS})
include_directories(${LibXML2_INCLUDE_DIRS} ${CPPUNIT_INCLUDE_DIR})

if(WIN32)
  set(WINVER "0x0501" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
  
  foreach(flag_var
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
     if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD[d]?" "/MTd" ${flag_var} "${${flag_var}}")
     endif(${flag_var} MATCHES "/MD")
  endforeach(flag_var)
endif(WIN32)

add_executable(agent_test ${agent_srcs} ${test_srcs} ${test_headers})
target_link_libraries(agent_test ${LibXML2_LIBRARIES} ${CPPUNIT_LIBRARY} ${LINUX_LIBRARIES} ${GCOV_LINK_FLAGS})  
set_target_properties(agent_test PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1)

add_custom_target( cov 
				COMMAND [ -d Coverage ]&&rm -rf Coverage/||echo "Creating new folder"
				COMMAND mkdir Coverage
				COMMAND agent_test
				COMMAND cp CMakeFiles/agent_test.dir/__/agent/*.gcno Coverage/
				COMMAND mv CMakeFiles/agent_test.dir/__/agent/*.gcda Coverage/
				COMMAND cd Coverage&&lcov -t "result" -o cppagent_coverage.info -c -d .
				COMMAND cd Coverage&&genhtml -o coverage cppagent_coverage.info
				COMMENT "Generating Coverage Report ..."
                )