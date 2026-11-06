set(AGENT_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/agent_version.txt")

# Read the first line and trim whitespace / stray \r from CRLF files
file(STRINGS "${AGENT_VERSION_FILE}" _agent_version_line LIMIT_COUNT 1)
string(STRIP "${_agent_version_line}" _agent_version_line)

# MAJOR.MINOR.PATCH.BUILD with an optional RC suffix: 2.8.0.4, 2.8.0.4-RC1, 2.8.0.4_RC1, 2.8.0.4RC1
if(NOT _agent_version_line MATCHES
   "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)([-_]?(RC[0-9]+))?$")
  message(FATAL_ERROR
    "Invalid version '${_agent_version_line}' in ${AGENT_VERSION_FILE}; "
    "expected MAJOR.MINOR.PATCH.BUILD[-RCn]")
endif()

set(AGENT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(AGENT_VERSION_MINOR ${CMAKE_MATCH_2})
set(AGENT_VERSION_PATCH ${CMAKE_MATCH_3})
set(AGENT_VERSION_BUILD ${CMAKE_MATCH_4})
set(AGENT_VERSION_RC    "${CMAKE_MATCH_6}")   # "" when there's no RC

# Re-run configure automatically when the version file changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${AGENT_VERSION_FILE}")
