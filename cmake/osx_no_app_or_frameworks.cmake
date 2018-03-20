# Change how CMake find_* commands choose between OS X Application Bundles and unix-style package components.
# We will always prefer packages
#
set(CMAKE_FIND_APPBUNDLE NEVER)

# Change how CMake find_* commands choose between OS X frameworks and unix-style package components.
# We will always prefer packages
#
set(CMAKE_FIND_FRAMEWORK NEVER FORCE)
