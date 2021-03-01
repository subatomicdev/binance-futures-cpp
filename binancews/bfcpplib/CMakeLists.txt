
include_directories("../../vcpkg_linux/installed/x64-linux/include")

add_library(bfcpplib STATIC "bfcppCommon.hpp" "Logger.hpp" "IntervalTimer.hpp" "IntervalTimer.cpp" "Market.cpp" "Market.hpp" "Futures.hpp")

SET_TARGET_PROPERTIES(bfcpplib PROPERTIES LINKER_LANGUAGE CXX)
SET_TARGET_PROPERTIES(bfcpplib PROPERTIES CXX_STANDARD 17)

target_link_libraries(bfcpplib -lcpprest -lssl -lcrypto -lz -ldl)