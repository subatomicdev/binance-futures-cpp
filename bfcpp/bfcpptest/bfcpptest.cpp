


#include <iostream>
#include <future>

#include <Futures.hpp>
#include <Logger.hpp>

#include "OpenAndCloseLimitOrder.h"


using namespace std::chrono_literals;
using namespace bfcpp;


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
void usdFutureTestNetDataStream(const Market::ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures TESTNET User Data ---\n";
	std::cout << "You must create/cancel etc an order for anything to show here\n";

	UsdFuturesTestMarket futuresTest { access} ;
	futuresTest.monitorUserData(handleUserDataUsdFutures);

	std::this_thread::sleep_for(10s);
}


/// <summary>
/// 
/// </summary>
/// <param name="access"></param>
void usdFutureDataStream(const Market::ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures User Data ---\n";
	std::cout << "You must create/cancel etc an order for anything to show here\n";

	UsdFuturesMarket futures { access };
	futures.monitorUserData(handleUserDataUsdFutures);

	std::this_thread::sleep_for(10s);
}



void monitorSymbol()
{
	std::cout << "\n\n--- USD-M Futures Monitor Symbol ---\n";

	UsdFuturesMarket futures;
	futures.monitorSymbol("BTCUSDT", handleKeyValueData);

	std::this_thread::sleep_for(10s);
}


int main(int argc, char** argv)
{
	try
	{
		// futures testnet
		const string apiKeyUsdFuturesTest = "";
		const string secretKeyUsdFuturesTest = "";
		// futures 'real'
		const string apiKeyUsdFutures = "";
		const string secretKeyUsdFutures = "";

		std::string apiFutTest = apiKeyUsdFuturesTest, secretFutTest = secretKeyUsdFuturesTest;
		std::string apiFut = apiKeyUsdFutures, secretFut = apiKeyUsdFutures;

		bool testNetMode = true;

		if (argc == 2)
		{
			// keyfile present: first line is "test" OR "live" to define if the keys are for testnet or live exchange
			if (auto fileSize = std::filesystem::file_size(std::filesystem::path {argv[1]}) ; fileSize > 140)
			{
				logg("Key file should be format with 3 lines:\nLine 1: <test | live>\nLine 2: api key\nLine 3: secret key");
			}
			else
			{
				char type[5], api[65], secret[65];
									
				std::ifstream fileStream(argv[1]);
				fileStream.getline(type, 5);
				fileStream.getline(api, 65);
				fileStream.getline(secret, 65) ;

				if (std::string_view{ type } == "live")
				{
					apiFut = api;
					secretFut = secret;
					testNetMode = false;
				}
				else
				{
					apiFutTest.assign(api, 64);
					secretFutTest.assign(secret, 64);
				}
			}
		}

		// these don't require keys
		//markPrice();
		//monitorSymbol();
		//multipleStreams();

		if (testNetMode)
		{
			//usdFutureTestNetDataStream(Market::ApiAccess {apiFutTest, secretFutTest});

			//OpenAndCloseLimitOrder test{ Market::ApiAccess {apiFutTest, secretFutTest} };
			//test.run();
		}
		else
		{
			//usdFutureDataStream(Market::ApiAccess {apiFut, secretFut});
		}
	}
	catch (const std::exception ex)
	{
		logg(ex.what());
	}

	return 0;
}
