# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project (binancews C CXX)

# set_target_properties(binancews PROPERTIES CXX_STANDARD 17)

# Include sub-projects.
add_subdirectory("binancews")
add_subdirectory("binancewslib")


