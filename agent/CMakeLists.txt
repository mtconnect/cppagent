
# The version number.
set (AGENT_VERSION_MAJOR 1)
set (AGENT_VERSION_MINOR 4)
set (AGENT_VERSION_PATCH 0)
set (AGENT_VERSION_BUILD 9)
set (AGENT_VERSION_RC "RC9")

if(WIN32)
	# Version info
	string(TIMESTAMP AGENT_VERSION_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
	set(RESOURCE_FILE "${PROJECT_BINARY_DIR}/agent/version.rc")
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
		${RESOURCE_FILE} )

	set(WINDOWS_LIBRARIES shlwapi)
  
else()
	set(RESOURCE_FILE "")
endif()

if(UNIX)
	execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
	if(CMAKE_SYSTEM_NAME MATCHES Linux)
		set(LINUX_LIBRARIES pthread)
	endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)

if (APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef -Wno-deprecated-declarations")
endif(APPLE)


set(AGENT_SRCS cppagent.cpp 
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
	xml_printer.cpp
	rolling_file_logger.cpp
	)

file(GLOB AGENT_HEADERS *.hpp)

# configure a header file to pass some of the CMake settings
# to the source code
add_custom_target(UPDATE_VERSION ALL
				  COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_CURRENT_SOURCE_DIR}/version.cpp")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
	"${PROJECT_BINARY_DIR}/agent/version.h" )

# Allow better viewing and grouping of files in Visual Studio by defining source groups
source_group("Headers Files" FILES ${AGENT_HEADERS})
source_group("Source Files" FILES ${AGENT_SRCS})
source_group("Resource Files" FILES ${RESOURCE_FILE})

add_executable(agent
	${AGENT_SRCS}
	${AGENT_HEADERS}
	${RESOURCE_FILE})

AddMsvcXPSupport(agent)
AddLibXML2Support(agent)
AddDLibSupport(agent)

target_include_directories(agent
	PRIVATE ${PROJECT_BINARY_DIR}/agent
	)

target_link_libraries(agent PRIVATE ${LINUX_LIBRARIES} PRIVATE ${WINDOWS_LIBRARIES})

add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}" "/DLIBXML_STATIC")

INSTALL(TARGETS agent RUNTIME DESTINATION bin)
add_dependencies(agent UPDATE_VERSION)
