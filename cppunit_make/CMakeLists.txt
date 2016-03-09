cmake_minimum_required(VERSION 2.6)

project(cppunit)

set(CPPUNIT_SRC_DIR ../cppunit/cppunit/src/cppunit)
set(CPPUNIT_INCLUDE_DIRS ../cppunit/cppunit/include)

file(GLOB internal_headers ${CPPUNIT_SRC_DIR}/A-Za-z_]*.h)
file(GLOB install_headers ${CPPUNIT_INCLUDE_DIRS}/cppunit/A-Za-z_]/*.h)
file(GLOB sources ${CPPUNIT_SRC_DIR}/[A-Za-z_]*.cpp)
list(REMOVE_ITEM sources ${CPPUNIT_SRC_DIR}/UnixDynamicLibraryManager.cpp)

include_directories(${CPPUNIT_INCLUDE_DIRS} ${CPPUNIT_SRC_DIR})

add_library(cppunit ${sources} ${headers})

# correct library names
set_target_properties(cppunit PROPERTIES
  DEBUG_POSTFIX "d"
  RELEASE_POSTFIX ""
  MINSIZEREL_POSTFIX ""
  RELWITHDEBINFO_POSTFIX ""
)
    
if(MSVC)
	### General stuff
	# a) Change MSVC runtime library settings (/MD[d], /MT[d], /ML[d] (single-threaded until VS 2003))
	#    plus set lib suffix for later use and project label accordingly
	# see http://msdn.microsoft.com/en-us/library/aa278396(v=VS.60).aspx
	#     http://msdn.microsoft.com/en-us/library/2kzt1wy3(v=VS.71).aspx
	set(LIB_RT_SUFFIX "md")	# CMake defaults to /MD for MSVC
	set(LIB_RT_OPTION "/MD")
	#  
  
	if(NOT MSVC_SHARED_RT)	# User wants to have static runtime libraries (/MT, /ML)
		if(MSVC_STHREADED_RT)	# User wants to have old single-threaded static runtime libraries
			set(LIB_RT_SUFFIX "ml")
			set(LIB_RT_OPTION "/ML")
			if(NOT ${MSVC_VERSION} LESS 1400)
				message(FATAL_ERROR "Single-threaded static runtime libraries (/ML) only available until VS .NET 2003 (7.1).")
			endif()
		else()
			set(LIB_RT_SUFFIX "mt")
			set(LIB_RT_OPTION "/MT")
		endif()

		# correct linker options
		foreach(flag_var  CMAKE_C_FLAGS  CMAKE_CXX_FLAGS)
			foreach(config_name  ""  DEBUG  RELEASE  MINSIZEREL  RELWITHDEBINFO)
				set(var_name "${flag_var}")
				if(NOT "${config_name}" STREQUAL "")
					set(var_name "${var_name}_${config_name}")
				endif()
				string(REPLACE "/MD" "${LIB_RT_OPTION}" ${var_name} "${${var_name}}")
			endforeach()
		endforeach()
	endif()
endif(MSVC)


