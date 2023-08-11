# The version number.
set(AGENT_VERSION_MAJOR 2)
set(AGENT_VERSION_MINOR 2)
set(AGENT_VERSION_PATCH 0)
set(AGENT_VERSION_BUILD 4)
set(AGENT_VERSION_RC "")

# This minimum version is to support Visual Studio 2017 and C++ feature checking and FetchContent
cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

set(USE_FOLDERS ON)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

option(SHARED_AGENT_LIB "Generate shared agent library. Conan options: shared" OFF)
option(DEVELOPMENT "Used for development, includes tests as a subdirectory instead of a package" OFF)
set(AGENT_PREFIX "" CACHE STRING "Prefix for the name of the agent and the agent library: suggested 'mtc'")

set(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/mtconnect")

message(INFO " Shared build: ${SHARED_AGENT_LIB}")

project(cppagent LANGUAGES C CXX)

# We will define these properties by default for each CMake target to be created.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CXX_COMPILE_FEATURES cxx_std_17)

set(WITH_RUBY ON CACHE STRING "With Ruby Support")

# Add our './cmake' sub-folder to the lists searched when calling functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# Setup compiler options for Windows, OSX, Linux (each file guards against usage on an inappropriate
# platform)
if(MSVC)
  # Default winver to Vista and Later
  set(WINVER "0x0600" CACHE STRING "Windows Target Version: 0x0400 - 95 & NT 4.0, 0x0500 - 2000, 0x0501 - XP, 0x0600 - Vista, 0x0601 - 7, 0x0602 - 8")
endif()

if (UNIX)
  if (SHARED_AGENT_LIB)
    add_compile_options(-fPIC -Wno-psabi -fvisibility-inlines-hidden)
  else()
    add_compile_options(-fPIC -Wno-psabi -fvisibility-inlines-hidden -fvisibility=hidden)
  endif()
endif()

if(SHARED_AGENT_LIB)
  if (APPLE)
    set(CMAKE_BUILD_RPATH "@load_path/;@executable_path/;@load_path/../lib;@executable_path/../lib")
    set(CMAKE_INSTALL_RPATH "${CMAKE_BUILD_RPATH}")
  else()
    if(UNIX)
      set(CMAKE_BUILD_RPATH "$ORIGIN;$ORIGIN/../lib")
      set(CMAKE_INSTALL_RPATH "${CMAKE_BUILD_RPATH}")
    endif()
  endif()

  if (MSVC)
    include(InstallRequiredSystemLibraries)
  endif()
endif()

include(cmake/remove_link_directories.cmake)
include(cmake/osx_no_app_or_frameworks.cmake)
include(cmake/ClangFormat.cmake)
include(cmake/ClangTidy.cmake)

# Add our projects
add_subdirectory(agent_lib)
add_subdirectory(agent)

include(cmake/ide_integration.cmake)

if(DEVELOPMENT)
  message(STATUS "Including test package as part of the build for development")
  enable_testing()
  add_subdirectory(test_package test)
endif()

create_clangformat_target()

include(GNUInstallDirs)

if(NOT MSVC)
  set(CMAKE_INSTALL_DATADIR "share/mtconnect")
endif()

# For Visual Studio generators it is now possible (since V3.6) to set a default startup project.
# We will set this to the agent_test project.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT agent)

install(FILES README.md LICENSE.TXT TYPE DATA)
install(DIRECTORY schemas TYPE DATA)
install(DIRECTORY simulator TYPE DATA)
install(DIRECTORY styles TYPE DATA)
install(DIRECTORY tools TYPE DATA)
install(DIRECTORY demo TYPE DATA)

set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_PACKAGE_VERSION "${AGENT_VERSION_MAJOR}.${AGENT_VERSION_MINOR}.${AGENT_VERSION_PATCH}.${AGENT_VERSION_BUILD}${AGENT_VERSION_RC}")
set(CPACK_PACKAGE_VERSION_MAJOR ${AGENT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${AGENT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${AGENT_VERSION_PATCH})
set(CPACK_PACKAGE_NAME "agent")
set(CPACK_PACKAGE_VENDOR "MTConnect.org")

include(CPack)
