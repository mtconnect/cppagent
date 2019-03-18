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

file(GLOB TEST_SRCS *.cpp)
file(GLOB TEST_HEADERS *.hpp)

set(AGENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
      
file(GLOB AGENT_SRCS ${AGENT_DIR}/*.cpp)
file(GLOB AGENT_HEADERS ${AGENT_DIR}/*.hpp)

list(REMOVE_ITEM AGENT_SRCS ${AGENT_DIR}/cppagent.cpp)

set_property(SOURCE ${AGENT_SRCS} APPEND PROPERTY COMPILE_FLAGS ${GCOV_COMPILE_FLAGS})

# Allow better viewing and grouping of files in Visual Studio by defining source groups
source_group("Agent Source Files" FILES ${AGENT_SRCS})
source_group("Agent Header Files" FILES ${AGENT_HEADERS})
source_group("Test Headers Files" FILES ${TEST_HEADERS})
source_group("Test Source Files" FILES ${TEST_SRCS})
source_group("Resource Files" FILES ${RESOURCE_FILE})
source_group("Dlib Source Files" FILES ${DLIB_FIX_SRCS})

add_executable(agent_test
  ${AGENT_SRCS}
  ${DLIB_FIX_SRCS}
  ${AGENT_HEADERS}
  ${TEST_SRCS}
  ${TEST_HEADERS})

AddMsvcXPSupport(agent_test)
AddLibXML2Support(agent_test)
AddCppUnitSupport(agent_test)
AddDLibSupport(agent_test)
AddDateSupport(agent_test)
AddJsonSupport(agent_test)

target_include_directories(agent_test
  PRIVATE ${AGENT_DIR}
  PRIVATE .
  )

target_link_libraries(agent_test PRIVATE ${LINUX_LIBRARIES} PRIVATE ${WINDOWS_LIBRARIES} PRIVATE ${GCOV_LINK_FLAGS})

set_target_properties(agent_test PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1)

target_compile_features(agent_test PRIVATE cxx_std_14)

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
