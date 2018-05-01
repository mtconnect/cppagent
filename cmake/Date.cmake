# This module allows the addition of Date to a target

#################################################################################
#																				#
#								Functions										#
#																				#
#################################################################################

function(AddDateSupport projectTarget)
	target_include_directories(${projectTarget} PRIVATE ${CMAKE_SOURCE_DIR}/date/include)
endfunction()