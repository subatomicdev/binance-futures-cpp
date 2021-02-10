cmake_minimum_required (VERSION 3.8)

include_directories("../../vcpkg_linux/packages/cpprestsdk_x64-linux/include")
include_directories("../../vcpkg_linux/packages/redis-plus-plus_x64-linux/include")
include_directories("../../vcpkg_linux/packages/poco_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-asio_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-config_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-system_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-winapi_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-predef_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-date-time_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-throw-exception_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-assert_x64-linux/include")
include_directories("../../vcpkg_linux/packages/openssl_x64-windows-static/include")
include_directories("../binancewslib")


LINK_DIRECTORIES("../../vcpkg_linux/packages/poco_x64-linux/lib")
LINK_DIRECTORIES("../../vcpkg_linux/packages/cpprestsdk_x64-linux/lib")
LINK_DIRECTORIES("../../vcpkg_linux/packages/redis-plus-plus_x64-linux/lib")
LINK_DIRECTORIES("../../vcpkg_linux/packages/boost-system_x64-linux/lib")
LINK_DIRECTORIES("../../vcpkg_linux/packages/boost-date-time_x64-linux/lib")
LINK_DIRECTORIES("../../vcpkg_linux/packages/openssl_x64-windows-static/lib")


# Add source to this project's executable.
add_executable (binancews "binancews.cpp")

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MT")

target_link_libraries(binancews -lPocoFoundation -lpthread -lredis++ -lcrypto -lssl -lboost_system -lboost_date_time -lcpprest)
target_link_libraries(binancews binancewslib)

