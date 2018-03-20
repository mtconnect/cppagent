# By default Visual Studio sets its 'working directory' for debugging to the MSBuild property $(ProjectDir).
# Some targets may require a specific working folder (for example to locate files or libraries).
# This CMake module contains a function that can set the 'working directory'.
#

#################################################################################
#																				#
#								Functions										#
#																				#
#################################################################################

function(SetVisualStudioWorkingDirectory projectTarget workingDirectory)
	if(MSVC)
		set_target_properties(${projectTarget} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${workingDirectory})
	endif()
endfunction()