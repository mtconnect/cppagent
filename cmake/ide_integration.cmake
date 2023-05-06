# For Visual Studio generators it is now possible (since V3.6) to set a default startup project.
# We will set this to the agent_test project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mtcagent)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Allow better viewing and grouping of files in Visual Studio by defining source groups
get_target_property(agent_sources mtcagent SOURCES)
source_group("Agent\\Headers Files" FILES ${agent_sources} REGULAR_EXPRESSION "\\.(h|hpp)$")
source_group("Agent\\Source Files" FILES ${agent_sources} REGULAR_EXPRESSION "\\.(c|cpp)$")
