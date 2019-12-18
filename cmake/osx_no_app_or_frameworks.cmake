# Change how CMake find_* commands choose between OS X Application Bundles and unix-style package
# components.
# We will always prefer packages
set(CMAKE_FIND_APPBUNDLE NEVER)
set(CMAKE_FIND_FRAMEWORK NEVER FORCE)

# We need to use the CLang C++14 compiler for the agent on OS X since we are using the
# ms suffix which was added post 11.
if (APPLE)
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedef -Wno-deprecated-declarations")
endif()
