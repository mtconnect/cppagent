set(AGENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/config.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/globals.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/globals.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/json_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/json_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/options.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/options.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/ref_counted.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/rolling_file_logger.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/rolling_file_logger.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/service.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/service.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/version.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_helper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_parser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer_helper.hpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/adapter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/adapter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/connector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/connector.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/source.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/pipeline.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/transform.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/shdr_tokenizer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/shdr_token_mapper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/shdr_token_mapper.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/timestamp_extractor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/source/timestamp_extractor.hpp"
  
  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/asset.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/asset.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/asset_buffer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/asset_buffer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/file_asset.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/file_asset.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/cutting_tool.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/assets/cutting_tool.hpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/agent_device.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/agent_device.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/component_configuration.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/component.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/component.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/composition.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/coordinate_systems.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/description.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/definitions.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/device.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/device.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/geometry.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/motion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/solid_model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/specifications.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/specifications.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/sensor_configuration.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/relationships.hpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/entity.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/requirement.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/requirement.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/factory.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/factory.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/json_parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/json_parser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/json_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/json_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/xml_parser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/xml_parser.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/xml_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/xml_printer.cpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/cached_file.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/server.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/server.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/file_cache.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/file_cache.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/response.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/response.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/routing.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/routing.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/observation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/observation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/checkpoint.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/checkpoint.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/circular_buffer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/change_observer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/change_observer.hpp"    
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/observation/data_set.hpp"
  
)

## configure a header file to pass some of the CMake settings to the source code
add_custom_target(UPDATE_VERSION ALL
  COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_CURRENT_SOURCE_DIR}/../src/version.cpp")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../src/version.h.in" "${PROJECT_BINARY_DIR}/agent_lib/version.h")

set(AGENT_LIB_HEADERS ${AGENT_SOURCES})
list(FILTER AGENT_LIB_HEADERS INCLUDE REGEX "\\.(h|hpp)$")
set(AGENT_LIB_SOURCES ${AGENT_SOURCES})
list(FILTER AGENT_LIB_SOURCES INCLUDE REGEX "\\.(c|cpp)$")

# For IDE Grouping
source_group("Header Files")
source_group("Source Files")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../src" PREFIX "Header Files" FILES ${AGENT_LIB_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../src" PREFIX "Source Files" FILES ${AGENT_LIB_SOURCES})

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
target_compile_features(agent_lib PUBLIC ${CXX_COMPILE_FEATURES})
target_clangformat_setup(agent_lib)
target_clangtidy_setup(agent_lib)
