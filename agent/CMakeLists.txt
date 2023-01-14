set(AGENT_SOURCES cppagent.cpp)

if(WIN32)
  # Version info
  string(TIMESTAMP AGENT_VERSION_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
  set(RESOURCE_FILE "${PROJECT_BINARY_DIR}/agent/version.rc")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/../src/mtconnect/version.rc.in" "${RESOURCE_FILE}")
  list(APPEND AGENT_SOURCES "${RESOURCE_FILE}")
endif()

add_executable(agent ${AGENT_SOURCES})

if(WITH_PYTHON)
  target_compile_definitions(
    agent
    PUBLIC
    WITH_PYTHON )
endif()

if(WITH_RUBY)
  target_compile_definitions(
    agent
    PUBLIC
    WITH_RUBY )
endif()

if(AGENT_WITHOUT_IPV6)
  target_compile_definitions(
    agent
    PUBLIC
    AGENT_WITHOUT_IPV6 )
endif()

target_link_libraries(
  agent
  PUBLIC
  agent_lib
  $<$<PLATFORM_ID:Windows>:shlwapi>
  )

target_clangtidy_setup(agent_lib)

install(TARGETS agent RUNTIME DESTINATION bin)


if(SHARED_AGENT_LIB AND MSVC)
  set(CMAKE_INSTALL_DEBUG_LIBRARIES ON)  
  include(InstallRequiredSystemLibraries)
  install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT Libraries)
  install(DIRECTORY "${PROJECT_BINARY_DIR}/bin/" DESTINATION bin COMPONENT Libraries
           FILES_MATCHING PATTERN "*.dll")
endif()
