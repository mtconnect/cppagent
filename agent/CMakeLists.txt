set(AGENT_SOURCES cppagent.cpp)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.75.0 REQUIRED )


if(WIN32)
  # Version info
  string(TIMESTAMP AGENT_VERSION_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
  set(RESOURCE_FILE "${PROJECT_BINARY_DIR}/agent/version.rc")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/../src/version.rc.in" "${RESOURCE_FILE}")
  list(APPEND AGENT_SOURCES "${RESOURCE_FILE}")
endif()

if(boost_FOUND)
  list(APPEND AGENT_SOURCES "${Boost_INCLUDE_DIRS}")
endif()

add_executable(agent ${AGENT_SOURCES})
target_link_libraries(
  agent
  PRIVATE
    agent_lib
    $<$<PLATFORM_ID:Windows>:shlwapi>
)
target_clangtidy_setup(agent_lib)

install(TARGETS agent RUNTIME DESTINATION bin)
