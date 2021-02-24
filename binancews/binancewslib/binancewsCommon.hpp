#ifndef __BINANCE_COMMON_HPP 
#define __BINANCE_COMMON_HPP

#include <functional>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <future>
#include <chrono>
#include <sstream>

#include <Poco/Notification.h>



namespace binancews
{
    using std::shared_ptr;
    using std::vector;
    using std::string;
    using std::map;
    using std::future;

    namespace fs = std::filesystem;

    using PGClock = std::chrono::system_clock;


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


    template <typename T>
    string toString(const T a_value, const int n = 6)
    {
        std::ostringstream out;
        out.precision(n);
        out << std::fixed << a_value;
        return out.str();
    }
}

#endif
