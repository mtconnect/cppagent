# When building for Windows using the Microsoft compiler we wish to use
# the static multi-threaded runtime libraries (this makes deployment easier).
#
# By default CMake targets the dynamic runtime libaries using the '/MD' and 'MDd' options
# Change these to use the static variants 'MT' and 'MTd'
#
# Note: This must be done before adding any CMake targets (executables, libraries etc...)
#
if(MSVC)
	foreach(flag_var
			CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
		if(${flag_var} MATCHES "/MD")
			string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
		endif()
	endforeach()
endif()