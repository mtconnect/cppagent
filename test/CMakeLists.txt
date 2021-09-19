
include(GoogleTest)

if (APPLE)
  set(LIB_PREFIX "lib")
  set(LIB_SUFFIX ".dylib")
else()
  set(LIB_PREFIX "")
  set(LIB_SUFFIX "")
endif()

add_library(sink_plugin_test SHARED testsink_service.cpp testsink_service.hpp)
target_include_directories(
  sink_plugin_test
  PUBLIC
    "${CONAN_INCLUDE_DIRS}"
    "${PROJECT_BINARY_DIR}/agent_lib"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    )
target_compile_definitions(sink_plugin_test
    PRIVATE "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\""
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:sink_plugin_test>/../Resources\""
    $<$<BOOL:$ENV{APPVEYOR}>:APPVEYOR>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WINVER=${WINVER}>
    $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${WINVER}>)
    
target_compile_features(sink_plugin_test PUBLIC ${CXX_COMPILE_FEATURES})
set_property(TARGET sink_plugin_test PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

target_link_libraries(
  sink_plugin_test
  PRIVATE
  agent_lib
  $<$<PLATFORM_ID:Windows>:shlwapi>
  )

set_target_properties(sink_plugin_test PROPERTIES FOLDER "test/sink")

if (APPLE)
  set_target_properties(sink_plugin_test PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER sink_plugin_test
    VERSION 16.4.0
    SOVERSION 1.0.0)
endif()

#########

add_library(adapter_plugin_test SHARED testadapter_service.cpp testadapter_service.hpp)

target_include_directories(
  adapter_plugin_test
  PUBLIC
    "${CONAN_INCLUDE_DIRS}"
    "${PROJECT_BINARY_DIR}/agent_lib"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    )
target_compile_definitions(adapter_plugin_test
    PRIVATE "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\""
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:sink_plugin_test>/../Resources\""
    $<$<BOOL:$ENV{APPVEYOR}>:APPVEYOR>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WINVER=${WINVER}>
    $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${WINVER}>)

if (APPLE)
  set_target_properties(adapter_plugin_test PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER adapter_plugin_test
    VERSION 16.4.0
    SOVERSION 1.0.0)
endif()
  
target_compile_features(adapter_plugin_test PUBLIC ${CXX_COMPILE_FEATURES})
set_property(TARGET adapter_plugin_test PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

target_link_libraries(
  adapter_plugin_test
  PRIVATE
  agent_lib
  $<$<PLATFORM_ID:Windows>:shlwapi>
  )

set_target_properties(adapter_plugin_test PROPERTIES FOLDER "test/adapter")

###########

set(AGENT_TEST_LIB_SOURCES test_utilities.cpp agent_test_helper.cpp agent_test_helper.hpp test_utilities.hpp)
add_library(agent_test_lib STATIC ${AGENT_TEST_LIB_SOURCES})
target_include_directories(
  agent_test_lib
  PUBLIC
    "${CONAN_INCLUDE_DIRS}"
    "${PROJECT_BINARY_DIR}/agent_lib"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    "${googletest_SOURCE_DIR}/googletest/include"
    )
set_property(TARGET agent_test_lib PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(
  agent_test_lib
  PUBLIC
  agent_lib
)
target_compile_definitions(agent_test_lib
  PUBLIC
    "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\""
  )
target_clangtidy_setup(agent_test_lib)
set_target_properties(agent_test_lib PROPERTIES FOLDER "test")

# Define test cases
macro(add_agent_test AGENT_TEST_NAME ADD_TEST_HELPER SUB_FOLDER)
  set(_sources ${AGENT_TEST_NAME}_test.cpp)
  add_executable(${AGENT_TEST_NAME}_test ${_sources})
  set_property(TARGET ${AGENT_TEST_NAME}_test PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )
  # set_property(TARGET ${AGENT_TEST_NAME}_test PROPERTY LINK_FLAGS_DEBUG "${COVERAGE_FLAGS}")
  target_link_libraries(${AGENT_TEST_NAME}_test agent_lib agent_test_lib CONAN_PKG::gtest)
  target_include_directories(${AGENT_TEST_NAME}_test
    PRIVATE "${googletest_SOURCE_DIR}/googletest/include")

  target_compile_definitions(${AGENT_TEST_NAME}_test
    PRIVATE "PROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\""
    "TEST_BIN_ROOT_DIR=\"$<TARGET_FILE_DIR:${AGENT_TEST_NAME}_test>/../Resources\"")
  target_clangtidy_setup(agent_lib)
  add_test(NAME ${AGENT_TEST_NAME}_test COMMAND ${AGENT_TEST_NAME}_test)
  target_compile_features(${AGENT_TEST_NAME}_test PUBLIC ${CXX_COMPILE_FEATURES})

  # Organize into folders
  set_target_properties(${AGENT_TEST_NAME}_test PROPERTIES FOLDER "test/${SUB_FOLDER}")

  gtest_discover_tests(${AGENT_TEST_NAME}_test DISCOVERY_MODE PRE_TEST)
endmacro()

add_agent_test(asset TRUE asset)
add_agent_test(file_asset TRUE asset)
add_agent_test(cutting_tool TRUE asset)

add_agent_test(agent_device TRUE device_model)
add_agent_test(component FALSE device_model)
add_agent_test(composition TRUE device_model)
add_agent_test(device FALSE device_model)
add_agent_test(references TRUE device_model)

add_agent_test(data_item FALSE device_model/data_item)
add_agent_test(unit_conversion FALSE device_model/data_item)

add_agent_test(coordinate_system TRUE device_model/configuration)
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
add_agent_test(http_server FALSE sink/rest_sink)
add_agent_test(tls_http_server FALSE sink/rest_sink)
add_agent_test(routing FALSE sink/rest_sink)

add_agent_test(json_printer_asset TRUE json)
add_agent_test(json_printer_error TRUE json)
add_agent_test(json_printer_probe TRUE json)
add_agent_test(json_printer_stream TRUE json)

add_agent_test(xml_parser TRUE xml)
add_agent_test(xml_printer TRUE xml)

add_agent_test(adapter FALSE adapter)
add_agent_test(connector FALSE adapter)
add_agent_test(mqtt_adapter FALSE adapter)

add_agent_test(shdr_tokenizer FALSE pipeline)
add_agent_test(timestamp_extractor FALSE pipeline)
add_agent_test(data_item_mapping FALSE pipeline)
add_agent_test(duplicate_filter FALSE pipeline)
add_agent_test(pipeline_deliver TRUE pipeline)
add_agent_test(topic_mapping TRUE pipeline)
add_agent_test(pipeline_edit FALSE pipeline)

add_agent_test(agent TRUE core)
add_agent_test(asset_buffer TRUE core)
add_agent_test(config FALSE core)
add_agent_test(change_observer FALSE core)
add_agent_test(globals FALSE core)

add_agent_test(config_parser FALSE configuration)

add_agent_test(checkpoint FALSE core)

add_agent_test(observation TRUE observation)
add_agent_test(data_set TRUE observation)
add_agent_test(table TRUE observation)

if(NOT WITHOUT_PYTHON)
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

if(NOT MSVC)
  add_custom_command(TARGET config_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:adapter_plugin_test>"
        "$<TARGET_FILE_DIR:config_test>/${LIB_PREFIX}$<TARGET_FILE_BASE_NAME:adapter_plugin_test>${LIB_SUFFIX}"
	)
      
  add_custom_command(TARGET config_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:sink_plugin_test>"
        "$<TARGET_FILE_DIR:config_test>/${LIB_PREFIX}$<TARGET_FILE_BASE_NAME:sink_plugin_test>${LIB_SUFFIX}")

endif()

