#pragma once

#define NOMINMAX

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


    /// Everything related to an instrument
    struct Symbol
    {
        Symbol() : symbol(), bidPrice(0), bidQty(0), askPrice(0), askQty(0), price(0), lastUpdate(PGClock::now())
        {

        }
        Symbol(const string& s) : symbol(s), bidPrice(0),bidQty(0), askPrice(0), askQty(0), price(0), lastUpdate(PGClock::now())
        {

        }
        Symbol(const string& s, double currentPrice) : Symbol(s)
        {
            price = currentPrice;
        }
        Symbol(string&& s, double&& bidP, double&& bidQ, double&& askP, double&& askQ, double&& price)
            :   symbol(s),  bidPrice(std::move(bidP)), bidQty(std::move(bidQ)),
                            askPrice(std::move(askP)), askQty(std::move(askQ)),
                            price(std::move(price)), lastUpdate(PGClock::now())
        {

        }
        
        bool isNull() { return symbol.empty(); }
        
        string symbol;
        
        double bidPrice;
        double bidQty;

        double askPrice;
        double askQty;

        double price;
        PGClock::time_point lastUpdate;
    };


    struct SymbolUpdate
    {
        SymbolUpdate() : receivedTime(PGClock::now())
        {

        }
        SymbolUpdate(Symbol&& sym, PGClock::time_point&& rcvdTime = PGClock::now()) : symbol(std::move(sym)), receivedTime(std::move(rcvdTime))
        {

        }

        Symbol symbol;
        PGClock::time_point receivedTime;
    };

    
    struct StartUpParams
    {
        map<string, string> params;
        vector<string> argv;
    };


    struct ExchangeCreditentials
    {
        virtual ~ExchangeCreditentials() = default;
    };

    struct BinanceCreditentials : public ExchangeCreditentials
    {
        BinanceCreditentials()
        {

        }
        
        BinanceCreditentials(const string& api, const string& secret, const fs::path cert) : apiKey(api), secretKey(secret), sslCertPath(cert)
        {

        }


        string apiKey;
        string secretKey;
        fs::path sslCertPath;
    };


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