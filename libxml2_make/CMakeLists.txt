cmake_minimum_required(VERSION 2.6)

project(libxml2 C)

set(LibXML2_SRC_DIR ../libxml2)
set(LibXML2_INCLUDE_DIRS ../libxml2/include ./include)

file(GLOB internal_headers ${LibXML2_SRC_DIR}/[a-z_]*.h)
file(GLOB install_headers ${LibXML2_INCLUDE_DIRS}/libxml2/[a-z_]*.h)
set(sources ${LibXML2_SRC_DIR}/buf.c
        	${LibXML2_SRC_DIR}/c14n.c
        	${LibXML2_SRC_DIR}/catalog.c
        	${LibXML2_SRC_DIR}/chvalid.c
        	${LibXML2_SRC_DIR}/debugXML.c
        	${LibXML2_SRC_DIR}/dict.c
        	${LibXML2_SRC_DIR}/DOCBparser.c
        	${LibXML2_SRC_DIR}/encoding.c
        	${LibXML2_SRC_DIR}/entities.c
        	${LibXML2_SRC_DIR}/error.c
        	${LibXML2_SRC_DIR}/globals.c
        	${LibXML2_SRC_DIR}/hash.c
        	${LibXML2_SRC_DIR}/HTMLparser.c
        	${LibXML2_SRC_DIR}/HTMLtree.c
        	${LibXML2_SRC_DIR}/legacy.c
        	${LibXML2_SRC_DIR}/list.c
        	${LibXML2_SRC_DIR}/nanoftp.c
        	${LibXML2_SRC_DIR}/nanohttp.c
        	${LibXML2_SRC_DIR}/parser.c
        	${LibXML2_SRC_DIR}/parserInternals.c
        	${LibXML2_SRC_DIR}/pattern.c
        	${LibXML2_SRC_DIR}/relaxng.c
        	${LibXML2_SRC_DIR}/SAX2.c
        	${LibXML2_SRC_DIR}/SAX.c
        	${LibXML2_SRC_DIR}/schematron.c
        	${LibXML2_SRC_DIR}/threads.c
        	${LibXML2_SRC_DIR}/tree.c
        	${LibXML2_SRC_DIR}/uri.c
        	${LibXML2_SRC_DIR}/valid.c
        	${LibXML2_SRC_DIR}/xinclude.c
        	${LibXML2_SRC_DIR}/xlink.c
        	${LibXML2_SRC_DIR}/xmlIO.c
        	${LibXML2_SRC_DIR}/xmlmemory.c
        	${LibXML2_SRC_DIR}/xmlreader.c
        	${LibXML2_SRC_DIR}/xmlregexp.c
        	${LibXML2_SRC_DIR}/xmlmodule.c
        	${LibXML2_SRC_DIR}/xmlsave.c
        	${LibXML2_SRC_DIR}/xmlschemas.c
        	${LibXML2_SRC_DIR}/xmlschemastypes.c
        	${LibXML2_SRC_DIR}/xmlunicode.c
        	${LibXML2_SRC_DIR}/xmlwriter.c
        	${LibXML2_SRC_DIR}/xpath.c
        	${LibXML2_SRC_DIR}/xpointer.c
            ${LibXML2_SRC_DIR}/xmlstring.c)


include_directories(${LibXML2_INCLUDE_DIRS} ${LibXML2_SRC_DIR})

add_library(libxml2 ${sources} ${headers})

# correct library names
set_target_properties(libxml2 PROPERTIES
  DEBUG_POSTFIX "d"
  RELEASE_POSTFIX ""
  MINSIZEREL_POSTFIX ""
  RELWITHDEBINFO_POSTFIX ""
)
    
add_definitions(/D HAVE_WIN32_THREADS)
    
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


