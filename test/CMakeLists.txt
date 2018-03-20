set(CMAKE_BUILD_TYPE DEBUG) # Specify the build type on single-configuration generators such as Ninja and Makefile

if(WIN32)
  set(WINDOWS_LIBRARIES shlwapi)
endif(WIN32)
if(UNIX)
	execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
	if(CMAKE_SYSTEM_NAME MATCHES Linux)
		set(LINUX_LIBRARIES pthread)
	endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)

if (APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef -Wno-deprecated-declarations")
endif(APPLE)


set(TEST_SRCS
	test.cpp
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

set(AGENT_SRCS
	../agent/adapter.cpp
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
	../agent/asset.cpp
	../agent/version.cpp
	../agent/rolling_file_logger.cpp
	)

set_property(SOURCE ${AGENT_SRCS} APPEND PROPERTY COMPILE_FLAGS ${GCOV_COMPILE_FLAGS})

file(GLOB TEST_HEADERS *.hpp ../agent/*.hpp)

# Allow better viewing and grouping of files in Visual Studio by defining source groups
source_group("Headers Files" FILES ${TEST_HEADERS})
source_group("Source Files" FILES ${TEST_SRCS} ${AGENT_SRCS})
source_group("Resource Files" FILES ${RESOURCE_FILE})

add_executable(agent_test
	${AGENT_SRCS}
	${TEST_SRCS}
	${TEST_HEADERS})

AddMsvcXPSupport(agent_test)
AddLibXML2Support(agent_test)
AddCppUnitSupport(agent_test)
AddDLibSupport(agent_test)

target_include_directories(agent_test
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../agent
	PRIVATE .
	)

target_link_libraries(agent_test PRIVATE ${LINUX_LIBRARIES} PRIVATE ${WINDOWS_LIBRARIES} PRIVATE ${GCOV_LINK_FLAGS})
set_target_properties(agent_test PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1)

SetVisualStudioWorkingDirectory(agent_test ${CMAKE_SOURCE_DIR}/test)

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