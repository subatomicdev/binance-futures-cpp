


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
        string redisIp;
        int redisPort = 6379;

        if (argc == 3)
        {
            redisIp = argv[1];
            redisPort = std::stoi(argv[2]);
        }
        else
        {
            logg("Not using Redis to do use: [RedisIP] [RedisPort] command line args, i.e.:");
            logg("./binancews 192.168.10.10 6379");
        }


        // create and connect to Redis
        shared_ptr<Redis> redis;

        if (!redisIp.empty())
        {
            redis = std::make_shared<Redis>();
            redis->init(redisIp, redisPort);
        }


        auto onAllSymbolsDataFunc = [redis] (std::map<std::string, std::string> data)
        {
            static string ChannelNameStart = "binance_";
            static string ChannelNameEnd = "_EXCHANGE_INSTRUMENT_PRICE_CHANNEL";

            std::stringstream ss;

            if (redis)
            {
                ss << "Publishing " << data.size() << " symbol updates";
                logg(ss.str());

                for (const auto& sym : data)
                {
                    redis->publish(ChannelNameStart + sym.first + ChannelNameEnd, { "{\"exchange\":\"binance\", \"instrument\":\"" + sym.first + "\", \"price\":\"" + sym.second + "\"}" });
                }
            }
            else
            {
                ss << "Received " << data.size() << " symbol updates";
                logg(ss.str());
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