cmake_minimum_required (VERSION 3.8)

include_directories("../../vcpkg_linux/installed/x64-linux/include")
include_directories("../bfcpplib")


LINK_DIRECTORIES("../../vcpkg_linux/installed/x64-linux/lib")


# Add source to this project's executable.
add_executable (bfcpptest "bfcpptest.cpp")

set_target_properties(bfcpptest PROPERTIES CXX_STANDARD 17)

target_link_libraries(bfcpptest -lPocoFoundation -lpthread -lboost_system -lboost_date_time -lcpprest -lssl -lcrypto -lz -ldl)
target_link_libraries(bfcpptest bfcpplib)

