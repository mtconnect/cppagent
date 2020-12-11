set(AGENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent_device.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent_device.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/asset.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/asset.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/cached_file.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/change_observer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/change_observer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/checkpoint.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/checkpoint.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/circular_buffer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/component_configuration.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/component.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/component.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/composition.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/config.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/connector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/connector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/coordinate_systems.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/cutting_tool.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/cutting_tool.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/description.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/data_item.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/data_item.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/definitions.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/file_asset.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/file_asset.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/geometry.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/globals.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/globals.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/json_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/json_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/motion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/options.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/options.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/ref_counted.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/relationships.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/rolling_file_logger.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/rolling_file_logger.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/sensor_configuration.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/service.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/service.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/solid_model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/specifications.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/specifications.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/version.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_parser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer.hpp"
)

## configure a header file to pass some of the CMake settings to the source code
add_custom_target(UPDATE_VERSION ALL
  COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_CURRENT_SOURCE_DIR}/../src/version.cpp")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../src/version.h.in" "${PROJECT_BINARY_DIR}/agent_lib/version.h")

add_library(agent_lib STATIC ${AGENT_SOURCES})
add_dependencies(agent_lib UPDATE_VERSION)
target_include_directories(
  agent_lib
  PUBLIC
    "${PROJECT_BINARY_DIR}/agent_lib"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
)
target_link_libraries(
  agent_lib
  PUBLIC
    dlib::dlib
    date
    json
    LibXml2::LibXml2
    $<$<PLATFORM_ID:Linux>:pthread>
)
target_compile_definitions(
  agent_lib
  PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WINVER=${WINVER}>
    $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${WINVER}>
    )

# set_property(SOURCE ${AGENT_SOURCES} PROPERTY COMPILE_FLAGS_DEBUG "${COVERAGE_FLAGS}")
target_compile_features(agent_lib PUBLIC cxx_std_14)
target_clangformat_setup(agent_lib)
target_clangtidy_setup(agent_lib)
