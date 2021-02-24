#ifndef __BINANCE_LOGGER_HPP 
#define __BINANCE_LOGGER_HPP

#include <string>
#include <iostream>
#include <future>
#include <iostream>
#include <Poco/DateTimeFormatter.h>
#include <Poco/LocalDateTime.h>

#include "binancewsCommon.hpp"

namespace binancews
{
    enum class LogLevel { LogDebug, LogError };

#ifdef _DEBUG
    static const LogLevel Level = LogLevel::LogDebug;
#else
    static const LogLevel Level = LogLevel::LogDebug; ;// LogLevel::LogError;
#endif
    

    inline void logg(const std::string& str, const LogLevel l = LogLevel::LogDebug)
    {
        static std::mutex mux;

        if (!str.empty() && l == Level)
        {
            std::scoped_lock lock(mux);
            std::cout << "[" << dateTimeToString(Poco::LocalDateTime{}) << "] " << str << "\n";
        }
    }
}

#endif
