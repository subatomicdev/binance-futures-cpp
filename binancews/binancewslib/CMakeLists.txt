
include_directories("../../vcpkg_linux/installed/x64-linux/include")

add_library(binancewslib STATIC "BinanceExchange.hpp" "binancewsCommon.hpp" "Logger.hpp" "IntervalTimer.hpp" "IntervalTimer.cpp" "Market.cpp" "Market.hpp")

SET_TARGET_PROPERTIES(binancewslib PROPERTIES LINKER_LANGUAGE CXX)
SET_TARGET_PROPERTIES(binancewslib PROPERTIES CXX_STANDARD 17)

target_link_libraries(binancewslib -lcpprest -lssl -lcrypto -lz -ldl)