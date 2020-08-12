# For Visual Studio generators it is now possible (since V3.6) to set a default startup project.
# We will set this to the agent_test project.
if(AGENT_ENABLE_UNITTESTS)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT agent_test)
else()
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT agent)
endif()

# Allow better viewing and grouping of files in Visual Studio by defining source groups
get_target_property(agent_lib_sources agent_lib SOURCES)
source_group("AgentLib\\Headers Files" FILES ${agent_lib_sources} REGULAR_EXPRESSION "\\.(h|hpp)$")
source_group("AgentLib\\Source Files" FILES ${agent_lib_sources} REGULAR_EXPRESSION "\\.(c|cpp)$")

get_target_property(agent_sources agent SOURCES)
source_group("Agent\\Headers Files" FILES ${agent_sources} REGULAR_EXPRESSION "\\.(h|hpp)$")
source_group("Agent\\Source Files" FILES ${agent_sources} REGULAR_EXPRESSION "\\.(c|cpp)$")
