# This module allows the addition of DLib to a target

#################################################################################
#																				#
#								Functions										#
#																				#
#################################################################################

set(DLIB_NO_GUI_SUPPORT true)
set(DLIB_USE_BLAS false)
set(DLIB_USE_LAPACK false)
set(DLIB_USE_CUDA false)
set(DLIB_LINK_WITH_SQLITE3 false)
set(DLIB_USE_MKL_FFT false)
set(DLIB_GIF_SUPPORT false)
set(DLIB_JPEG_SUPPORT false)

function(AddDLibSupport projectTarget)
  target_include_directories(${projectTarget} PRIVATE ${CMAKE_SOURCE_DIR}/dlib)
  target_compile_definitions(${projectTarget} PRIVATE DLIB_NO_GUI_SUPPORT)
  target_link_libraries(${projectTarget} PRIVATE dlib::dlib)
endfunction()
