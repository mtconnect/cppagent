set(AGENT_SOURCES cppagent.cpp)

if(WIN32)
  # Version info
  string(TIMESTAMP AGENT_VERSION_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
  set(RESOURCE_FILE "${PROJECT_BINARY_DIR}/agent/version.rc")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/../src/mtconnect/version.rc.in" "${RESOURCE_FILE}")
  list(APPEND AGENT_SOURCES "${RESOURCE_FILE}")
endif()

add_executable(agent ${AGENT_SOURCES})
if(AGENT_PREFIX)
  set_target_properties(agent PROPERTIES OUTPUT_NAME "${AGENT_PREFIX}agent")
endif()

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

target_clangtidy_setup(agent)

install(TARGETS agent RUNTIME)
install(TARGETS agent ARCHIVE)

if(SHARED_AGENT_LIB AND MSVC)
  set(CMAKE_INSTALL_DEBUG_LIBRARIES OFF)
  include(InstallRequiredSystemLibraries)
  install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT Libraries)
  file(GLOB dlls CONFIGURE_DEPENDS "${cppagent_BINARY_DIR}/dlls/*.dll")
  install(FILES ${dlls} DESTINATION bin COMPONENT Libraries)

  add_custom_command(TARGET agent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${dlls} $<TARGET_FILE_DIR:agent>
    COMMAND_EXPAND_LISTS)		     
  add_custom_command(TARGET agent POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:agent> $<TARGET_FILE_DIR:agent> 
    COMMAND_EXPAND_LISTS)
elseif(SHARED_AGENT_LIB)
  file(GLOB dlls CONFIGURE_DEPENDS "${cppagent_BINARY_DIR}/dlls/*.dylib" "${cppagent_BINARY_DIR}/dlls/*.so")
  message(STATUS "!!!!! Copying ${dlls} to lib directory")
  install(FILES ${dlls} TYPE LIB COMPONENT Libraries)
endif()
