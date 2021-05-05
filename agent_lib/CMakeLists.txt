set(AGENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent_loopback_pipeline.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/agent_loopback_pipeline.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/utilities.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/utilities.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/json_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/json_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/logging.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/version.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_helper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_parser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/xml_printer_helper.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/config_options.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/parser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/agent_config.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/agent_config.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/service.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/service.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/option.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/configuration/option.hpp"


  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/adapter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/adapter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/connector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/connector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/adapter_pipeline.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/adapter/adapter_pipeline.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline_context.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline_contract.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/transform.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/duplicate_filter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/period_filter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/delta_filter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/shdr_tokenizer.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/shdr_token_mapper.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/shdr_token_mapper.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/timestamp_extractor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/timestamp_extractor.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/deliver.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/deliver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/guard.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/convert_sample.hpp"
  
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
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/component.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/component.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/composition.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/composition.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/description.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/description.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/device.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/device.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/reference.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/reference.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/data_item.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/data_item.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/source.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/constraints.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/filter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/relationships.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/definition.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/unit_conversion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/data_item/unit_conversion.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/motion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/motion.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/solid_model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/solid_model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/specifications.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/specifications.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/sensor_configuration.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/sensor_configuration.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/relationships.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/relationships.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/configuration.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/configuration.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/coordinate_systems.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/device_model/configuration/coordinate_systems.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/entity.hpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/entity.cpp"  
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/entity/qname.hpp"  
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
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/parameter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/request.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/response.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/routing.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/session.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/session_impl.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/session_impl.hpp"

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

if(MSVC)
  if(CMAKE_GENERATOR_TOOLSET)
    if(${CMAKE_GENERATOR_TOOLSET} MATCHES "v14[01]_xp")
      message(info ": Setting /Ob1 for http_server/session_impl.cpp")
      set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../src/http_server/session_impl.cpp"
	PROPERTY COMPILE_FLAGS "/Ob1")
    endif()
  endif()
endif()


add_library(agent_lib STATIC ${AGENT_SOURCES})
add_dependencies(agent_lib UPDATE_VERSION)
target_include_directories(
  agent_lib
  PUBLIC
    "${CONAN_INCLUDE_DIRS}"
    "${PROJECT_BINARY_DIR}/agent_lib"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    )

set_property(TARGET agent_lib PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  
target_link_libraries(
  agent_lib
  PUBLIC
  CONAN_PKG::boost CONAN_PKG::libxml2 CONAN_PKG::date
  
  $<$<PLATFORM_ID:Linux>:pthread>
  $<$<PLATFORM_ID:Windows>:bcrypt>
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
