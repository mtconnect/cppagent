if(WIN32)
  set(Boost_USE_DEBUG_LIBS         OFF CACHE BOOL "" FORCE) # ignore debug libs and
  set(Boost_USE_RELEASE_LIBS       ON CACHE BOOL "" FORCE)  # only find release libs
endif()

set(Boost_USE_MULTITHREADED ON CACHE BOOL "Boost thread support" FORCE)
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
 
find_package(Boost 1.72 REQUIRED COMPONENTS log_setup log REQUIRED)
set_target_properties(${TARGET} PROPERTIES COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK")

include_directories(${Boost_INCLUDE_DIRS})

