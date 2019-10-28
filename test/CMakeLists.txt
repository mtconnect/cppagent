# Include googletest
set(BUILD_GMOCK "ON")
set(INSTALL_GTEST "OFF")
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../googletest" "googletest")

# Define test cases
macro(add_agent_test AGENT_TEST_NAME ADD_TEST_HELPER)
  set(_sources)
  list(APPEND _sources ${AGENT_TEST_NAME}_test.cpp)
  list(APPEND _sources "${CMAKE_CURRENT_LIST_DIR}/../googletest/googletest/src/gtest_main.cc")
  if(${ADD_TEST_HELPER})
    list(APPEND _sources test_globals.cpp agent_test_helper.cpp)
  endif()
  add_executable(${AGENT_TEST_NAME}_test ${_sources})
  target_link_libraries(${AGENT_TEST_NAME}_test agent_lib gtest)
  target_include_directories(${AGENT_TEST_NAME}_test
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../googletest/googletest/include")

  # TODO Move include directories for dlib, libxml and date to the respective libraries.
  target_include_directories(agent_lib
    PUBLIC  "${CMAKE_CURRENT_LIST_DIR}/../date/include"
            "${CMAKE_CURRENT_LIST_DIR}/../dlib"
            "${CMAKE_CURRENT_LIST_DIR}/../libxml2/include"
            "${CMAKE_CURRENT_LIST_DIR}/../libxml2_make/include"
            "${CMAKE_CURRENT_LIST_DIR}/../json/include")

  target_compile_definitions(${AGENT_TEST_NAME}_test
    PRIVATE "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\"" NOMINMAX
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:${AGENT_TEST_NAME}_test>/../Resources\"")
  add_test(NAME ${AGENT_TEST_NAME}_test COMMAND ${AGENT_TEST_NAME}_test)
endmacro()

add_agent_test(agent TRUE)
add_agent_test(adapter FALSE)
add_agent_test(config FALSE)
add_agent_test(change_observer FALSE)
add_agent_test(checkpoint FALSE)
add_agent_test(component FALSE)
add_agent_test(connector FALSE)
add_agent_test(data_item FALSE)
add_agent_test(data_set TRUE)
add_agent_test(device FALSE)
add_agent_test(globals FALSE)
add_agent_test(json_printer_asset TRUE)
add_agent_test(json_printer_error TRUE)
add_agent_test(json_printer_probe TRUE)
add_agent_test(json_printer_stream TRUE)
add_agent_test(observation TRUE)
add_agent_test(xml_parser TRUE)

# TODO Reorganize data. Do not copy files around. Unit test could be run only with sources in place.
add_custom_command(
  TARGET adapter_test POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/probe.xml"
  "$<TARGET_FILE_DIR:adapter_test>/../Resources/probe.xml"
  COMMENT "Copying to output directory")
