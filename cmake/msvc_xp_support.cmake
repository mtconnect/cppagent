# If we are using a XP compatable toolset (specified with the '-T' argument to cmake.exe) then
# we may need to make some changes to the way projects are compiled.

# Force CMake to show the toolset generator option. This will allow a user to specify a toolset not available in the GUI (Option is now available from CMake v3.5.1).
# Values could be something like:
#  - v120_xp (Visual Studio 2013 targeted for XP)
#  - v140_xp (Visual Studio 2015 targeted for XP)
#  - v141_xp (Visual Studio 2017 targeted for XP)
#  - v120 (Normal Visual Studio 2013)
#  - v140 (Normal Visual Studio 2015)
#  - v141 (Normal isual Studio 2017)
set(CMAKE_GENERATOR_TOOLSET ${CMAKE_GENERATOR_TOOLSET} CACHE string "Use to target a toolset not available in the CMake GUI list of options, e.g. 'v141_xp'" FORCE)


#################################################################################
#																				#
#								Functions										#
#																				#
#################################################################################

function(AddMsvcXPSupport projectTarget)
	if (CMAKE_GENERATOR_TOOLSET MATCHES "v[0-9]*_xp")
		message(STATUS "Visual Studio XP compliant generator toolkit specified for target ${projectTarget}")

		# Define compiler flags for the target to prevent use of Windows APIs that are not XP compliant
		target_compile_definitions(${projectTarget}
			PRIVATE WINVER=0x502
			PRIVATE _WIN32_WINNT=0x502)

		# From Visual Studio 2015 and later, the compiler initialises static variables
		# in a thread-safe manner. This can cause issues when running under Windows XP
		# from a C# host application or anything dynamically loading a module.
		# So to be extra safe we will disable this behaviour for shared libraries.
		# https://docs.microsoft.com/en-us/cpp/build/reference/zc-threadsafeinit-thread-safe-local-static-initialization
		# https://support.microsoft.com/en-gb/help/118816/prb-calling-loadlibrary-to-load-a-dll-that-has-static-tls
		# https://msdn.microsoft.com/en-us/library/windows/desktop/6yh4a9k1(v=vs.140).aspx
		# https://blogs.msdn.microsoft.com/oldnewthing/20101122-00/?p=12233/#10095505
		if(MSVC_VERSION GREATER 1700)
			get_target_property(target_type ${projectTarget} TYPE)
			if(target_type STREQUAL "SHARED_LIBRARY")
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:threadSafeInit- ")
			endif()
		endif()
	endif()
endfunction()