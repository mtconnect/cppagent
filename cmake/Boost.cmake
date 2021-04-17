set(Boost_USE_MULTITHREADED ON CACHE BOOL "Boost thread support" FORCE)
set(Boost_USE_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
 
find_package(Boost 1.75 COMPONENTS log_setup log coroutine REQUIRED)
set_target_properties(${TARGET} PROPERTIES COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK")

include_directories(${Boost_INCLUDE_DIRS})

