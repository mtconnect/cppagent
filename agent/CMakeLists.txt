set(AGENT_SOURCES cppagent.cpp)

if(WIN32)
  # Version info
  string(TIMESTAMP AGENT_VERSION_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
  set(RESOURCE_FILE "${PROJECT_BINARY_DIR}/agent/version.rc")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/../src/version.rc.in" "${RESOURCE_FILE}")
  list(APPEND AGENT_SOURCES "${RESOURCE_FILE}")
endif()

add_executable(agent ${AGENT_SOURCES})

set_property(TARGET agent PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


target_link_libraries(
  agent
  PRIVATE
  agent_lib
  $<$<PLATFORM_ID:Windows>:shlwapi>
  )
if(APPLE)
  add_custom_command(TARGET agent POST_BUILD COMMAND
    ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../lib" $<TARGET_FILE:agent>)
endif()

target_clangtidy_setup(agent_lib)

install(TARGETS agent RUNTIME DESTINATION bin)
