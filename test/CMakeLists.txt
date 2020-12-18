include(GoogleTest)

# Include googletest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.10.0.zip
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  set(BUILD_GMOCK "ON")
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()


# Define test cases
macro(add_agent_test AGENT_TEST_NAME ADD_TEST_HELPER)
  set(_sources)
  list(APPEND _sources ${AGENT_TEST_NAME}_test.cpp)
  list(APPEND _sources "${googletest_SOURCE_DIR}/googletest/src/gtest_main.cc")
  if(${ADD_TEST_HELPER})
    list(APPEND _sources test_globals.cpp agent_test_helper.cpp)
  endif()
  add_executable(${AGENT_TEST_NAME}_test ${_sources})
  # set_property(TARGET ${AGENT_TEST_NAME}_test PROPERTY LINK_FLAGS_DEBUG "${COVERAGE_FLAGS}")
  target_link_libraries(${AGENT_TEST_NAME}_test agent_lib gtest)
  target_include_directories(${AGENT_TEST_NAME}_test
    PRIVATE "${googletest_SOURCE_DIR}/googletest/include")

  target_compile_definitions(${AGENT_TEST_NAME}_test
    PRIVATE "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\""
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:${AGENT_TEST_NAME}_test>/../Resources\"")
  target_clangtidy_setup(agent_lib)
  add_test(NAME ${AGENT_TEST_NAME}_test COMMAND ${AGENT_TEST_NAME}_test)

  gtest_discover_tests(${AGENT_TEST_NAME}_test)
endmacro()

add_agent_test(agent TRUE)
add_agent_test(agent_device TRUE)
add_agent_test(adapter FALSE)
add_agent_test(config FALSE)
add_agent_test(change_observer FALSE)
add_agent_test(checkpoint FALSE)
add_agent_test(component FALSE)
add_agent_test(composition TRUE)
add_agent_test(connector FALSE)
add_agent_test(coordinate_system TRUE)
add_agent_test(data_item FALSE)
add_agent_test(data_set TRUE)
add_agent_test(device FALSE)
add_agent_test(entity TRUE)
add_agent_test(entity_parser TRUE)
add_agent_test(entity_printer TRUE)
add_agent_test(file_asset TRUE)
add_agent_test(globals FALSE)
add_agent_test(json_printer_asset TRUE)
add_agent_test(json_printer_error TRUE)
add_agent_test(json_printer_probe TRUE)
add_agent_test(json_printer_stream TRUE)
add_agent_test(kinematics TRUE)
add_agent_test(observation TRUE)
add_agent_test(relationship TRUE)
add_agent_test(solid_model TRUE)
add_agent_test(specification TRUE)
add_agent_test(table TRUE)
add_agent_test(xml_parser TRUE)
add_agent_test(xml_printer TRUE)

# TODO Reorganize data. Do not copy files around. Unit test could be run only with sources in place.
add_custom_command(
  TARGET adapter_test POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/probe.xml"
  "$<TARGET_FILE_DIR:adapter_test>/../Resources/probe.xml"
  COMMENT "Copying to output directory")
