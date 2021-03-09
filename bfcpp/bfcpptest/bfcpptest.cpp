


#include <iostream>
#include <future>

#include <Futures.hpp>
#include "Logger.hpp"

#include "OpenAndCloseLimitOrder.h"
#include "ScopedTimer.hpp"


using namespace std::chrono_literals;
using namespace bfcpp;



auto handleUserDataUsdFutures = [](std::any userData)
{
	UsdFutureUserData data = std::any_cast<UsdFutureUserData> (userData);

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

	auto markPriceHanlder = [](std::any data)
	{
		auto priceData = std::any_cast<MarkPriceStream> (data);

		std::stringstream ss;

		for (const auto& pair : priceData.prices)
		{
			std::for_each(std::begin(pair), std::end(pair), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
		}

		logg(ss.str());
	};


	UsdFuturesMarket usdFutures;

	try
	{
		usdFutures.monitorMarkPrice(markPriceHanlder);
	}
	catch (bfcpp::BfcppDisconnectException dex)
	{
		logg(dex.source() + " has been disconnected");
	}
	

	std::this_thread::sleep_for(10s);
}


/// <summary>
/// Receive candlesticks from websocket stream
/// </summary>
void monitorCandleSticks()
{
	std::cout << "\n\n--- USD-M Futures Candles ---\n";

	auto onStickHandler = [](std::any data)
	{
		auto candleData = std::any_cast<CandleStream> (data);

		std::stringstream ss;
		ss << "\neventType=" << candleData.eventTime;
		ss << "\nsymbol=" << candleData.symbol;

		std::for_each(std::begin(candleData.candle), std::end(candleData.candle), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
		logg(ss.str());
	};


	UsdFuturesMarket usdFutures;

	try
	{
		usdFutures.monitorKlineCandlestickStream("btcusdt", "15m", onStickHandler);
	}
	catch (bfcpp::BfcppDisconnectException dex)
	{
		logg(dex.source() + " has been disconnected");
	}


	std::this_thread::sleep_for(30s);
}



/// <summary>
/// Shows it's easy to receive multiple streams.
/// </summary>
void monitorMultipleStreams()
{
	std::cout << "\n\n--- USD-M Futures Multiple Streams on Futures ---\n";

	auto markPriceHandler = [](std::any data)
	{
		auto priceData = std::any_cast<MarkPriceStream> (data);

		std::stringstream ss;

		for (const auto& pair : priceData.prices)
		{
			std::for_each(std::begin(pair), std::end(pair), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
		}

		logg(ss.str());
	};


	auto onSymbolMiniTickHandler = [](std::any data)
	{
		auto ticker = std::any_cast<AllMarketMiniTickerStream> (data);

		stringstream ss;

		for (auto& tick : ticker.data)
		{
			std::for_each(std::begin(tick), std::end(tick), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
		}

		logg(ss.str());
	};


	UsdFuturesMarket usdFutures;
	usdFutures.monitorMarkPrice(markPriceHandler);
	usdFutures.monitorMiniTicker(onSymbolMiniTickHandler);

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
	std::cout << "\n\n--- USD-M Futures Monitor Symbol Mini Ticker ---\n";

	auto onSymbolMiniTickHandler = [](std::any data)
	{
		auto tick = std::any_cast<SymbolMiniTickerStream> (data);

		stringstream ss;
		std::for_each(std::begin(tick.data), std::end(tick.data), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });

		logg(ss.str());
	};


	UsdFuturesMarket futures;
	futures.monitorSymbol("BTCUSDT", onSymbolMiniTickHandler);

	std::this_thread::sleep_for(10s);
}


/// <summary>
/// Received any update to the best bid or ask's price or quantity in real-time for a specified symbol.
/// </summary>
void monitorSymbolBook()
{
	std::cout << "\n\n--- USD-M Futures Monitor Symbol Book Ticker ---\n";

	auto onBookSymbolMiniTickHandler = [](std::any data)
	{
		auto tick = std::any_cast<SymbolBookTickerStream> (data);

		stringstream ss;
		std::for_each(std::begin(tick.data), std::end(tick.data), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });

		logg(ss.str());
	};


	UsdFuturesMarket futures;
	futures.monitorSymbolBookStream("BTCUSDT", onBookSymbolMiniTickHandler);

	std::this_thread::sleep_for(10s);
}



void monitorAllMarketMiniTicker()
{
	std::cout << "\n\n--- USD-M Futures Monitor All Market Symbol Ticker ---\n";

	auto onSymbolMiniTickHandler = [](std::any data)
	{
		auto ticker = std::any_cast<AllMarketMiniTickerStream> (data);

		stringstream ss;

		for (auto& tick : ticker.data)
		{
			std::for_each(std::begin(tick), std::end(tick), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
		}

		logg(ss.str());
	};


	UsdFuturesMarket futures;
	futures.monitorMiniTicker(onSymbolMiniTickHandler);

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



void klines()
{
	std::cout << "\n\n--- USD-M Futures Klines ---\n";

	UsdFuturesMarket futuresTest{};
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



using namespace std::chrono;

static size_t NumNewOrders = 5;


void performanceCheckSync(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures New Order Sync Performance ---\n";

	map<string, string> order =
	{
		{"symbol", "BTCUSDT"},
		{"side", "BUY"},
		{"type", "MARKET"},
		{"quantity", "0.001"}
	};

	UsdFuturesTestMarketPerfomance market{ access };

	vector<NewOrderPerformanceResult> results;
	results.reserve(NumNewOrders);

	for (size_t i = 0; i < NumNewOrders; ++i)
	{
		auto start = Clock::now();
		auto result = market.newOrderPerfomanceCheck(std::move(order));
		result.total = Clock::now() - start;
		results.emplace_back(std::move(result));
	}
	

	high_resolution_clock::duration	total{};
	high_resolution_clock::duration	avgQueryBuild{}, avgApiCall{}, avgResponseHandler{},
																	maxApiCall{ high_resolution_clock::duration::min() },
																	minApiCall{ high_resolution_clock::duration::max() };

	for (const auto& result : results)
	{
		if (result.valid())
		{
			total += result.total;

			avgQueryBuild += result.restQueryBuild;
			avgApiCall += result.restApiCall;
			avgResponseHandler += result.restResponseHandler;
			minApiCall = std::min<high_resolution_clock::duration>(minApiCall, result.restApiCall);
			maxApiCall = std::max<high_resolution_clock::duration>(maxApiCall, result.restApiCall);
		}
		else
		{
			logg("Error: " + result.msg());
		}
		
		total += result.total;
	}


	avgQueryBuild /= NumNewOrders;
	avgApiCall /= NumNewOrders;
	avgResponseHandler /= NumNewOrders;

	stringstream ss;
	ss << "\nTotal: " << NumNewOrders << " orders in " << duration_cast<std::chrono::milliseconds>(total).count() << " milliseconds\n";
	ss << "\n|\t\t\t| time (nanoseconds) |" <<
		"\n------------------------------------------" <<
		"\nAvg. Rest Query Build:\t\t" << duration_cast<nanoseconds>(avgQueryBuild).count() <<
		"\nAvg. Rest Call Latency:\t\t" << duration_cast<nanoseconds>(avgApiCall).count() << " (Min:" << duration_cast<nanoseconds>(minApiCall).count() << ", Max: " << duration_cast<nanoseconds>(maxApiCall).count() << ")" <<
		"\nAvg. Rest Response Handler:\t" << duration_cast<nanoseconds>(avgResponseHandler).count() <<
		"\n------------------------------------------";

	logg(ss.str());
}


void performanceCheckAsync(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures New Order Async Performance ---\n";

	map<string, string> order =
	{
		{"symbol", "BTCUSDT"},
		{"side", "BUY"},
		{"type", "MARKET"},
		{"quantity", "0.001"}
	};

	UsdFuturesTestMarketPerfomance market{ access };

	vector<pplx::task<NewOrderPerformanceResult>> results;
	results.reserve(NumNewOrders);

	auto start = Clock::now();
	for (size_t i = 0; i < NumNewOrders; ++i)
	{
		auto result = market.newOrderPerfomanceCheckAsync(std::move(order));
		results.emplace_back(std::move(result));
	}

	
	// note: you could use pplx::when_any() to handle each task as it completes, then call again when_any() until all
	//			 are finished.
	
	// wait for the new order tasks to return, the majority of which is due to the REST call latency
	pplx::when_all(std::begin(results), std::end(results)).wait();
	auto end = Clock::now();


	high_resolution_clock::duration avgQueryBuild{}, avgApiCall{}, avgResponseHandler{},
																	maxApiCall{ high_resolution_clock::duration::min() },
																	minApiCall{ high_resolution_clock::duration::max()};

	for (const auto& taskResult : results)
	{
		const auto& result = taskResult.get();

		if (result.valid())
		{
			avgQueryBuild += result.restQueryBuild;
			avgApiCall += result.restApiCall;
			avgResponseHandler += result.restResponseHandler;
			minApiCall = std::min<high_resolution_clock::duration>(minApiCall, result.restApiCall);
			maxApiCall = std::max<high_resolution_clock::duration>(maxApiCall, result.restApiCall);
		}
		else
		{			
			logg("Error: " + result.msg());
		}
	}
	
	avgQueryBuild /= NumNewOrders;
	avgApiCall /= NumNewOrders;
	avgResponseHandler /= NumNewOrders;

	stringstream ss;
	ss << "\nTotal: " << NumNewOrders << " orders in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " milliseconds\n";
	ss << "\n|\t\t\t| time (nanoseconds) |" <<
		"\n------------------------------------------" <<
		"\nAvg. Rest Query Build:\t\t" << duration_cast<nanoseconds>(avgQueryBuild).count() <<
		"\nAvg. Rest Call Latency:\t\t" << duration_cast<nanoseconds>(avgApiCall).count() << " (Min:" << duration_cast<nanoseconds>(minApiCall).count() << ", Max: " << duration_cast<nanoseconds>(maxApiCall).count() << ")" <<
		"\nAvg. Rest Response Handler:\t" << duration_cast<nanoseconds>(avgResponseHandler).count() <<
		"\n------------------------------------------";
	
	logg(ss.str());
}


void newOrderAsync(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures New Order Async ---\n";
	
	map<string, string> order =
	{
		{"symbol", "BTCUSDT"},
		{"side", "BUY"},
		{"type", "MARKET"},
		{"quantity", "0.001"}
	};

	UsdFuturesTestMarket market{ access };

	vector<pplx::task<NewOrderResult>> results;
	results.reserve(NumNewOrders);


	logg("Sending orders");

	for (size_t i = 0; i < NumNewOrders; ++i)
	{
		results.emplace_back(std::move(market.newOrderAsync(std::move(order))));
	}

	logg("Waiting for all to complete");

	// note: you could use pplx::when_any() to handle each task as it completes, 
	//			 then call when_any() until all are finished.

	// wait for the new order tasks to return, the majority of which is due to the REST call latency
	pplx::when_all(std::begin(results), std::end(results)).wait();

	logg("Done: ");

	stringstream ss;
	ss << "\nOrder Ids: ";
	for (auto& task : results)
	{
		NewOrderResult result = task.get();

		if (result.valid())
		{
			// do stuff with result
			ss << "\n" << result.response["orderId"];
		}
	}

	logg(ss.str());
}



void newOrderBatch(const ApiAccess& access)
{
	std::cout << "\n\n--- USD-M Futures New Order Batch ---\n";

	map<string, string> order =
	{
		{"symbol", "BTCUSDT"},
		{"side", "BUY"},
		{"type", "MARKET"},
		{"quantity", "0.001"}
	};

	vector<map<string, string>> orders;
	orders.push_back(order);
	orders.push_back(order);
	orders.push_back(order);


	UsdFuturesTestMarket market{ access };

	try
	{
		auto result = market.newOrderBatch(std::move(orders));

		stringstream ss;
		ss << "\nResponse:";
		for (const auto& order : result.response)
		{
			ss << "\n{";
			std::for_each(std::begin(order), std::end(order), [&ss](auto& info) { ss << "\n\t" << info.first + "=" + info.second ;  });
			ss << "\n}";
		}
		logg(ss.str());
	}
	catch (BfcppException bef)
	{
		logg("error: " + string{ bef.what() });
	}
}



void exchangeInfo ()
{
	std::cout << "\n\n--- USD-M Futures Exchange Info ---\n";


	UsdFuturesTestMarket market{};

	try
	{
		auto result = market.exchangeInfo();

		stringstream ss;
		ss << "\nResponse:";

		ss << "\nSymbols\n{";
		for (const auto& symbol: result.symbols)
		{
			ss << "\n\tdata\n\t{";
			std::for_each(std::begin(symbol.data), std::end(symbol.data), [&ss](auto& info) { ss << "\n\t\t" << info.first + "=" + info.second;  });
			ss << "\n\t}";

			ss << "\n\tfilters\n\t{";
			for (const auto& filter : symbol.filters)
			{
				std::for_each(std::begin(filter), std::end(filter), [&ss](auto& info) { ss << "\n\t\t" << info.first + "=" + info.second;  });
			}
			ss << "\n\t}";


			ss << "\n\torderType\n\t{";
			std::for_each(std::begin(symbol.orderTypes), std::end(symbol.orderTypes), [&ss](auto& info) { ss << "\n\t\t" << info;  });
			ss << "\n\t}";

			ss << "\n\ttimeInForce\n\t{";
			std::for_each(std::begin(symbol.timeInForce), std::end(symbol.timeInForce), [&ss](auto& info) { ss << "\n\t\t" << info;  });
			ss << "\n\t}";
		}
		ss << "\n}";


		ss << "\nRate Limits\n{";
		for (const auto& rate : result.rateLimits)
		{
			std::for_each(std::begin(rate), std::end(rate), [&ss](auto& info) { ss << "\n\t" << info.first + "=" + info.second;  });
		}
		ss << "\n}";

		
		ss << "\nExchange Filters\n{";
		for (const auto& filter : result.exchangeFilters)
		{
			std::for_each(std::begin(filter), std::end(filter), [&ss](auto& info) { ss << "\n\t" << info.first + "=" + info.second;  });
		}
		ss << "\n}";


		ss << "\nserverTime=" << result.serverTime << "\ntimezone=" << result.timezone;

		logg(ss.str());
	}
	catch (BfcppException bef)
	{
		logg("error: " + string{ bef.what() });
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
		//monitorCandleSticks();
		//monitorSymbol();
		//monitorSymbolBook();
		//monitorAllMarketMiniTicker();
		//monitorMultipleStreams();
		
		klines();
		//exchangeInfo();

		if (testNetMode)
		{
			ApiAccess access { apiFutTest, secretFutTest };
			//usdFutureTestNetDataStream(access);

			//OpenAndCloseLimitOrder openCloseLimit{ access };
			//openCloseLimit.run();

			//allOrders(access);

			//accountInformation(access);

			//accountBalance(access);

			//performanceCheckSync(access);

			//performanceCheckAsync(access);

			//newOrderAsync(access);

			//newOrderBatch(access);

			//usdFutureDataStream(ApiAccess{ apiFut, secretFut });
		}
		else
		{
			ApiAccess access{ apiFut, secretFut };

			//takerBuySellVolume(access);
		}
	}
	catch (const std::exception ex)
	{
		logg(ex.what());
	}

	return 0;
}
