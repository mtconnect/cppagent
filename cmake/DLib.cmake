####################################################################################################
#                             Providing dlib library - http://dlib.net                             #
#                                                                                                  #
# To use the dlib library simply link to it: target_link_libraries(<project> PRIVATE dlib::dlib).  #
# This automatically sets all required information such as include directories, definitions etc.   #
####################################################################################################

set(agent_dlib_version_tag "v19.18")

if(NOT TARGET dlib::dlib)
  message(STATUS "Make dlib library (${agent_dlib_version_tag}) available.")
  include(FetchContent)
  FetchContent_Declare(
    dlibprovider
    URL "https://github.com/davisking/dlib/archive/${agent_dlib_version_tag}.zip"
  )
  FetchContent_GetProperties(dlibprovider)

  if(UNIX)
    set(build_options "-j4")
  else()
    set(build_options "-m:4")
  endif()

  set(_link_library "${CMAKE_BINARY_DIR}/install/dlib/lib/${CMAKE_STATIC_LIBRARY_PREFIX}dlib${CMAKE_STATIC_LIBRARY_SUFFIX}")
  if(EXISTS "${_link_library}")
    message(STATUS "  * Reusing previously built dlib library")
  else()
    message(STATUS "  * Donwload dlib library")
    FetchContent_Populate(dlibprovider)

    message(STATUS "  * Patch dlib library")
    message("${dlibprovider_SOURCE_DIR}/dlib")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/../dlib_fix" "${dlibprovider_SOURCE_DIR}/dlib")

    message(STATUS "  * Configure dlib library")
    execute_process(
      COMMAND ${CMAKE_COMMAND}
        -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
        -DCMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED}
        -DCMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS}
        -DBUILD_SHARED_LIBS=OFF
        -DDLIB_ISO_CPP_ONLY=OFF
        -DDLIB_NO_GUI_SUPPORT=ON
        -DDLIB_USE_BLAS=OFF
        -DDLIB_USE_LAPACK=OFF
        -DDLIB_USE_CUDA=OFF
        -DDLIB_USE_MKL_FFT=OFF
        -DDLIB_LINK_WITH_SQLITE3=OFF
        -DDLIB_GIF_SUPPORT=OFF
        -DDLIB_JPEG_SUPPORT=OFF
        -DDLIB_PNG_SUPPORT=OFF
        -DDLIB_USE_MKL_FFT=OFF
	-DWINVER=${WINVER}
	-DCMAKE_DEBUG_POSTFIX=d 
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install/dlib
        ${dlibprovider_SOURCE_DIR}/dlib
      WORKING_DIRECTORY ${dlibprovider_BINARY_DIR}
      OUTPUT_FILE       ${dlibprovider_BINARY_DIR}/configure_output.log
      ERROR_FILE        ${dlibprovider_BINARY_DIR}/configure_output.log
    )
    file(READ "${dlibprovider_BINARY_DIR}/configure_output.log" _output)
    message("============================================================")
    message("${_output}")
    message("============================================================")

    message(STATUS "  * Build Release dlib library")
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build . --target install --config Release -- ${build_options}
      WORKING_DIRECTORY ${dlibprovider_BINARY_DIR}
      OUTPUT_FILE       ${dlibprovider_BINARY_DIR}/build_output.log
      ERROR_FILE        ${dlibprovider_BINARY_DIR}/build_output.log
    )
    file(READ "${dlibprovider_BINARY_DIR}/build_output.log" _output)
    message("============================================================")
    message("${_output}")
    message("============================================================")

    message(STATUS "  * Build Debug dlib library")
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build . --target install --config Debug -- ${build_options}
      WORKING_DIRECTORY ${dlibprovider_BINARY_DIR}
      OUTPUT_FILE       ${dlibprovider_BINARY_DIR}/build_output.log
      ERROR_FILE        ${dlibprovider_BINARY_DIR}/build_output.log
    )

    file(READ "${dlibprovider_BINARY_DIR}/build_output.log" _output)
    message("============================================================")
    message("${_output}")
    message("============================================================")
  endif()

  message(STATUS "  * Create targets for dlib library")
  add_library(dlib::dlib STATIC IMPORTED)
  set_target_properties(dlib::dlib PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/install/dlib/lib/${CMAKE_STATIC_LIBRARY_PREFIX}dlib${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_LOCATION_DEBUG "${CMAKE_BINARY_DIR}/install/dlib/lib/${CMAKE_STATIC_LIBRARY_PREFIX}dlibd${CMAKE_STATIC_LIBRARY_SUFFIX}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/install/dlib/include"
  )
endif()
