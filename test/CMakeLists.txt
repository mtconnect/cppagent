# Include googletest
set(BUILD_GMOCK "ON")
set(INSTALL_GTEST "OFF")
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../googletest" "googletest")

# Define test cases
macro(add_agent_test AGENT_TEST_NAME)
  add_executable(${AGENT_TEST_NAME}_test ${AGENT_TEST_NAME}_test.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../googletest/googletest/src/gtest_main.cc")
  target_link_libraries(${AGENT_TEST_NAME}_test agent_lib gtest)
  target_include_directories(${AGENT_TEST_NAME}_test
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../googletest/googletest/include")

  # TODO Move include directories for dlib, libxml and date to the respective libraries.
  target_include_directories(agent_lib
    PUBLIC  "${CMAKE_CURRENT_LIST_DIR}/../date/include"
            "${CMAKE_CURRENT_LIST_DIR}/../dlib"
            "${CMAKE_CURRENT_LIST_DIR}/../libxml2/include"
            "${CMAKE_CURRENT_LIST_DIR}/../libxml2_make/include")

  target_compile_definitions(${AGENT_TEST_NAME}_test
    PRIVATE "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\"" NOMINMAX
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:${AGENT_TEST_NAME}_test>/../Resources\"")
  add_test(NAME ${AGENT_TEST_NAME}_test COMMAND ${AGENT_TEST_NAME}_test)
endmacro()

add_agent_test(adapter)
add_agent_test(config)
add_agent_test(change_observer)
