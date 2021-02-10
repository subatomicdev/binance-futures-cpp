


#include <iostream>
#include <future>

#include <cpprest/ws_client.h>
#include <cpprest/json.h>

#include <BinanceExchange.hpp>
#include <Logger.hpp>
#include <Redis.hpp>


using namespace std::chrono_literals;
using namespace binancews;


int main(int argc, char** argv)
{
    try
    {
        // create and connect to Redis
        auto redis = std::make_shared<Redis>();
        redis->init("172.22.253.65", 7379);


        auto onAllSymbolsDataFunc = [redis] (std::map<std::string, std::string> data)
        {
            static string ChannelNameStart = "binance_";
            static string ChannelNameEnd = "_EXCHANGE_INSTRUMENT_PRICE_CHANNEL";

            std::stringstream ss;
            ss << "Publishing " << data.size() << " symbol updates";
            logg(ss.str());


            for (const auto& sym : data)
            {
                web::json::value exchangeValue ;
                exchangeValue[utility::conversions::to_string_t("exchange")] = web::json::value{ L"binance" };

                web::json::value instrumentValue;
                instrumentValue[utility::conversions::to_string_t("instrument")] = web::json::value{ utility::conversions::to_string_t(sym.first) };

                web::json::value priceValue;
                priceValue[utility::conversions::to_string_t("price")] = web::json::value{ utility::conversions::to_string_t(sym.second) };

                web::json::value val;
                val[0] = exchangeValue;
                val[1] = instrumentValue;
                val[2] = priceValue;

                auto wideString = val.serialize();
                auto asString = std::string{ wideString.cbegin(), wideString.cend() };

                redis->publish(ChannelNameStart + sym.first + ChannelNameEnd, asString);
            }
        };


        auto consoleFuture = std::async(std::launch::async, []()
        {
            bool run = true;
            std::string cmd;
            while (run && std::getline(std::cin, cmd))
            {
                run = (cmd != "stop");
            }
        });


        Binance be;
        if (auto allSymbolsToken = be.monitorAllSymbols(onAllSymbolsDataFunc) ;  allSymbolsToken.isValid())
        {
            consoleFuture.wait();
        }
        else
        {
            logg("Failed to create monitor for All Symbols");
        }        
    }
    catch (const std::exception ex)
    {
        logg(ex.what());
    }

    
    return 0;
}