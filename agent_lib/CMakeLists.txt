
set(AGENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(UNIX)
  execute_process(COMMAND uname OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CMAKE_SYSTEM_NAME)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(LINUX_LIBRARIES pthread)
  endif(CMAKE_SYSTEM_NAME MATCHES Linux)
endif(UNIX)

source_group("Source Files" FILES ${AGENT_SRCS})

file(GLOB AGENT_SRCS ${AGENT_SOURCE_DIR}/*.cpp)
file(GLOB AGENT_HEADERS ${AGENT_SOURCE_DIR}/*.hpp)

# configure a header file to pass some of the CMake settings
# to the source code
add_custom_target(UPDATE_VERSION ALL
	  COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${AGENT_SOURCE_DIR}/version.cpp")

configure_file(
	"${AGENT_SOURCE_DIR}/version.h.in"
	"${PROJECT_BINARY_DIR}/agent_lib/version.h" )

# Allow better viewing and grouping of files in Visual Studio by defining source groups
source_group("Headers Files" FILES ${AGENT_HEADERS})
source_group("Source Files" FILES ${AGENT_SRCS})
source_group("Dlib Source Files" FILES ${DLIB_FIX_SRCS})

add_library(agent_lib
  ${AGENT_SRCS}
  ${DLIB_FIX_SRCS}
  ${AGENT_HEADERS})

AddMsvcXPSupport(agent_lib)
AddLibXML2Support(agent_lib)
AddDLibSupport(agent_lib)
AddDateSupport(agent_lib)
AddJsonSupport(agent_lib)

target_include_directories(agent_lib
  PRIVATE ${PROJECT_BINARY_DIR}/agent_lib
  )

target_compile_definitions(agent_lib PRIVATE NOMINMAX)

if (MSVC)
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}" "/DLIBXML_STATIC")
endif()  

add_dependencies(agent_lib UPDATE_VERSION)

target_compile_features(agent_lib PRIVATE cxx_std_14)

