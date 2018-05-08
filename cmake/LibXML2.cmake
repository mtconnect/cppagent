# This module allows the addition of LibXML2 to a target
# Depending on or compiler this is either via our sub-module or searching
# for a package on the system

if(MSVC)
    add_subdirectory(${CMAKE_SOURCE_DIR}/libxml2_make "${CMAKE_BINARY_DIR}/libxml2")
endif(MSVC)

#################################################################################
#																				#
#								Functions										#
#																				#
#################################################################################

function(AddLibXML2Support projectTarget)
	
	if(WIN32)
		# For Windows we compile our own version of LibXML2 from our Git submodule source
		# Define a dependency between the targets and link them.
		set(LibXML2_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libxml2/include ${CMAKE_SOURCE_DIR}/libxml2_make/include)
		set(LibXML2_LIBRARIES libxml2)

		target_compile_definitions(${projectTarget} PRIVATE LIBXML_STATIC)
	elseif(UNIX)
		# We will use the system's installed packages; which we have to look for.
		find_package(LibXML2 REQUIRED)
		
		# Create a target from this source if not already done
		if(TARGET libxml2)
			add_library(libxml2 UNKNOWN IMPORTED)
		endif()
		
		set_property(${projectTarget} libxml2 PROPERTY IMPORTED_LOCATION ${LibXML2_LIBRARIES})
	endif()
	
	add_dependencies(${projectTarget} libxml2)
	target_include_directories(${projectTarget} PRIVATE ${LibXML2_INCLUDE_DIRS})
	target_link_libraries(${projectTarget} PRIVATE ${LibXML2_LIBRARIES})
	target_compile_definitions(${projectTarget} PRIVATE ${LibXML2_DEFINITIONS})
endfunction()