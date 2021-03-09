#ifndef  BFCPP_TEST_OPENANDCLOSELIMITORDER_H
#define BFCPP_TEST_OPENANDCLOSELIMITORDER_H

#include "TestCommon.hpp"
#include <atomic>


/// <summary>
/// A basic example of opening a LIMIT BUY order, waiting 8 seconds then either cancelling/closing.
/// If the order is NEW it is cancelled.
/// If the order is FILLED then a SELL is created.
/// </summary>
/// <param name="access"></param>
class OpenAndCloseLimitOrder : public BfcppTest
{

public:
	OpenAndCloseLimitOrder(const ApiAccess access) : BfcppTest(access), m_market{access}
	{
		m_status = OrderStatus::None;
		m_symbol = "BTCUSDT";
	}


	void handleMarkPrice(std::any data)
	{
		auto priceData = std::any_cast<MarkPriceStream> (data);

		for (auto& price : priceData.prices)
		{
			if (price["s"] == m_symbol)
			{
				m_markPriceString = price["p"];
				m_priceSet.notify_all();
				break;
			}
		}
	}


	void handleUserDataUsdFutures(std::any userData)
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

			if (auto orderEntry = data.ou.orders.find(m_symbol); orderEntry != data.ou.orders.cend())
			{
				ss << "Order Status = " << orderEntry->second["X"];

				m_status = OrderStatusMap.at(orderEntry->second["X"]);
			}

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


	virtual void run() override
	{
		auto funcMarkPrice = std::bind(&OpenAndCloseLimitOrder::handleMarkPrice, std::ref(*this), std::placeholders::_1);
		m_market.monitorMarkPrice(funcMarkPrice);	// to get an accurate price

		auto funcUserData = std::bind(&OpenAndCloseLimitOrder::handleUserDataUsdFutures, std::ref(*this), std::placeholders::_1);
		m_market.monitorUserData(funcUserData);	// to get order updates

		logg("Create order");
		m_orderId = createOrder(m_symbol);

		logg("Waiting");
		std::this_thread::sleep_for(5s);

		closeOrder(m_orderId, m_status);
	}


private:
	string createOrder(const string& symbol)
	{
		map<string, string> order =
		{
			{"symbol", symbol},
			{"side", "BUY"},
			{"timeInForce", "GTC"},
			{"type", "LIMIT"},
			{"quantity", "0.001"},
			{"price",""} // updated below with mark price from user data stream
		};

		std::mutex mux;
		std::unique_lock lock(mux);
		m_priceSet.wait(lock);	// notified by handleMarkPrice()

		// set price then send order
		order["price"] = priceTransform(std::to_string(std::stod(m_markPriceString)));

		auto result = m_market.newOrder(std::move(order));


		stringstream ss;
		ss << "\nnewOrder() returned:\n";
		for (const auto& val : result.response)
		{
			ss << val.first + "=" + val.second << "\n";
		}
		logg(ss.str());

		return result.response["orderId"];
	}


	void closeOrder(const string& orderId, const OrderStatus status)
	{
		if (status == OrderStatus::New)
		{
			logg("Close order");

			map<string, string> cancel =
			{
				{"symbol", m_symbol},
				{"orderId", m_orderId}
			};

			auto cancelResult = m_market.cancelOrder(std::move(cancel));

			stringstream ss;
			ss << "\ncancelOrder() returned:\n";
			for (const auto& val : cancelResult.response)
			{
				ss << val.first + "=" + val.second << "\n";
			}
			logg(ss.str());
		}
		else if (status == OrderStatus::Filled)
		{
			logg("Close Filled position");

			map<string, string> cancel =
			{
				{"symbol", m_symbol},
				{"side", "SELL"},
				{"type", "MARKET"},
				{"quantity", "0.001"},
				{"orderId", m_orderId}
			};

			auto cancelResult = m_market.newOrder(std::move(cancel));

			stringstream ss;
			ss << "\nnewOrder() returned:\n";
			for (const auto& val : cancelResult.response)
			{
				ss << val.first + "=" + val.second << "\n";
			}
			logg(ss.str());
		}
		else if (m_status == OrderStatus::PartiallyFilled)
		{
			logg("TODO Close PartiallyFilled position");
		}
	}



private:
	string m_symbol;
	string m_markPriceString;
	string m_orderId;

	std::condition_variable m_priceSet;
	UsdFuturesTestMarket m_market;
	std::atomic<OrderStatus> m_status;
};


#endif 

