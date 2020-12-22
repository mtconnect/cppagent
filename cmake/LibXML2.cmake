####################################################################################################
#             Providing LibXML2 library - https://gitlab.gnome.org/GNOME/libxml2.git               #
#                                                                                                  #
# To use the LibXML2 library simply link to it:                                                    #
# target_link_libraries(<project> PRIVATE LibXml2::LibXml2).                                       #
# This automatically sets all required information such as include directories, definitions etc.   #
####################################################################################################

option(AGENT_USE_NATIVE_LIBXML2 "Use systems LibXML2" OFF)

if(UNIX AND AGENT_USE_NATIVE_LIBXML2)
  find_package(LibXML2 REQUIRED)

  if(NOT TARGET LibXml2::LibXml2)
    add_library(LibXml2::LibXml2 SHARED IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
      IMPORTED_LOCATION "${LibXML2_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${LibXML2_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "${LibXML2_DEFINITIONS}"
    )
  endif()
else()
  set(agent_xml2_lib_version_tag "v2.9.10")
  if(NOT TARGET LibXml2::LibXml2)
    message(STATUS "Make LibXML2 (${agent_xml2_lib_version_tag}) available.")
    include(FetchContent)
    FetchContent_Declare(
      libxmlprovider
      URL "https://github.com/GNOME/libxml2/archive/${agent_xml2_lib_version_tag}.zip"
#      URL "https://gitlab.gnome.org/GNOME/libxml2/-/archive/${agent_xml2_lib_version_tag}/libxml2-${agent_xml2_lib_version_tag}.zip"
    )
    FetchContent_GetProperties(libxmlprovider)

    if(UNIX)
      set(CUSTOM_MAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../libxml2_make/linux")
    else()
      set(CUSTOM_MAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../libxml2_make/windows")
    endif()
    set(_link_library "${CMAKE_BINARY_DIR}/install/libxml2/lib/${CMAKE_STATIC_LIBRARY_PREFIX}libxml2${CMAKE_STATIC_LIBRARY_SUFFIX}")

    if(EXISTS "${_link_library}")
      message(STATUS "  * Reusing previously built LibXML2 library")
    else()
      message(STATUS "  * Download LibXML2")
      FetchContent_Populate(libxmlprovider)

      if(UNIX)
        set(build_options "-j4")
      else()
        set(build_options "-m:4")
      endif()


      message(STATUS "  * Configure LibXML2")
      execute_process(
        COMMAND ${CMAKE_COMMAND}
          -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
          -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
          -DWINVER=${WINVER}
          -DBUILD_SHARED_LIBS=OFF
          -DLibXML2_SRC_DIR="${libxmlprovider_SOURCE_DIR}"
          -DLibXML2_INCLUDE_DIRS=${CUSTOM_MAKE_PATH}/include
          -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install/libxml2
          ${CUSTOM_MAKE_PATH}
        WORKING_DIRECTORY ${libxmlprovider_BINARY_DIR}
        OUTPUT_FILE       ${libxmlprovider_BINARY_DIR}/configure_output.log
        ERROR_FILE        ${libxmlprovider_BINARY_DIR}/configure_output.log
      )
      file(READ "${libxmlprovider_BINARY_DIR}/configure_output.log" _output)
      message("============================================================")
      message("${_output}")
      message("============================================================")


      message(STATUS "  * Build LibXML2")
      execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --target install --config Release -- ${build_options}
        WORKING_DIRECTORY ${libxmlprovider_BINARY_DIR}
        OUTPUT_FILE       ${libxmlprovider_BINARY_DIR}/build_output.log
        ERROR_FILE        ${libxmlprovider_BINARY_DIR}/build_output.log
      )
      file(READ "${libxmlprovider_BINARY_DIR}/build_output.log" _output)
      message("============================================================")
      message("${_output}")
      message("============================================================")
    endif()

    # Make LibXml2 library available within CMake
    add_library(LibXml2::LibXml2 STATIC IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
      IMPORTED_LOCATION "${_link_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${CUSTOM_MAKE_PATH}/include;${CMAKE_BINARY_DIR}/install/libxml2/include"
      INTERFACE_COMPILE_DEFINITIONS LIBXML_STATIC
    )
  endif()
endif()
