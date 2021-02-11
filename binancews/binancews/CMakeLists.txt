cmake_minimum_required (VERSION 3.8)

include_directories("../../vcpkg_linux/installed/x64-linux/include")
include_directories("../binancewslib")


LINK_DIRECTORIES("../../vcpkg_linux/installed/x64-linux/lib")


# Add source to this project's executable.
add_executable (binancews "binancews.cpp")

set_target_properties(binancews PROPERTIES CXX_STANDARD 17)

target_link_libraries(binancews -lPocoFoundation -lpthread -lredis++ -lboost_system -lboost_date_time -lcpprest -lhiredis -lssl -lcrypto -lz -ldl)
target_link_libraries(binancews binancewslib)

