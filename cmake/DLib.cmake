# This module allows the addition of DLib to a target

####################################################################################################
#                                                                                                  #
#                                            Functions                                             #
#                                                                                                  #
####################################################################################################

set(DLIB_NO_GUI_SUPPORT ON CACHE STRING "NO GUI DLIB Suport" FORCE)
option(DLIB_NO_GUI_SUPPORT true)
option(DLIB_USE_BLAS false)
option(DLIB_USE_LAPACK false)
option(DLIB_USE_CUDA false)
option(DLIB_LINK_WITH_SQLITE3 false)
option(DLIB_USE_MKL_FFT false)
option(DLIB_GIF_SUPPORT false)
option(DLIB_JPEG_SUPPORT false)
option(DLIB_PNG_SUPPORT false)
option(DLIB_USE_MKL_FFT false)

add_subdirectory(dlib)

file(GLOB DLIB_FIX_SRCS "${CMAKE_SOURCE_DIR}/dlib_fix/sockets/*.cpp"
  "${CMAKE_SOURCE_DIR}/dlib_fix/tokenizer/*.cpp")

function(AddDLibSupport projectTarget)
  target_include_directories(${projectTarget} PRIVATE "${CMAKE_SOURCE_DIR}/dlib_fix"
    "${CMAKE_SOURCE_DIR}/dlib")
  target_compile_definitions(${projectTarget} PRIVATE DLIB_NO_GUI_SUPPORT)
  target_link_libraries(${projectTarget} PRIVATE dlib::dlib)
endfunction()
