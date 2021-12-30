####################################################################################################
#                    Providing JSON library - https://nlohmann.github.io/json                      #
#                                                                                                  #
# To use the JSON library simply link to it: target_link_libraries(<project> PRIVATE json).        #
# This automatically sets all required information such as include directories, definitions etc.   #
####################################################################################################

set(agent_json_lib_version_tag "v3.9.1")

if(NOT TARGET json)
  message(STATUS "Make JSON library (${agent_json_lib_version_tag}) available.")
  message(STATUS "  * Download JSON library")
  include(FetchContent)
  FetchContent_Declare(
    jsonprovider
    URL "https://github.com/nlohmann/json/archive/${agent_json_lib_version_tag}.zip"
  )
  FetchContent_GetProperties(jsonprovider)
  FetchContent_Populate(jsonprovider)

  message(STATUS "  * Create targets for date library")
  add_library(json INTERFACE)
  target_include_directories(json INTERFACE "${jsonprovider_SOURCE_DIR}/single_include")
endif()
