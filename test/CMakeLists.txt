# Specify the build type on single-configuration generators such as Ninja and Makefile
set(CMAKE_BUILD_TYPE DEBUG)

if(NOT APPVEYOR AND (WIN32 OR APPLE))
  add_subdirectory(../CUTI "${CMAKE_BINARY_DIR}/cuti")
endif()

  
if(WIN32)
  set(WINDOWS_LIBRARIES shlwapi)
elseif(UNIX)
  execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(LINUX_LIBRARIES pthread)
  endif()
endif()

set(APPVEYOR OFF CACHE STRING "Set for AppVeyor builds")

file(GLOB TEST_SRCS *.cpp)
file(GLOB TEST_HEADERS *.hpp)

list(REMOVE_ITEM TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/test.cpp")

set(AGENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
      
file(GLOB AGENT_SRCS "${AGENT_DIR}/*.cpp")
file(GLOB AGENT_HEADERS "${AGENT_DIR}/*.hpp")


# Allow better viewing and grouping of files in Visual Studio by defining source groups
source_group("Test Headers Files" FILES ${TEST_HEADERS})
source_group("Test Source Files" FILES ${TEST_SRCS})
source_group("Resource Files" FILES ${RESOURCE_FILE})
source_group("Dlib Source Files" FILES ${DLIB_FIX_SRCS})

if(NOT APPVEYOR AND (WIN32 OR APPLE))
  cuti_creates_test_target(agent_test agent_lib ${TEST_SRCS} ${TEST_HEADERS} ${DLIB_FIX_SRCS})
  target_compile_definitions(agent_test  PRIVATE CUTI_CPPUNIT_COMPATABILITY)
else()
  add_executable(agent_test ${TEST_SRCS} test.cpp ${TEST_HEADERS} ${DLIB_FIX_SRCS})
  target_compile_definitions(agent_test  PRIVATE CUTI_NO_INTEGRATION)
  target_link_libraries(agent_test PRIVATE agent_lib)
  AddCppUnitSupport(agent_test)
endif()

target_compile_definitions(agent_test PRIVATE "PROJECT_ROOT=${CMAKE_SOURCE_DIR}"
    NOMINMAX "TEST_BIN_ROOT=$<TARGET_FILE_DIR:agent_test>")

AddMsvcXPSupport(agent_test)
AddLibXML2Support(agent_test)
AddDLibSupport(agent_test)
AddDateSupport(agent_test)
AddJsonSupport(agent_test)

target_include_directories(agent_test PRIVATE "${AGENT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
    "../CUTI/include")
  
target_link_libraries(agent_test PRIVATE ${LINUX_LIBRARIES} ${WINDOWS_LIBRARIES} ${GCOV_LINK_FLAGS})

set_target_properties(agent_test PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1)

target_compile_features(agent_test PRIVATE cxx_std_14)

add_custom_command(
  TARGET agent_test POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/probe.xml"
  "$<TARGET_FILE_DIR:agent_test>/../Resources/probe.xml"
  COMMENT "Copying to output directory")
