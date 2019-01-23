cmake_minimum_required(VERSION 3.7 FATAL_ERROR) #This minimum version is to support Visual Studio 2017 and C++ feature checking

project(cppagent)

# All of the C++ Agent code, DLib and CppUnit require C++ 14 features to compile.
# We will define these properties by default for each CMake target to be created.
set_property(GLOBAL PROPERTY CXX_STANDARD 14)
set_property(GLOBAL PROPERTY CXX_STANDARD_REQUIRED ON)

# By default only generate 2 configurations (Debug and Release) for simplicity.
# The user can change this option if required for the additional ones such as 'RelWithDebInfo'.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configs" FORCE)
if(DEFINED CMAKE_BUILD_TYPE)
  set_property( CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES} )
endif()

# Add our './cmake' sub-folder to the lists searched when calling functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# Setup compiler options for Windows, OSX, Linux (each file guards against usage on an inappropriate platform)
if(MSVC)
  # Default winver to Vista and Later
  set(WINVER "0x0600" CACHE STRING "Windows Target Version: 0x0400 - 95 & NT 4.0, 0x0500 - 2000, 0x0501 - XP, 0x0600 - Vista, 0x0601 - 7, 0x0602 - 8")
endif()

include(cmake/msvc_xp_support.cmake)
include(cmake/msvc_use_static_runtime.cmake)
include(cmake/vs_set_working_directory.cmake)
include(cmake/osx_no_app_or_frameworks.cmake)

# Add our dependency projects
include(cmake/LibXML2.cmake)
include(cmake/CppUnit.cmake)
include(cmake/DLib.cmake)
include(cmake/Date.cmake)

# Add our projects
add_subdirectory(agent)
add_subdirectory(test)

# For Visual Studio generators it is now possible (since V3.6) to set a default startup project.
# We will set this to the agent_test project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT agent_test)


