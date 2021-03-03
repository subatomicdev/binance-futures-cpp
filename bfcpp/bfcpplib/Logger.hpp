#pragma once

#include <string>
#include <iostream>
#include <future>
#include <iostream>
#include <Poco/DateTimeFormatter.h>
#include <Poco/LocalDateTime.h>

#include "bfcppCommon.hpp"

namespace bfcpp 
{
    enum class LogLevel { LogDebug, LogError };

#ifdef _DEBUG
    static const LogLevel Level = LogLevel::LogDebug;
#else
    static const LogLevel Level = LogLevel::LogDebug; ;// LogLevel::LogError;
#endif
    
    inline string timePointToString(const PGClock::time_point& tp)
    {
       auto tm_t = PGClock::to_time_t(tp);
       auto tm = std::localtime(&tm_t);
       std::stringstream ss;
       ss << std::put_time(tm, "%H:%M:%S");
       return ss.str();
    }


    inline string dateTimeToString(Poco::LocalDateTime ldt)
    {
       return Poco::DateTimeFormatter::format(ldt, "%H:%M:%S.%i");
    }


    inline void logg(const std::string& str, const LogLevel l = LogLevel::LogDebug)
    {
        static std::mutex mux;

        if (!str.empty() && l == Level)
        {
            std::scoped_lock lock(mux);
            std::cout << "[" << dateTimeToString(Poco::LocalDateTime{}) << "] " << str << "\n";
        }
    }

    inline void logg(std::string&& str, const LogLevel l = LogLevel::LogDebug)
    {
       static std::mutex mux;

       if (!str.empty() && l == Level)
       {
          std::scoped_lock lock(mux);
          std::cout << "[" << dateTimeToString(Poco::LocalDateTime{}) << "] " << str << "\n";
       }
    }
}