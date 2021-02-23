


#include <iostream>
#include <future>

#include <BinanceExchange.hpp>
#include <Logger.hpp>


using namespace std::chrono_literals;
using namespace binancews;


std::atomic_size_t count;
std::atomic_bool silent = false;


auto handleKeyMultipleValueData = [](Market::BinanceKeyMultiValueData data)
{
    if (!silent)
    {
        std::stringstream ss;

        for (auto& s : data.values)
        {
            ss << "\n" << s.first << "\n{";

            for (auto& value : s.second)
            {
                ss << "\n\t" << value.first << "=" << value.second;
            }

            ss << "\n}";
        }

        logg(ss.str());
    }

    count += data.values.size();
};


auto handleKeyValueData = [](Market::BinanceKeyValueData data)
{
    if (!silent)
    {
        for (auto& p : data.values)
        {
            logg(p.first + "=" + p.second);
        }
    }

    count += data.values.size();
};


auto handleUserDataSpot = [](Market::SpotUserData data)
{
    count += data.data.size();

    if (!silent)
    {
        for (auto& p : data.data)
        {
            logg(p.first + "=" + p.second);
        }


        if (data.type == Market::SpotUserData::EventType::AccountUpdate)
        {
            std::stringstream ss;

            for (auto& asset : data.balances)
            {
                ss << "\n" << asset.first << "\n{"; // asset symbol

                for (const auto& balance : asset.second)
                {
                    ss << "\n\t" << asset.first << "=" << balance.second;   // the asset symbol, free and locked values for this symbol
                }

                ss << "\n}";
            }

            logg(ss.str());
        }
    }
};


auto handleUserDataUsdFutures = [](Market::UsdFutureUserData data)
{
    count += data.mc.data.size();

    if (!silent)
    {
        if (data.type == Market::UsdFutureUserData::EventType::MarginCall)
        {
            std::stringstream ss;
            ss << "\nMargin Call\n{";

            for (auto& p : data.mc.data)
            {
                logg(p.first + "=" + p.second);
            }

            for (auto& asset : data.mc.positions)
            {
                ss << "\n" << asset.first << "\n{"; // asset symbol

                for (const auto& balance : asset.second)
                {
                    ss << "\n\t" << balance.first << "=" << balance.second;   // the asset symbol, free and locked values for this symbol
                }

                ss << "\n}";
            }

            ss << "\n}";

            logg(ss.str());
        }
        else if (data.type == Market::UsdFutureUserData::EventType::OrderUpdate)
        {
            std::stringstream ss;
            ss << "\nOrder Update\n{";

            for (auto& p : data.ou.data)
            {
                ss << "\n" << p.first << "=" << p.second;
            }

            for (auto& asset : data.ou.orders)
            {
                ss << "\n" << asset.first << "\n{"; // asset symbol

                for (const auto& order : asset.second)
                {
                    ss << "\n\t" << order.first << "=" << order.second;
                }

                ss << "\n}";
            }

            ss << "\n}";

            logg(ss.str());
        }
        else if (data.type == Market::UsdFutureUserData::EventType::AccountUpdate)
        {
            std::stringstream ss;
            ss << "\nAccount Update\n{";

            for (auto& p : data.au.data)
            {
                ss << "\n" << p.first << "=" << p.second;
            }

            ss << "\nReason: " << data.au.reason;

            ss << "\nBalances:";
            for (const auto& balance : data.au.balances)
            {
                for (auto& asset : balance)
                {
                    ss << "\n\t" << asset.first << "=" << asset.second;
                }
            }

            ss << "\nPositions:";
            for (const auto& positions : data.au.positions)
            {
                for (auto& asset : positions)
                {
                    ss << "\n\t" << asset.first << "=" << asset.second;
                }
            }

            ss << "\n}";

            logg(ss.str());
        }
    }
};




void markPrice()
{
    std::cout << "\n\n--- Mark Price ---\n";

    count = 0;

    UsdFuturesMarket usdFutures;

    usdFutures.monitorMarkPrice(handleKeyMultipleValueData);

    std::this_thread::sleep_for(10s);
}


void multipleStreams()
{
    std::cout << "\n\n--- Multiple Streams on Futures ---\n";

    count = 0;

    UsdFuturesMarket usdFutures;

    usdFutures.monitorMarkPrice(handleKeyMultipleValueData);
    usdFutures.monitorMiniTicker(handleKeyMultipleValueData);

    std::this_thread::sleep_for(10s);
}


void usdFutureTestNetDataStream(const string& apiKey, const string& secretKey)
{
    std::cout << "\n\n--- USD-M Futures TestNet User Data ---\n";
    std::cout << "You must create/cancel etc an order for anything to show here\n";

    UsdFuturesTestMarket futuresTest;
    futuresTest.monitorUserData(apiKey, secretKey, handleUserDataUsdFutures);

    std::this_thread::sleep_for(10s);
}


void usdFutureDataStream(const string& apiKey, const string& secretKey)
{
    std::cout << "\n\n--- USD-M Futures TestNet User Data ---\n";
    std::cout << "You must create/cancel etc an order for anything to show here\n";

    UsdFuturesMarket futures;
    futures.monitorUserData(apiKey, secretKey, handleUserDataUsdFutures);

    std::this_thread::sleep_for(10s);
}


int main(int argc, char** argv)
{
    try
    {
        /*
        std::cout << "Commands:\nstop : exit\nsilent (s): no output to console\nverbose (v): output to console\n\n";
        auto consoleFuture = std::async(std::launch::async, []()
        {
            bool run = true;
            std::string cmd;
            while (run && std::getline(std::cin, cmd))
            {
                run = (cmd != "stop");
                
                if (cmd == "silent" || cmd == "s")
                    silent = true;
                else if (cmd == "verbose" || cmd == "v")
                    silent = false;
            }
        });*/
        
        
        // api and secret keys
        const string apiKeyUsdFuturesTest = "";
        const string secretKeyUsdFuturesTest = "";

        const string apiKeySpotMarket = "";
        const string secretKeySpotMarket = "";

        const string apiKeyUsdFutures = "";
        const string secretKeyUsdFutures = "";



        // NOTE 
        //  1. if a function does take api/secret keys as args, you can run without
        //  2. these functions are synchronous



        markPrice();

        //multipleStreams();
        
        //usdFutureTestNetDataStream(apiKeyUsdFuturesTest, secretKeyUsdFuturesTest);

        //usdFutureDataStream(apiKeyUsdFutures, secretKeyUsdFutures);
        
    }
    catch (const std::exception ex)
    {
        logg(ex.what());
    }

    
    return 0;
}
