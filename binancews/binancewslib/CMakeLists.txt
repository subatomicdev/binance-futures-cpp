
include_directories("../../vcpkg_linux/packages/cpprestsdk_x64-linux/include")
include_directories("../../vcpkg_linux/packages/redis-plus-plus_x64-linux/include")
include_directories("../../vcpkg_linux/packages/poco_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-asio_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-config_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-system_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-winapi_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-predef_x64-linux/include")
include_directories("../../vcpkg_linux/packages/boost-date-time_x64-linux/include")
include_directories("../../vcpkg_linux/packages/openssl_x64-windows-static/include")



add_library(binancewslib STATIC "BinanceExchange.hpp" "binancewsCommon.hpp" "Logger.hpp" "Redis.hpp")

SET_TARGET_PROPERTIES(binancewslib PROPERTIES LINKER_LANGUAGE CXX)
SET_TARGET_PROPERTIES(binancewslib PROPERTIES CXX_STANDARD 17)

#add_library(binancewslib STATIC)
#target_sources(binancewslib PUBLIC "BinanceExchange.hpp" binancewsCommon.hpp Logger.hpp Redis.hpp)

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

