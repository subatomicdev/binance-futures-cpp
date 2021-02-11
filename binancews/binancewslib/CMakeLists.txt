
include_directories("../../vcpkg_linux/installed/x64-linux/include")

add_library(binancewslib STATIC "BinanceExchange.hpp" "binancewsCommon.hpp" "Logger.hpp" "Redis.hpp")

SET_TARGET_PROPERTIES(binancewslib PROPERTIES LINKER_LANGUAGE CXX)
SET_TARGET_PROPERTIES(binancewslib PROPERTIES CXX_STANDARD 17)
