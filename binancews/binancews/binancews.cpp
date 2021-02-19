


#include <iostream>
#include <future>

#include <BinanceExchange.hpp>
#include <Logger.hpp>


using namespace std::chrono_literals;
using namespace binancews;


int main(int argc, char** argv)
{
    try
    {
        // flags
        std::atomic_bool silent = false;


        auto handleKeyValueData = [&silent](Binance::BinanceKeyValueData data)
        {
            if (!silent)
            {
                for (auto& p : data.values)
                {
                    logg(p.first + "=" + p.second);
                }
            }
        };


        auto handleKeyMultipleValueData = [&silent](Binance::BinanceKeyMultiValueData data)
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
        };

        
        auto handleUserDataSpot = [&silent](Binance::UserDataStreamData data)
        {
            if (!silent)
            {
                for (auto& p : data.data)
                {
                    logg(p.first + "=" + p.second);
                }

                if (data.type == Binance::UserDataStreamData::EventType::AccountUpdate)
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


        Binance be;
        
        be.monitorUserData("YOUR API KEY", handleUserDataSpot);

        //if (auto valid = be.monitorTradeStream("grtusdt", handleKeyValueData); !valid.isValid())
        //{
            //logg("monitorTradeStream failed");
        //}

        //if (auto valid = be.monitorAllSymbols(handleKeyMultipleValueData); !valid.isValid())
        //{
            //logg("monitorAllSymbols failed");
        //}

        //if (auto valid = be.monitorSymbol("zilusdt", handleKeyValueData); !valid.isValid())
        //{
            //logg("monitorSymbol failed");
        //}

        //if (auto valid = be.monitorSymbolBookStream("zilusdt", handleKeyValueData); !valid.isValid())
        //{
            //logg("monitorSymbolBookStream failed");
        //}

        //if (auto valid = be.monitorKlineCandlestickStream("zilusdt", "5m", handleKeyMultipleValueData); !valid.isValid())
        //{
            //logg("monitorSymbolBookStream failed");
        //}
        
        consoleFuture.wait();
    }
    catch (const std::exception ex)
    {
        logg(ex.what());
    }

    
    return 0;
}
