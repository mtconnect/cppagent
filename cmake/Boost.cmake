if(NOT Boost_FOUND)
  if(WIN32)
    set(Boost_USE_MULTITHREADED ON CACHE BOOL "Boost thread support")
    set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Boost static libs")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "Boost static runtime")
  endif()
  
  find_package(Boost 1.72.0 REQUIRED COMPONENTS system exception filesystem thread chrono date_time regex)

  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_USE_DEBUG_LIBS         OFF) # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
find_package(Boost 1.72 REQUIRED COMPONENTS filesystem log_setup log system thread date_time REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION}")
message (STATUS ${Boost_LIBRARIES})
set_target_properties(${TARGET} PROPERTIES COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK")

