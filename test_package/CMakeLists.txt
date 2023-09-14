cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

option(DEVELOPMENT "Used for development, includes tests as a subdirectory instead of a package" OFF)
message(STATUS "Development: ${DEVELOPMENT}")

if(NOT DEVELOPMENT)
  project(mtconnect_agent_tests LANGUAGES CXX)
endif()

set(USE_FOLDERS ON)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

if (DEVELOPMENT)
  file(REAL_PATH ${CMAKE_SOURCE_DIR} project_root_dir)
  file(REAL_PATH "${CMAKE_SOURCE_DIR}/test_package/resources" test_resource_dir)
else()
  include(../cmake/remove_link_directories.cmake)
  include(../cmake/osx_no_app_or_frameworks.cmake)
  include(../cmake/ClangFormat.cmake)
  include(../cmake/ClangTidy.cmake)

  find_package(Boost REQUIRED  GLOBAL)
  find_package(LibXml2 REQUIRED  GLOBAL)
  find_package(date REQUIRED  GLOBAL)
  find_package(OpenSSL REQUIRED  GLOBAL)
  find_package(nlohmann_json REQUIRED GLOBAL)
  find_package(mqtt_cpp REQUIRED GLOBAL)
  find_package(RapidJSON REQUIRED GLOBAL)
  find_package(mtconnect_agent REQUIRED GLOBAL)

  enable_testing()

  file(REAL_PATH "${CMAKE_SOURCE_DIR}/.." project_root_dir)
  file(REAL_PATH "${CMAKE_SOURCE_DIR}/resources" test_resource_dir)
endif()

find_package(GTest REQUIRED GLOBAL)
include(GoogleTest)

message(STATUS "Project Root: ${project_root_dir}")
message(STATUS "Test Resources: ${test_resource_dir}")

set(COMMON_DEFINITIONS 
  "PROJECT_ROOT_DIR=\"${project_root_dir}\";BOOST_FILESYSTEM_VERSION=3;$<$<BOOL:$ENV{APPVEYOR}>:APPVEYOR>;$<$<PLATFORM_ID:Windows>:NOMINMAX>;$<$<PLATFORM_ID:Windows>:WINVER=${WINVER}>;$<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${WINVER}>;TEST_RESOURCE_DIR=\"${test_resource_dir}\"")

message(STATUS "Common definitions: ${COMMON_DEFINITIONS}")

#### Sink Plugin Test

list(REMOVE_ITEM CMAKE_INCLUDE_PATH "include")

add_library(sink_plugin_test SHARED testsink_service.cpp testsink_service.hpp)
target_include_directories(
  sink_plugin_test
  PUBLIC
  "${CMAKE_INCLUDE_PATH}"
  )

target_compile_definitions(sink_plugin_test
  PRIVATE
  ${COMMON_DEFINITIONS}
  "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:sink_plugin_test>/../Resources\"")

target_compile_features(sink_plugin_test
  PRIVATE
  ${CXX_COMPILE_FEATURES}
  )

if(DEVELOPMENT)
  target_link_libraries(
    sink_plugin_test
    PRIVATE
    agent_lib
    $<$<PLATFORM_ID:Windows>:shlwapi>
    )
else()
  target_link_libraries(
    sink_plugin_test
    PRIVATE
    mtconnect_agent::mtconnect_agent
    $<$<PLATFORM_ID:Windows>:shlwapi>
    )
endif()
target_clangformat_setup(sink_plugin_test)

set_target_properties(sink_plugin_test PROPERTIES FOLDER "test/sink")

if (APPLE)
  set_target_properties(sink_plugin_test PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER sink-plugin-test
    VERSION 16.4.0
    SOVERSION 1.0.0)
endif()

#### Adapter Plugin Test

add_library(adapter_plugin_test SHARED testadapter_service.cpp testadapter_service.hpp)

target_include_directories(
  adapter_plugin_test
  PUBLIC
    "${CMAKE_INCLUDE_PATH}"
    )
  
target_compile_definitions(adapter_plugin_test
  PRIVATE
  "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:adapter_plugin_test>/../Resources\""  
  ${COMMON_DEFINITIONS})

if (APPLE)
  set_target_properties(adapter_plugin_test PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER adapter-plugin-test
    VERSION 16.4.0
    SOVERSION 1.0.0)
endif()

target_compile_features(adapter_plugin_test PUBLIC ${CXX_COMPILE_FEATURES})

if(DEVELOPMENT)
  target_link_libraries(
    adapter_plugin_test
    PRIVATE
    agent_lib
    $<$<PLATFORM_ID:Windows>:shlwapi>
    )
else()
  target_link_libraries(
    adapter_plugin_test
    PRIVATE
    mtconnect_agent::mtconnect_agent
    $<$<PLATFORM_ID:Windows>:shlwapi>
    )
endif()

set_target_properties(adapter_plugin_test PROPERTIES FOLDER "test/adapter")

target_clangformat_setup(adapter_plugin_test)

##### Agent Test Helper

set(AGENT_TEST_LIB_SOURCES agent_test_helper.cpp agent_test_helper.hpp test_utilities.hpp json_helper.hpp)
add_library(agent_test_lib STATIC ${AGENT_TEST_LIB_SOURCES})
target_include_directories(
  agent_test_lib
  PUBLIC
    "${CMAKE_INCLUDE_PATH}"
    )

if(DEVELOPMENT)
  target_link_libraries(
    agent_test_lib
    PUBLIC
    agent_lib
    GTest::GTest
    )
else()
  target_link_libraries(
    agent_test_lib
    PUBLIC
    mtconnect_agent::mtconnect_agent
    GTest::GTest
    )
endif()

target_compile_definitions(agent_test_lib PUBLIC ${COMMON_DEFINITIONS})
target_clangtidy_setup(agent_test_lib)
target_clangformat_setup(agent_test_lib)
set_target_properties(agent_test_lib PROPERTIES FOLDER "test")

#### Define test cases macro

macro(add_agent_test AGENT_TEST_NAME ADD_TEST_HELPER SUB_FOLDER)
  set(_sources ${AGENT_TEST_NAME}_test.cpp)
  add_executable(${AGENT_TEST_NAME}_test ${_sources})
  # set_property(TARGET ${AGENT_TEST_NAME}_test PROPERTY LINK_FLAGS_DEBUG "${COVERAGE_FLAGS}")
  target_link_libraries(${AGENT_TEST_NAME}_test agent_test_lib  
    $<$<PLATFORM_ID:Linux>:pthread>
    $<$<PLATFORM_ID:Windows>:bcrypt>)

  target_compile_definitions(${AGENT_TEST_NAME}_test
    PRIVATE 
    ${COMMON_DEFINITIONS}
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:${AGENT_TEST_NAME}_test>/../Resources\"")
  target_compile_features(${AGENT_TEST_NAME}_test PUBLIC ${CXX_COMPILE_FEATURES})

  # Organize into folders
  set_target_properties(${AGENT_TEST_NAME}_test PROPERTIES FOLDER "test/${SUB_FOLDER}")

  gtest_discover_tests(${AGENT_TEST_NAME}_test DISCOVERY_MODE PRE_TEST)

  if(MSVC AND ${ARGC} GREATER 3)
    message(info ": Setting /bigobj for ${_sources}")
    set_property(SOURCE
      ${_sources}
      PROPERTY COMPILE_FLAGS "/bigobj")   
  endif()

  target_clangformat_setup(${AGENT_TEST_NAME}_test)  
endmacro()

add_agent_test(asset TRUE asset)
add_agent_test(file_asset TRUE asset)
add_agent_test(cutting_tool TRUE asset)
add_agent_test(raw_material TRUE asset)
add_agent_test(qif_document TRUE asset)
add_agent_test(asset_buffer TRUE asset)
add_agent_test(component_parameters TRUE asset)
add_agent_test(asset_hash TRUE asset)

add_agent_test(agent_device TRUE device_model)
add_agent_test(component FALSE device_model)
add_agent_test(composition TRUE device_model)
add_agent_test(device FALSE device_model)
add_agent_test(references TRUE device_model)

add_agent_test(data_item FALSE device_model/data_item)
add_agent_test(unit_conversion FALSE device_model/data_item)

add_agent_test(coordinate_system TRUE device_model/configuration)
add_agent_test(image_file TRUE device_model/configuration)
add_agent_test(kinematics TRUE device_model/configuration)
add_agent_test(relationship TRUE device_model/configuration)
add_agent_test(sensor_configuration TRUE device_model/configuration)
add_agent_test(solid_model TRUE device_model/configuration)
add_agent_test(specification TRUE device_model/configuration)

add_agent_test(entity FALSE entity)
add_agent_test(entity_parser TRUE entity)
add_agent_test(entity_printer TRUE entity)
add_agent_test(json_parser TRUE entity)
add_agent_test(json_printer TRUE entity)
add_agent_test(qname FALSE entity)

add_agent_test(file_cache FALSE sink/rest_sink)
add_agent_test(http_server FALSE sink/rest_sink TRUE)
add_agent_test(tls_http_server FALSE sink/rest_sink TRUE)
add_agent_test(routing FALSE sink/rest_sink)

add_agent_test(mqtt_isolated FALSE mqtt_isolated TRUE)
add_agent_test(mqtt_sink FALSE sink/mqtt_sink TRUE)
add_agent_test(mqtt_sink_2 FALSE sink/mqtt_sink_2 TRUE)

add_agent_test(json_printer_asset TRUE json)
add_agent_test(json_printer_error TRUE json)
add_agent_test(json_printer_probe TRUE json)
add_agent_test(json_printer_stream TRUE json)

add_agent_test(xml_parser TRUE xml)
add_agent_test(xml_printer TRUE xml)

add_agent_test(adapter FALSE adapter)
add_agent_test(connector FALSE adapter)
add_agent_test(mqtt_adapter FALSE adapter)
add_agent_test(url_parser FALSE adapter)
add_agent_test(agent_adapter FALSE adapter)

add_agent_test(shdr_tokenizer FALSE pipeline)
add_agent_test(timestamp_extractor FALSE pipeline)
add_agent_test(data_item_mapping FALSE pipeline)
add_agent_test(duplicate_filter FALSE pipeline)
add_agent_test(pipeline_deliver TRUE pipeline)
add_agent_test(topic_mapping TRUE pipeline)
add_agent_test(period_filter TRUE pipeline)
add_agent_test(pipeline_edit FALSE pipeline)
add_agent_test(mtconnect_xml_transform FALSE pipeline)
add_agent_test(response_document FALSE pipeline)

add_agent_test(agent TRUE core)
add_agent_test(change_observer FALSE core)
add_agent_test(globals FALSE core)

add_agent_test(config_parser FALSE configuration)
add_agent_test(config FALSE configuration)

add_agent_test(observation TRUE observation)
add_agent_test(data_set TRUE observation)
add_agent_test(table TRUE observation)

add_agent_test(checkpoint FALSE buffer)
add_agent_test(circular_buffer FALSE buffer)


if (WITH_RUBY)
  add_agent_test(embedded_ruby TRUE ruby)
endif()

if( WITH_PYTHON)
  add_agent_test(python_transform TRUE python)
endif()

# TODO Reorganize data. Do not copy files around. Unit test could be run only with sources in place.
add_custom_command(
  TARGET adapter_test POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/probe.xml"
  "$<TARGET_FILE_DIR:adapter_test>/../Resources/probe.xml"
  COMMENT "Copying to output directory")

add_dependencies(config_test adapter_plugin_test sink_plugin_test)

if(APPLE)
  add_custom_command(TARGET config_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:adapter_plugin_test>"
    "$<TARGET_FILE_DIR:config_test>/lib$<TARGET_FILE_BASE_NAME:adapter_plugin_test>.dylib"
    )
  
  add_custom_command(TARGET config_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:sink_plugin_test>"
    "$<TARGET_FILE_DIR:config_test>/lib$<TARGET_FILE_BASE_NAME:sink_plugin_test>.dylib")
elseif(UNIX)
  add_custom_command(TARGET config_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:adapter_plugin_test>"
    "$<TARGET_FILE_DIR:config_test>/"
    )
  
  add_custom_command(TARGET config_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:sink_plugin_test>"
    "$<TARGET_FILE_DIR:config_test>/")
endif()

if(SHARED_AGENT_LIB AND MSVC)
  add_custom_command(TARGET config_test POST_BUILD 
     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:config_test> "$<TARGET_FILE_DIR:config_test>"
     COMMAND_EXPAND_LISTS)
endif()
		     
