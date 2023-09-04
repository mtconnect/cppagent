set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/mtconnect")

set(AGENT_SOURCES
 # src HEADER_FILE_ONLY
        
        "${SOURCE_DIR}/agent.hpp"
        "${SOURCE_DIR}/config.hpp"
        "${SOURCE_DIR}/logging.hpp"
        "${SOURCE_DIR}/utilities.hpp"

# src SOURCE_FILES_ONLY

        "${SOURCE_DIR}/agent.cpp"
        "${SOURCE_DIR}/utilities.cpp"
        "${SOURCE_DIR}/version.cpp"
        

# src/asset HEADER_FILE_ONLY 

        "${SOURCE_DIR}/asset/asset.hpp"
        "${SOURCE_DIR}/asset/asset_buffer.hpp"
        "${SOURCE_DIR}/asset/asset_storage.hpp"
        "${SOURCE_DIR}/asset/cutting_tool.hpp"
        "${SOURCE_DIR}/asset/file_asset.hpp"
        "${SOURCE_DIR}/asset/raw_material.hpp"
        "${SOURCE_DIR}/asset/qif_document.hpp"
        "${SOURCE_DIR}/asset/component_configuration_parameters.hpp"

# src/asset SOURCE_FILES_ONLY
  
        "${SOURCE_DIR}/asset/asset.cpp"
        "${SOURCE_DIR}/asset/cutting_tool.cpp"
        "${SOURCE_DIR}/asset/file_asset.cpp"
        "${SOURCE_DIR}/asset/raw_material.cpp"
        "${SOURCE_DIR}/asset/qif_document.cpp"
        "${SOURCE_DIR}/asset/component_configuration_parameters.cpp"
        
# src/buffer HEADER_FILES_ONLY

        "${SOURCE_DIR}/buffer/checkpoint.hpp"
        "${SOURCE_DIR}/buffer/circular_buffer.hpp"

# src/buffer SOURCE_FILES_ONLY

        "${SOURCE_DIR}/buffer/checkpoint.cpp"

# src/configuration HEADER_FILE_ONLY

        "${SOURCE_DIR}/configuration/agent_config.hpp"
        "${SOURCE_DIR}/configuration/async_context.hpp"
        "${SOURCE_DIR}/configuration/config_options.hpp"
        "${SOURCE_DIR}/configuration/hook_manager.hpp"
        "${SOURCE_DIR}/configuration/parser.hpp"
        "${SOURCE_DIR}/configuration/service.hpp"

# src/configuration SOURCE_FILES_ONLY
  
        "${SOURCE_DIR}/configuration/agent_config.cpp"
        "${SOURCE_DIR}/configuration/parser.cpp"
        "${SOURCE_DIR}/configuration/service.cpp"

# src/device_model HEADER_FILE_ONLY

        "${SOURCE_DIR}/device_model/agent_device.hpp"
        "${SOURCE_DIR}/device_model/component.hpp"
        "${SOURCE_DIR}/device_model/composition.hpp"
        "${SOURCE_DIR}/device_model/description.hpp"
        "${SOURCE_DIR}/device_model/device.hpp"
        "${SOURCE_DIR}/device_model/reference.hpp"
  
# src/device_model SOURCE_FILES_ONLY

        "${SOURCE_DIR}/device_model/agent_device.cpp"
        "${SOURCE_DIR}/device_model/component.cpp"
        "${SOURCE_DIR}/device_model/composition.cpp"
        "${SOURCE_DIR}/device_model/description.cpp"
        "${SOURCE_DIR}/device_model/device.cpp"
        "${SOURCE_DIR}/device_model/reference.cpp"
  
# src/device_model/configuration HEADER_FILE_ONLY

        "${SOURCE_DIR}/device_model/configuration/configuration.hpp"
        "${SOURCE_DIR}/device_model/configuration/coordinate_systems.hpp"
        "${SOURCE_DIR}/device_model/configuration/image_file.hpp"
        "${SOURCE_DIR}/device_model/configuration/motion.hpp"
        "${SOURCE_DIR}/device_model/configuration/relationships.hpp"
        "${SOURCE_DIR}/device_model/configuration/sensor_configuration.hpp"
        "${SOURCE_DIR}/device_model/configuration/solid_model.hpp"
        "${SOURCE_DIR}/device_model/configuration/specifications.hpp"
        
# src/device_model/configuration SOURCE_FILES_ONLY

        "${SOURCE_DIR}/device_model/configuration/configuration.cpp"
        "${SOURCE_DIR}/device_model/configuration/coordinate_systems.cpp"
        "${SOURCE_DIR}/device_model/configuration/image_file.cpp"
        "${SOURCE_DIR}/device_model/configuration/motion.cpp"
        "${SOURCE_DIR}/device_model/configuration/relationships.cpp"
        "${SOURCE_DIR}/device_model/configuration/sensor_configuration.cpp"
        "${SOURCE_DIR}/device_model/configuration/solid_model.cpp"
        "${SOURCE_DIR}/device_model/configuration/specifications.cpp"
        
# src/device_model/data_item HEADER_FILE_ONLY

        "${SOURCE_DIR}/device_model/data_item/constraints.hpp"
        "${SOURCE_DIR}/device_model/data_item/data_item.hpp"
        "${SOURCE_DIR}/device_model/data_item/definition.hpp"
        "${SOURCE_DIR}/device_model/data_item/filter.hpp"
        "${SOURCE_DIR}/device_model/data_item/relationships.hpp"
        "${SOURCE_DIR}/device_model/data_item/source.hpp"
        "${SOURCE_DIR}/device_model/data_item/unit_conversion.hpp"

# src/device_model/data_item SOURCE_FILES_ONLY

        "${SOURCE_DIR}/device_model/data_item/data_item.cpp"
        "${SOURCE_DIR}/device_model/data_item/unit_conversion.cpp"

# src/entity HEADER_FILE_ONLY

        "${SOURCE_DIR}/entity/data_set.hpp"
        "${SOURCE_DIR}/entity/entity.hpp"
        "${SOURCE_DIR}/entity/factory.hpp"
        "${SOURCE_DIR}/entity/json_parser.hpp"
        "${SOURCE_DIR}/entity/json_printer.hpp"
        "${SOURCE_DIR}/entity/qname.hpp"
        "${SOURCE_DIR}/entity/requirement.hpp"
        "${SOURCE_DIR}/entity/xml_parser.hpp"
        "${SOURCE_DIR}/entity/xml_printer.hpp"
  
# src/entity SOURCE_FILES_ONLY

        "${SOURCE_DIR}/entity/data_set.cpp"
        "${SOURCE_DIR}/entity/entity.cpp"
        "${SOURCE_DIR}/entity/factory.cpp"
        "${SOURCE_DIR}/entity/json_parser.cpp"
        "${SOURCE_DIR}/entity/requirement.cpp"
        "${SOURCE_DIR}/entity/xml_parser.cpp"
        "${SOURCE_DIR}/entity/xml_printer.cpp"

# src/mqtt HEADER_FILE_ONLY

        "${SOURCE_DIR}/mqtt/mqtt_authorization.hpp"
        "${SOURCE_DIR}/mqtt/mqtt_client.hpp"
        "${SOURCE_DIR}/mqtt/mqtt_server.hpp"
        "${SOURCE_DIR}/mqtt/mqtt_client_impl.hpp"
        "${SOURCE_DIR}/mqtt/mqtt_server_impl.hpp"
  
# src/observation HEADER_FILE_ONLY 
        
        "${SOURCE_DIR}/observation/change_observer.hpp"
        "${SOURCE_DIR}/observation/observation.hpp"
   
#src/observation SOURCE_FILES_ONLY

        "${SOURCE_DIR}/observation/change_observer.cpp"
        "${SOURCE_DIR}/observation/observation.cpp"

# src/parser HEADER_FILE_ONLY

        "${SOURCE_DIR}/parser/xml_parser.hpp"

# src/parser SOURCE_FILES_ONLY

        "${SOURCE_DIR}/parser/xml_parser.cpp"

# src/pipeline HEADER_FILE_ONLY

        "${SOURCE_DIR}/pipeline/convert_sample.hpp"
        "${SOURCE_DIR}/pipeline/deliver.hpp"
        "${SOURCE_DIR}/pipeline/delta_filter.hpp"
        "${SOURCE_DIR}/pipeline/duplicate_filter.hpp"
        "${SOURCE_DIR}/pipeline/guard.hpp"
        "${SOURCE_DIR}/pipeline/message_mapper.hpp"
        "${SOURCE_DIR}/pipeline/mtconnect_xml_transform.hpp"
        "${SOURCE_DIR}/pipeline/period_filter.hpp"
        "${SOURCE_DIR}/pipeline/pipeline.hpp"
        "${SOURCE_DIR}/pipeline/pipeline_context.hpp"
        "${SOURCE_DIR}/pipeline/pipeline_contract.hpp"
        "${SOURCE_DIR}/pipeline/response_document.hpp"
        "${SOURCE_DIR}/pipeline/shdr_token_mapper.hpp"
        "${SOURCE_DIR}/pipeline/shdr_tokenizer.hpp"
        "${SOURCE_DIR}/pipeline/timestamp_extractor.hpp"
        "${SOURCE_DIR}/pipeline/topic_mapper.hpp"
        "${SOURCE_DIR}/pipeline/transform.hpp"
        "${SOURCE_DIR}/pipeline/upcase_value.hpp"
   
# src/pipeline SOURCE_FILES_ONLY
   
        "${SOURCE_DIR}/pipeline/deliver.cpp"
        "${SOURCE_DIR}/pipeline/shdr_token_mapper.cpp"
        "${SOURCE_DIR}/pipeline/timestamp_extractor.cpp"
        "${SOURCE_DIR}/pipeline/response_document.cpp"

# src/printer HEADER_FILE_ONLY

        "${SOURCE_DIR}/printer/json_printer.hpp"
        "${SOURCE_DIR}/printer/json_printer_helper.hpp"
        "${SOURCE_DIR}/printer/printer.hpp"
        "${SOURCE_DIR}/printer/xml_helper.hpp"
        "${SOURCE_DIR}/printer/xml_printer.hpp"
        "${SOURCE_DIR}/printer/xml_printer_helper.hpp"

# src/printer SOURCE_FILES_ONLY

        "${SOURCE_DIR}/printer/xml_printer.cpp"
        "${SOURCE_DIR}/printer/json_printer.cpp"

# src/source HEADER_FILE_ONLY

        "${SOURCE_DIR}/source/adapter/adapter.hpp"
        "${SOURCE_DIR}/source/adapter/adapter_pipeline.hpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/agent_adapter.hpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/http_session.hpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/https_session.hpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/session.hpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/session_impl.hpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/url_parser.hpp"
        "${SOURCE_DIR}/source/adapter/mqtt/mqtt_adapter.hpp"
        "${SOURCE_DIR}/source/adapter/shdr/connector.hpp"
        "${SOURCE_DIR}/source/adapter/shdr/shdr_adapter.hpp"
        "${SOURCE_DIR}/source/adapter/shdr/shdr_pipeline.hpp"
        "${SOURCE_DIR}/source/error_code.hpp"
        "${SOURCE_DIR}/source/loopback_source.hpp"
        "${SOURCE_DIR}/source/source.hpp"

# src/source SOURCE_FILES_ONLY
        
        "${SOURCE_DIR}/source/adapter/adapter_pipeline.cpp"
        "${SOURCE_DIR}/source/adapter/mqtt/mqtt_adapter.cpp"
        "${SOURCE_DIR}/source/adapter/shdr/connector.cpp"
        "${SOURCE_DIR}/source/adapter/shdr/shdr_adapter.cpp"
        "${SOURCE_DIR}/source/adapter/shdr/shdr_pipeline.cpp"
        "${SOURCE_DIR}/source/loopback_source.cpp"
        "${SOURCE_DIR}/source/source.cpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/agent_adapter.cpp"
        "${SOURCE_DIR}/source/adapter/agent_adapter/url_parser.cpp"
		
# src/sink HEADER_FILE_ONLY

        "${SOURCE_DIR}/sink/sink.hpp"

# src/sink SOURCE_FILE_ONLY
        
        "${SOURCE_DIR}/sink/sink.cpp"

# src/sink/mqtt_sink HEADER_FILE_ONLY

        "${SOURCE_DIR}/sink/mqtt_sink/mqtt_service.hpp"

#src/sink/mqtt_sink SOURCE_FILES_ONLY

        "${SOURCE_DIR}/sink/mqtt_sink/mqtt_service.cpp"
        
# src/sink/rest_sink HEADER_FILE_ONLY
        
        "${SOURCE_DIR}/sink/rest_sink/cached_file.hpp"
        "${SOURCE_DIR}/sink/rest_sink/file_cache.hpp"
        "${SOURCE_DIR}/sink/rest_sink/parameter.hpp"
        "${SOURCE_DIR}/sink/rest_sink/request.hpp"
        "${SOURCE_DIR}/sink/rest_sink/response.hpp"
        "${SOURCE_DIR}/sink/rest_sink/rest_service.hpp"
        "${SOURCE_DIR}/sink/rest_sink/routing.hpp"
        "${SOURCE_DIR}/sink/rest_sink/server.hpp"
        "${SOURCE_DIR}/sink/rest_sink/session.hpp"
        "${SOURCE_DIR}/sink/rest_sink/session_impl.hpp"
        "${SOURCE_DIR}/sink/rest_sink/tls_dector.hpp"
  
# src/sink/rest_sink SOURCE_FILES_ONLY

        "${SOURCE_DIR}/sink/rest_sink/file_cache.cpp"
        "${SOURCE_DIR}/sink/rest_sink/rest_service.cpp"
        "${SOURCE_DIR}/sink/rest_sink/server.cpp"
        "${SOURCE_DIR}/sink/rest_sink/session_impl.cpp"
  )

if(WITH_RUBY)
  set(AGENT_SOURCES ${AGENT_SOURCES}    
# HEADER_FILE_ONLY      
        "${SOURCE_DIR}/ruby/embedded.hpp"
        "${SOURCE_DIR}/ruby/ruby_agent.hpp"
        "${SOURCE_DIR}/ruby/ruby_entity.hpp"
        "${SOURCE_DIR}/ruby/ruby_observation.hpp"
        "${SOURCE_DIR}/ruby/ruby_pipeline.hpp"
        "${SOURCE_DIR}/ruby/ruby_smart_ptr.hpp"
        "${SOURCE_DIR}/ruby/ruby_transform.hpp"
        "${SOURCE_DIR}/ruby/ruby_type.hpp"
        "${SOURCE_DIR}/ruby/ruby_vm.hpp"
        
#SOURCE_FILES_ONLY
        "${SOURCE_DIR}/ruby/embedded.cpp"
    )
endif()

find_package(Boost REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(date REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(mqtt_cpp REQUIRED)
find_package(RapidJSON REQUIRED)

## configure a header file to pass some of the CMake settings to the source code
configure_file("${SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/agent_lib/mtconnect/version.h")

set(AGENT_LIB_HEADERS ${AGENT_SOURCES})
list(FILTER AGENT_LIB_HEADERS INCLUDE REGEX "\\.(h|hpp|ipp)$")
set(AGENT_LIB_SOURCES ${AGENT_SOURCES})
list(FILTER AGENT_LIB_SOURCES INCLUDE REGEX "\\.(c|cpp)$")

# For IDE Grouping
source_group("Header Files")
source_group("Source Files")
source_group(TREE "${SOURCE_DIR}" PREFIX "Header Files" FILES ${AGENT_LIB_HEADERS})
source_group(TREE "${SOURCE_DIR}" PREFIX "Source Files" FILES ${AGENT_LIB_SOURCES})

if(MSVC)
  if(CMAKE_GENERATOR_TOOLSET)
    if(${CMAKE_GENERATOR_TOOLSET} MATCHES "v14[01]_xp")
      message(info ": Setting /Ob1 for rest_sink/session_impl.cpp")
      set_property(SOURCE "${SOURCE_DIR}/sink/rest_sink/session_impl.cpp"
        PROPERTY COMPILE_FLAGS "/Ob1")
    endif()
  endif()

  # The modules including Beast required the /bigobj option in Windows
  set_property(SOURCE    
	"${SOURCE_DIR}/sink/mqtt_sink/mqtt_service.cpp"
    "${SOURCE_DIR}/sink/rest_sink/session_impl.cpp"
    "${SOURCE_DIR}/source/adapter/mqtt/mqtt_adapter.cpp"
    "${SOURCE_DIR}/source/adapter/agent_adapter/agent_adapter.cpp"
    "${SOURCE_DIR}/source/adapter/shdr/shdr_pipeline.cpp"
    "${SOURCE_DIR}/source/adapter/adapter_pipeline.cpp"
    "${SOURCE_DIR}/device_model/component.cpp"
    "${SOURCE_DIR}/configuration/agent_config.cpp"
    "${SOURCE_DIR}/sink/rest_sink/rest_service.cpp"
    "${SOURCE_DIR}/ruby/embedded.cpp"
    "${SOURCE_DIR}/pipeline/deliver.cpp"
    "${SOURCE_DIR}/agent.cpp"
    PROPERTY COMPILE_FLAGS "/bigobj")
endif()

if(SHARED_AGENT_LIB)
  add_library(agent_lib SHARED ${AGENT_SOURCES})
  target_compile_definitions(
    agent_lib
    PRIVATE
    AGENT_BUILD_SHARED_LIB)
  target_compile_definitions(
    agent_lib
    PUBLIC
    SHARED_AGENT_LIB)
else()
  add_library(agent_lib STATIC ${AGENT_SOURCES})
endif()

if(AGENT_PREFIX)
  set_target_properties(agent_lib PROPERTIES OUTPUT_NAME "${AGENT_PREFIX}agent_lib")
endif()

target_include_directories(
  agent_lib
  PUBLIC
    "${CONAN_INCLUDE_DIRS}"
    "${PROJECT_BINARY_DIR}/agent_lib"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    )

target_link_libraries(
  agent_lib
  PUBLIC
  boost::boost LibXml2::LibXml2 date::date-tz openssl::openssl
  nlohmann_json::nlohmann_json mqtt_cpp::mqtt_cpp 
  rapidjson BZip2::BZip2
  
  $<$<PLATFORM_ID:Linux>:pthread>
  $<$<PLATFORM_ID:Windows>:bcrypt>
  )

if(WITH_RUBY)
  find_package(mruby REQUIRED)
  find_package(oniguruma REQUIRED)
  target_link_libraries(
    agent_lib
    PUBLIC
    mruby::mruby
    oniguruma::onig)
endif()

target_compile_definitions(
  agent_lib
  PUBLIC
    $<$<BOOL:$ENV{APPVEYOR}>:APPVEYOR>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WINVER=${WINVER}>
    $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${WINVER}>
    MQTT_USE_TLS=ON
    MQTT_USE_WS=ON
    MQTT_USE_STR_CHECK=ON
    MQTT_STD_VARIANT
    MQTT_STD_OPTIONAL
    MQTT_STD_STRING_VIEW
    MQTT_USE_LOG
    BOOST_FILESYSTEM_VERSION=3
    )

if(WITH_PYTHON)
  target_compile_definitions(
    agent_lib
    PUBLIC
    WITH_PYTHON )
endif()

if(WITH_RUBY)
  target_compile_definitions(
    agent_lib
    PUBLIC
    WITH_RUBY )
endif()

if(AGENT_WITHOUT_IPV6)
  target_compile_definitions(
    agent_lib
    PUBLIC
    AGENT_WITHOUT_IPV6 )
endif()
  
# set_property(SOURCE ${AGENT_SOURCES} PROPERTY COMPILE_FLAGS_DEBUG "${COVERAGE_FLAGS}")
target_compile_features(agent_lib PUBLIC ${CXX_COMPILE_FEATURES})
target_clangformat_setup(agent_lib)
target_clangtidy_setup(agent_lib)

include(../cmake/document.cmake)
if (AGENT_WITH_DOCS AND DOXYGEN_FOUND)
  doxygen_add_docs(docs ../README.md ${AGENT_SOURCES})
  install(DIRECTORY "${PROJECT_BINARY_DIR}/Documentation/" DESTINATION "Documentation")  
endif()

target_sources(agent_lib PUBLIC FILE_SET headers
  TYPE HEADERS
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../src/" "${PROJECT_BINARY_DIR}/agent_lib/"
  FILES "${AGENT_LIB_HEADERS}" "${PROJECT_BINARY_DIR}/agent_lib/mtconnect/version.h")
  

install(TARGETS agent_lib LIBRARY)
install(TARGETS agent_lib ARCHIVE)
install(TARGETS agent_lib FILE_SET headers PUBLIC_HEADER)
