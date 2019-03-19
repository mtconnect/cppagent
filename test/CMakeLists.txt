set(CMAKE_BUILD_TYPE DEBUG) # Specify the build type on single-configuration generators such as Ninja and Makefile

add_subdirectory(../CUTI ${CMAKE_BINARY_DIR}/cuti)

if(WIN32)
  set(WINDOWS_LIBRARIES shlwapi)
endif(WIN32)
if(UNIX)
	execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
	if(CMAKE_SYSTEM_NAME MATCHES Linux)
		set(LINUX_LIBRARIES pthread)
	endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)

#file(GLOB TEST_SRCS *.cpp)
file(GLOB TEST_HEADERS *.hpp)

set(APPVEYOR OFF CACHE STRING "Set for AppVeyor builds")

set(TEST_SRCS json_printer_error_test.cpp json_printer_probe_test.cpp)

set(AGENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
      
file(GLOB AGENT_SRCS ${AGENT_DIR}/*.cpp)
file(GLOB AGENT_HEADERS ${AGENT_DIR}/*.hpp)

list(REMOVE_ITEM TEST_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp)

#set_property(SOURCE ${AGENT_SRCS} APPEND PROPERTY COMPILE_FLAGS ${GCOV_COMPILE_FLAGS})

# Allow better viewing and grouping of files in Visual Studio by defining source groups
source_group("Agent Source Files" FILES ${AGENT_SRCS})
source_group("Agent Header Files" FILES ${AGENT_HEADERS})
source_group("Test Headers Files" FILES ${TEST_HEADERS})
source_group("Test Source Files" FILES ${TEST_SRCS})
source_group("Resource Files" FILES ${RESOURCE_FILE})
source_group("Dlib Source Files" FILES ${DLIB_FIX_SRCS})

if(NOT APPVEYOR AND (WIN32 OR APPLE))
  cuti_creates_test_target(agent_test agent_lib ${TEST_SRCS} ${TEST_HEADERS})
  target_compile_definitions(agent_test  PRIVATE CUTI_CPPUNIT_COMPATABILITY)
else(NOT APPVEYOR AND (WIN32 OR APPLE))
  add_executable(agent_test ${TEST_SRCS} test.cpp ${TEST_HEADERS} )
  target_compile_definitions(agent_test  PRIVATE CUTI_NO_INTEGRATION )
  target_link_libraries(agent_test PRIVATE agent_lib)
  AddCppUnitSupport(agent_test)
endif(NOT APPVEYOR AND (WIN32 OR APPLE))

target_compile_definitions(agent_test  PRIVATE "PROJECT_ROOT=${CMAKE_SOURCE_DIR}" PRIVATE NOMINMAX)

AddMsvcXPSupport(agent_test)
AddLibXML2Support(agent_test)
AddDLibSupport(agent_test)
AddDateSupport(agent_test)
AddJsonSupport(agent_test)

target_include_directories(agent_test
  PRIVATE ${AGENT_DIR}
  PRIVATE .
  PRIVATE 
  )

target_include_directories(agent_test PRIVATE ../CUTI/include)  
  
target_link_libraries(agent_test PRIVATE ${LINUX_LIBRARIES} PRIVATE ${WINDOWS_LIBRARIES} PRIVATE ${GCOV_LINK_FLAGS})

set_target_properties(agent_test PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1)

target_compile_features(agent_test PRIVATE cxx_std_14)


