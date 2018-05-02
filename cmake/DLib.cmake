# This module allows the addition of DLib to a target

#################################################################################
#																				#
#								Functions										#
#																				#
#################################################################################

function(AddDLibSupport projectTarget)
	
	target_include_directories(${projectTarget} PRIVATE ${CMAKE_SOURCE_DIR}/lib)
	target_compile_definitions(${projectTarget} PRIVATE DLIB_NO_GUI_SUPPORT)
endfunction()