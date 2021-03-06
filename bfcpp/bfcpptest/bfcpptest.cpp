


#include <iostream>
#include <future>

#include <Futures.hpp>
#include "Logger.hpp"

#include "OpenAndCloseLimitOrder.h"
#include "ScopedTimer.hpp"


using namespace std::chrono_literals;
using namespace bfcpp;


auto handleKeyMultipleValueData = [](BinanceKeyMultiValueData data)
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


auto handleKeyValueData = [](BinanceKeyValueData data)
{
	for (const auto& p : data.values)
	{
			logg(p.first + "=" + p.second);
	}
};


auto handleUserDataUsdFutures = [](UsdFutureUserData data)
{
	if (data.type == UsdFutureUserData::EventType::MarginCall)
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
	else if (data.type == UsdFutureUserData::EventType::OrderUpdate)
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
	else if (data.type == UsdFutureUserData::EventType::AccountUpdate)
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
void monitorMarkPrice()
{
	std::cout << "\n\n--- USD-M Futures Mark Price ---\n";

	UsdFuturesMarket usdFutures;

	try
	{
		usdFutures.monitorMarkPrice(handleKeyMultipleValueData);
	}
	catch (bfcpp::BfcppDisconnectException dex)
	{
		logg(dex.source() + " has been disconnected");
	}
	

	std::this_thread::sleep_for(10s);
}


/// <summary>
/// Shows it's easy to receive multiple streams.
/// </summary>
void monitorMultipleStreams()
{
	std::cout << "\n\n--- USD-M Futures Multiple Streams on Futures ---\n";

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
void usdFutureTestNetDataStream(const ApiAccess& access)
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
void usdFutureDataStream(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures User Data ---\n";
	std::cout << "You must create/cancel etc an order for anything to show here\n";

	UsdFuturesMarket futures { access };
	futures.monitorUserData(handleUserDataUsdFutures);

	std::this_thread::sleep_for(10s);
}



/// <summary>
/// Receives from the symbol mini ticker, updated every 1000ms.
/// </summary>
void monitorSymbol()
{
	std::cout << "\n\n--- USD-M Futures Monitor Symbol ---\n";

	UsdFuturesMarket futures;
	futures.monitorSymbol("BTCUSDT", handleKeyValueData);

	std::this_thread::sleep_for(10s);
}



/// <summary>
/// 1. Get all orders (which defaults to within 7 days)
/// 2. Get all orders for today
/// 
/// Read Market::allOrders() for what will be returned.
/// </summary>
/// <param name="access"></param>
void allOrders(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures TESTNET All Orders ---\n";

	auto showResults = [](const AllOrdersResult& result)
	{
		stringstream ss;

		if (result.valid())
		{
			ss << "\nFound " << result.response.size() << " orders";

			for (const auto& order : result.response)
			{
				ss << "\n{";
				std::for_each(std::begin(order), std::end(order), [&ss](auto& values) { ss << "\n\t" << values.first << "=" << values.second;  });
				ss << "\n}";
			}
		}
		else
		{
			ss << "Invalid: " << result.msg();
		}

		logg(ss.str());
	};

	UsdFuturesTestMarket futuresTest { access };


	// --- Get all orders ---
	auto result = futuresTest.allOrders({ {"symbol", "BTCUSDT"} });
	logg("All orders");
	showResults(result);


	// --- Get all orders for today ---

	// get timestamp for start of today
	Clock::time_point startOfDay;
	{
		time_t tm = Clock::to_time_t(Clock::now());
		auto lt = std::localtime(&tm);
		lt->tm_hour = 0;
		lt->tm_min = 0;
		lt->tm_sec = 0;

		startOfDay = Clock::from_time_t(std::mktime(lt));
	}
	
	logg("All orders for today");
	result = futuresTest.allOrders({ {"symbol", "BTCUSDT"},  {"startTime", std::to_string(bfcpp::getTimestamp(startOfDay))} });

	showResults(result);
}



void accountInformation(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures TESTNET Account Information ---\n";

	auto showResults = [](const AccountInformation& result)
	{
		stringstream ss;

		std::for_each(std::begin(result.data), std::end(result.data), [&ss](auto& values) { ss << "\n" << values.first << "=" << values.second;  });

		ss << "\nFound " << result.assets.size() << " assets";
		for (const auto& asset : result.assets)
		{
			ss << "\n{";
			std::for_each(std::begin(asset), std::end(asset), [&ss](auto& values) { ss << "\n\t" << values.first << "=" << values.second;  });
			ss << "\n}";
		}

		ss << "\nFound " << result.positions.size() << " positions";
		for (const auto& position : result.positions)
		{
			ss << "\n{";
			std::for_each(std::begin(position), std::end(position), [&ss](auto& values) { ss << "\n\t" << values.first << "=" << values.second;  });
			ss << "\n}";
		}

		logg(ss.str());
	};


	UsdFuturesTestMarket futuresTest{ access };
	
	auto result = futuresTest.accountInformation();
	
	showResults(result);
}



void accountBalance(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures TESTNET Account Balance ---\n";

	UsdFuturesTestMarket futuresTest{ access };

	auto result = futuresTest.accountBalance();

	stringstream ss;
	ss << "\nFound " << result.balances.size() << " balances";
	for (const auto& asset : result.balances)
	{
		ss << "\n{";
		std::for_each(std::begin(asset), std::end(asset), [&ss](auto& values) { ss << "\n\t" << values.first << "=" << values.second;  });
		ss << "\n}";
	}

	logg(ss.str());
}



void takerBuySellVolume(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures Taker Buy Sell Volume ---\n";

	UsdFuturesMarket futuresTest{ access };

	auto result = futuresTest.takerBuySellVolume({ {"symbol","BTCUSDT"}, {"period","15m"} });

	stringstream ss;
	ss << "\nFound " << result.response.size() << " volumes";
	for (const auto& entry : result.response)
	{
		ss << "\n{";
		std::for_each(std::begin(entry), std::end(entry), [&ss](auto& values) { ss << "\n\t" << values.first << "=" << values.second;  });
		ss << "\n}";
	}

	logg(ss.str());
}



void klines(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures Klines ---\n";

	UsdFuturesMarket futuresTest{ access };
	futuresTest.setReceiveWindow(RestCall::KlineCandles, 3000ms);

	auto result = futuresTest.klines({ {"symbol","BTCUSDT"}, {"limit","5"}, {"interval", "15m"} });

	

	stringstream ss;
	ss << "\nFound " << result.response.size() << " kline sticks";
	for (const auto& entry : result.response)
	{
		ss << "\n{";
		std::for_each(std::begin(entry), std::end(entry), [&ss](auto& value) { ss << "\n\t" << value;  });
		ss << "\n}";
	}

	logg(ss.str());
}


void performanceCheck(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures API Performance ---\n";

	map<string, string> order =
	{
		{"symbol", "BTCUSDT"},
		{"side", "BUY"},
		{"type", "MARKET"},
		{"quantity", "0.001"}
	};

	UsdFuturesTestMarketPerfomance market{ access };

	auto start = Clock::now();
	auto result = market.newOrderPerfomanceCheck(std::move(order));
	result.total = Clock::now() - start;

	if (result.valid())
	{
		stringstream ss;
		ss << "\nOrder data:\n";
		for (const auto& val : result.response)
		{
			ss << val.first + "=" + val.second << "\n";
		}

		logg(ss.str());

		ss.clear(); ss.str("");
		ss <<	"\nRest Call Latency:\t" << std::chrono::duration_cast<std::chrono::milliseconds>(result.restApiCall).count() << " milliseconds" <<
					"\n------------------------------------------" <<
					"\nRest Query Build:\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(result.restQueryBuild).count() << " nanoseconds" <<
					"\nRest Response Handler:\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(result.restResponseHandler).count() << " nanoseconds" <<
					"\nTotal Request Handling:\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(result.bfcppTotalProcess).count() << " nanoseconds" <<
					"\n------------------------------------------" <<
					"\nTotal:\t\t\t" << std::chrono::duration_cast<std::chrono::milliseconds>(result.total).count() << " milliseconds";

		logg(ss.str());
	}
	else
	{
		logg("Error: " + result.msg());

		stringstream ss;
		ss << "\nRest Call Latency:\t" << std::chrono::duration_cast<std::chrono::milliseconds>(result.restApiCall).count() << " milliseconds" <<
					"\n------------------------------------------" <<
					"\nRest Query Build:\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(result.restQueryBuild).count() << " nanoseconds" <<
					"\nTotal Request Handling:\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(result.bfcppTotalProcess).count() << " nanoseconds" <<
					"\n------------------------------------------" <<
					"\nTotal:\t\t\t" << std::chrono::duration_cast<std::chrono::milliseconds>(result.total).count() << " milliseconds";

		logg(ss.str());
	}
	
}



int main(int argc, char** argv)
{
	try
	{
		std::string apiFutTest, secretFutTest;
		std::string apiFut, secretFut;

		bool testNetMode = true;

		if (argc == 2)
		{
			// keyfile used - format is 3 lines:
			// <test | live>
			// <api key>
			// <secret key>
			if (auto fileSize = std::filesystem::file_size(std::filesystem::path {argv[1]}) ; fileSize > 140)
			{
				logg("Key file should be format with 3 lines:\nLine 1: <test | live>\nLine 2: api key\nLine 3: secret key");
			}
			else
			{
				string line;
									
				std::ifstream fileStream(argv[1]);
				std::getline(fileStream, line);

				if (line == "live")
				{
					std::getline(fileStream, apiFut);
					std::getline(fileStream, secretFut);
					testNetMode = false;
				}
				else
				{
					std::getline(fileStream, apiFutTest);
					std::getline(fileStream, secretFutTest);
				}
			}
		}



		// these don't require keys
		//monitorMarkPrice();
		//monitorSymbol();
		//monitorMultipleStreams();


		if (testNetMode)
		{
			ApiAccess access { apiFutTest, secretFutTest };
			//usdFutureTestNetDataStream(access);

			//OpenAndCloseLimitOrder openCloseLimit{ access };
			//openCloseLimit.run();

			//allOrders(access);

			//accountInformation(access);

			//accountBalance(access);

			klines({});

			//performanceCheck(access);
		}
		else
		{
			ApiAccess access{ apiFut, secretFut };

			//takerBuySellVolume(access);

			//usdFutureDataStream(ApiAccess {apiFut, secretFut});
		}
	}
	catch (const std::exception ex)
	{
		logg(ex.what());
	}

	return 0;
}
