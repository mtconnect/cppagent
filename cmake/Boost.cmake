if(NOT Boost_FOUND)
  if(WIN32)
    set(Boost_USE_MULTITHREADED ON CACHE BOOL "Boost thread support")
    set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Boost static libs")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "Boost static runtime")
  endif()
  
  find_package(Boost 1.72.0 REQUIRED COMPONENTS system exception filesystem thread chrono date_time regex)

  include_directories(${Boost_INCLUDE_DIRS})
endif()
