if(WIN32)
  set(Boost_USE_STATIC_LIBS OFF FORCE)
  set(Boost_USE_MULTITHREADED ON FORCE)
  set(Boost_USE_STATIC_RUNTIME OFF FORCE)
  set(Boost_USE_DEBUG_LIBS         OFF FORCE) # ignore debug libs and
  set(Boost_USE_RELEASE_LIBS       ON FORCE)  # only find release libs
else()
    set(Boost_USE_MULTITHREADED ON CACHE BOOL "Boost thread support")
    set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Boost static libs")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "Boost static runtime")  
endif()
find_package(Boost 1.72 REQUIRED COMPONENTS log_setup log REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION}")
message (STATUS ${Boost_LIBRARIES})
set_target_properties(${TARGET} PROPERTIES COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK")

include_directories(${Boost_INCLUDE_DIRS})

