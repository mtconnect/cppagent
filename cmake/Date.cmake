####################################################################################################
#                 Providing date library - https://github.com/HowardHinnant/date                   #
#                                                                                                  #
# To use the date library simply link to it: target_link_libraries(<project> PRIVATE date).        #
# This automatically sets all required information such as include directories, definitions etc.   #
####################################################################################################

set(agent_date_lib_version_tag "v2.4.1")

if(NOT TARGET date)
  message(STATUS "Make date library (${agent_date_lib_version_tag}) available.")
  message(STATUS "  * Donwload date library")
  include(FetchContent)
  FetchContent_Declare(
    dateprovider
    URL "https://github.com/HowardHinnant/date/archive/${agent_date_lib_version_tag}.zip"
  )
  FetchContent_GetProperties(dateprovider)
  FetchContent_Populate(dateprovider)
  
  add_library(date INTERFACE)
  target_include_directories(date INTERFACE "${dateprovider_SOURCE_DIR}/include")
  set_target_properties(date PROPERTIES INTERFACE_SOURCES "${dateprovider_SOURCE_DIR}/src/tz.cpp" )
  set_target_properties(date PROPERTIES INTERFACE_COMPILE_DEFINITIONS "HAS_REMOTE_API=0")
endif()
