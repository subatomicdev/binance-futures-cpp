


#include <iostream>
#include <future>

#include <BinanceExchange.hpp>
#include <Logger.hpp>


using namespace std::chrono_literals;
using namespace binancews;


std::atomic_size_t count;



int main(int argc, char** argv)
{
    try
    {
        // flags
        std::atomic_bool silent = false;


        auto handleKeyValueData = [&silent](Market::BinanceKeyValueData data)
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


        auto handleKeyMultipleValueData = [&silent](Market::BinanceKeyMultiValueData data)
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

                ++count;
            }

            if (!silent)
            {
                logg(ss.str());
            }                     
        };

        
        auto handleUserDataSpot = [&silent](Market::UserDataStreamData data)
        {
            count += data.data.size();

            if (!silent)
            {
                for (auto& p : data.data)
                {
                    logg(p.first + "=" + p.second);                    
                }


                if (data.type == Market::UserDataStreamData::EventType::AccountUpdate)
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



        auto consoleFuture = std::async(std::launch::async, [&silent]()
        {
            std::cout << "Commands:\nstop : exit\nsilent (s): no output to console\nverbose (v): output to console";
                
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
        });


        
        count = 0;

        // USD futures
        {
            UsdFuturesMarket usdFutures;

            usdFutures.monitorMarkPrice(handleKeyMultipleValueData);
            usdFutures.monitorMiniTicker(handleKeyMultipleValueData);

            std::cout << "\n--- Futures";
            std::this_thread::sleep_for(10s);
            std::cout << "\nReceived " << count.load() << " updates on 2 streams in 10s";
        }
        

        count = 0;
        
        // spot
        {
            SpotMarket spot;
            spot.monitorMiniTicker(handleKeyMultipleValueData);

            std::cout << "\n--- Spot";
            std::this_thread::sleep_for(10s);

            std::cout << "\nReceived " << count.load() << " updates on 1 stream in 10s";
        }
        

        count = 0;

        // Use Market base ptr
        {
            shared_ptr<Market> usdFutures = std::make_shared<UsdFuturesMarket>();

            usdFutures->monitorMiniTicker(handleKeyMultipleValueData);

            std::cout << "\n--- Futures";
            std::this_thread::sleep_for(10s);
            std::cout << "\nReceived " << count.load() << " updates on 1 stream in 10s";
        }


        count = 0;

        // Use Market base ptr
        {
            shared_ptr<Market> spot = std::make_shared<SpotMarket>();

            spot->monitorMiniTicker(handleKeyMultipleValueData);

            std::cout << "\n--- Spot";
            std::this_thread::sleep_for(10s);
            std::cout << "\nReceived " << count.load() << " updates on 1 stream in 10s";
        }
        


        std::cout << "\n\n-------\nDone. Type 'stop' and press enter to exit";

        consoleFuture.wait();
    }
    catch (const std::exception ex)
    {
        logg(ex.what());
    }

    
    return 0;
}
