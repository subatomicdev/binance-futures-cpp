


#include <iostream>
#include <future>

#include <Futures.hpp>
#include <Spot.hpp>
#include <Logger.hpp>


using namespace std::chrono_literals;
using namespace binancews;


auto handleKeyMultipleValueData = [](Market::BinanceKeyMultiValueData data)
{
    std::stringstream ss;

    for (const auto& s : data.values)
    {
        ss << "\n" << s.first << "\n{";

        for (auto& value : s.second)
        {
            ss << "\n\t" << value.first << "=" << value.second;
        }

        ss << "\n}";
    }

    logg(ss.str());
};


auto handleKeyValueData = [](Market::BinanceKeyValueData data)
{
    for (const auto& p : data.values)
    {
        logg(p.first + "=" + p.second);
    }
};


auto handleUserDataSpot = [](Market::SpotUserData data)
{
    for (const auto& p : data.data)
    {
        logg(p.first + "=" + p.second);
    }

    if (data.type == Market::SpotUserData::EventType::AccountUpdate)
    {
        std::stringstream ss;

        for (auto& asset : data.au.balances)
        {
            ss << "\n" << asset.first << "\n{"; // asset symbol

            for (const auto& balance : asset.second)
            {
                ss << "\n\t" << balance.first << "=" << balance.second;   // the asset symbol, free and locked values for this symbol
            }

            ss << "\n}";
        }

        logg(ss.str());
    }
};


auto handleUserDataUsdFutures = [](Market::UsdFutureUserData data)
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
};



/// <summary>
/// A simple function to receive mark price from the Future's market for all symbols.
/// </summary>
void markPrice()
{
    std::cout << "\n\n--- Mark Price ---\n";

    UsdFuturesMarket usdFutures;

    usdFutures.monitorMarkPrice(handleKeyMultipleValueData);

    std::this_thread::sleep_for(10s);
}


/// <summary>
/// Shows it's easy to receive multiple streams.
/// </summary>
void multipleStreams()
{
    std::cout << "\n\n--- Multiple Streams on Futures ---\n";

    UsdFuturesMarket usdFutures;

    usdFutures.monitorMarkPrice(handleKeyMultipleValueData);
    usdFutures.monitorMiniTicker(handleKeyMultipleValueData);

    std::this_thread::sleep_for(10s);
}


/// <summary>
/// Receive user data from the Future's TestNet market: https://testnet.binancefuture.com/en/futures/BTC_USDT
/// Note, they seem to disconnect clients after a few orders are created/closed.
/// </summary>
/// <param name="apiKey">Create an account on the above URL. This key is available at the bottom of the page</param>
/// <param name="secretKey">Create an account on the above URL. This key is available at the bottom of the page</param>
void usdFutureTestNetDataStream(const string& apiKey, const string& secretKey)
{
    std::cout << "\n\n--- USD-M Futures TestNet User Data ---\n";
    std::cout << "You must create/cancel etc an order for anything to show here\n";

    UsdFuturesTestMarket futuresTest;
    futuresTest.monitorUserData(apiKey, secretKey, handleUserDataUsdFutures);

    std::this_thread::sleep_for(10s);
}


/// <summary>
/// Receive from the real Future market.
/// </summary>
/// <param name="apiKey"></param>
/// <param name="secretKey"></param>
void usdFutureDataStream(const string& apiKey, const string& secretKey)
{
    std::cout << "\n\n--- USD-M Futures User Data ---\n";
    std::cout << "You must create/cancel etc an order for anything to show here\n";

    UsdFuturesMarket futures;
    futures.monitorUserData(apiKey, secretKey, handleUserDataUsdFutures);

    std::this_thread::sleep_for(10s);
}


void spotTestNetDataStream(const string& apiKey, const string& secretKey)
{
    std::cout << "\n\n--- Spot TestNet User Data ---\n";
    std::cout << "You must create/cancel etc an order for anything to show here\n";

    SpotTestMarket spotTest;
    spotTest.monitorUserData(apiKey, secretKey, handleUserDataSpot);

    std::this_thread::sleep_for(10s);
}

void spotDataStream(const string& apiKey, const string& secretKey)
{
    std::cout << "\n\n--- Spot User Data ---\n";
    std::cout << "You must create/cancel etc an order for anything to show here\n";

    SpotMarket spot;
    spot.monitorUserData(apiKey, secretKey, handleUserDataSpot);

    std::this_thread::sleep_for(300s);
}


int main(int argc, char** argv)
{
    try
    {
        // spot testnet
        const string apiKeySpotTest = "";
        const string secretKeySpotTest = "";
        // spot 'real'
        const string apiKeySpotMarket = "";
        const string secretKeySpotMarket = "";
        // futures testnet
        const string apiKeyUsdFuturesTest = "";
        const string secretKeyUsdFuturesTest = "";
        // futures 'real'
        const string apiKeyUsdFutures = "";
        const string secretKeyUsdFutures = "";



        // NOTE 
        //  1. if a function does not take api/secret keys as args, you can run without
        //  2. these functions are synchronous


        markPrice();

        //multipleStreams();

        //usdFutureTestNetDataStream(apiKeyUsdFuturesTest, secretKeyUsdFuturesTest);

        //usdFutureDataStream(apiKeyUsdFutures, secretKeyUsdFutures);

        //spotTestNetDataStream(apiKeySpotTest, secretKeySpotTest);

        //spotDataStream(apiKeySpotMarket, secretKeySpotMarket);
    }
    catch (const std::exception ex)
    {
        logg(ex.what());
    }

    return 0;
}
