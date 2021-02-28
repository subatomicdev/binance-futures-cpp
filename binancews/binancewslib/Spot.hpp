#ifndef __BINANCE_BINANCEEXCHANGE_HPP 
#define __BINANCE_BINANCEEXCHANGE_HPP



#include "Market.hpp"



namespace binancews
{
    /// <summary>
    /// Access the Spot Market.
    /// </summary>
    class SpotMarket : public Market
    {
    protected:
        SpotMarket(MarketType mt, const string& exchangeUri, const string& apiKey = {}, const string& secretKey = {}) : Market(mt, exchangeUri, apiKey, secretKey)
        {
        }

    public:
        SpotMarket(const string& apiKey = {}, const string& secretKey = {}) : SpotMarket(MarketType::Spot, SpotWebSockUri, apiKey, secretKey)
        {

        }


        virtual ~SpotMarket()
        {
        }


    public:

        /// <summary>
        /// Receives from the Trade Streams for a given symbol 
        /// The updates in real time.
        /// </summary>
        /// <param name="symbol">The symbol to receive trades information</param>
        /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
        /// <returns>A MonitorToken. If MonitorToken::isValid() is a problem occured.</returns>
        MonitorToken monitorTradeStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
        {
            static const JsonKeys keys
            {
                {"e", {}},
                {"E", {}},
                {"s", {}},
                {"t", {}},
                {"p", {}},
                {"q", {}},
                {"b", {}},
                {"a", {}},
                {"T", {}},
                {"m", {}},
                {"M", {}}
            };

            auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + symbol + "@trade", keys);

            if (std::get<0>(tokenAndSession).isValid())
            {
                std::get<1>(tokenAndSession)->onDataUserCallback = onData;
            }

            return std::get<0>(tokenAndSession);
        }


        /// <summary>
        /// Monitor the user data on the Spot market. The API and secret keys must have permissioned enabled for this (usually default).
        /// </summary>
        /// <param name="apiKey"></param>
        /// <param name="onData"></param>
        /// <param name="mode"></param>
        /// <returns></returns>
        MonitorToken monitorUserData(std::function<void(SpotUserData)> onData)
        {            
            MonitorToken monitorToken;

            if (createListenKey(m_marketType))
            {
                if (auto session = connect(m_exchangeBaseUri + "/ws/" + m_listenKey); session)
                {
                    try
                    {
                        auto token = session->getCancelToken();

                        session->receiveTask = pplx::create_task([session, token, &onData, this]
                        {
                            handleUserData(session, onData);
                        }, token);

                        monitorToken.id = m_monitorId++;
                        session->id = monitorToken.id;
                        session->onSpotUserDataCallback = onData;

                        m_sessions.push_back(session);
                        m_idToSession[monitorToken.id] = session;
                    }
                    catch (pplx::task_canceled tc)
                    {
                        // the receive task was cancelled, not triggered by us, most likely the server we need to disconnect this client.
                        logg("task cancelled exception " + string{ tc.what() } + " on " + utility::conversions::to_utf8string(session->client.uri().to_string()));
                        logg("this stream will be disconnected");

                        disconnect(session->id, true);
                    }
                    catch (std::exception ex)
                    {
                        logg(ex.what());
                    }
                }
            }

            return monitorToken;
        }


    private:

        void handleUserData(shared_ptr<WebSocketSession> session, std::function<void(SpotUserData)> onData)
        {
            while (!session->getCancelToken().is_canceled())
            {
                try
                {
                    session->client.receive().then([=, token = session->getCancelToken()](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
                    {
                        if (!token.is_canceled())
                        {
                            try
                            {
                                // get the payload synchronously
                                std::string strMsg;
                                websocketInMessage.get().extract_string().then([=, &strMsg, cancelToken = session->getCancelToken()](pplx::task<std::string> str_tsk)
                                {
                                    try
                                    {
                                        if (!cancelToken.is_canceled())
                                            strMsg = str_tsk.get();
                                    }
                                    catch (...)
                                    {

                                    }
                                }, session->getCancelToken()).wait();


                                if (!strMsg.empty())
                                {
                                    std::error_code errCode;
                                    if (auto json = web::json::value::parse(strMsg, errCode); errCode.value() == 0)
                                    {
                                        extractSpotMarketData(session, std::move(json));
                                    }
                                    else
                                    {
                                        logg("Invalid json: " + strMsg);
                                    }
                                }                                
                            }
                            catch (pplx::task_canceled tc)
                            {
                                throw;
                            }
                            catch (std::exception ex)
                            {
                                throw;
                            }
                        }
                        else
                        {
                            pplx::cancel_current_task();
                        }

                    }, session->getCancelToken()).wait();
                }
                catch (...)
                {
                    throw;
                }
            }

            pplx::cancel_current_task();
        }


        void extractSpotMarketData(shared_ptr<WebSocketSession> session, web::json::value&& jsonVal)
        {
            const utility::string_t CodeField = utility::conversions::to_string_t("code");
            const utility::string_t MsgField = utility::conversions::to_string_t("msg");

            if (jsonVal.has_string_field(CodeField) && jsonVal.has_string_field(MsgField))
            {
                std::cout << "\nError: " << utility::conversions::to_utf8string(jsonVal.at(CodeField).as_string()) << " : " << utility::conversions::to_utf8string(jsonVal.at(MsgField).as_string());
            }
            else
            {
                const utility::string_t EventTypeField = utility::conversions::to_string_t("e");
                const utility::string_t BalancesField = utility::conversions::to_string_t("B");

                const utility::string_t EventOutboundAccountPosition = utility::conversions::to_string_t("outboundAccountPosition");
                const utility::string_t EventBalanceUpdate = utility::conversions::to_string_t("balanceUpdate");
                const utility::string_t EventExecutionReport = utility::conversions::to_string_t("executionReport");


                SpotUserData::EventType type = SpotUserData::EventType::Unknown;

                if (jsonVal.at(EventTypeField).as_string() == EventOutboundAccountPosition)
                {
                    type = SpotUserData::EventType::AccountUpdate;
                }
                else if (jsonVal.at(EventTypeField).as_string() == EventBalanceUpdate)
                {
                    type = SpotUserData::EventType::BalanceUpdate;
                }
                else if (jsonVal.at(EventTypeField).as_string() == EventExecutionReport)
                {
                    type = SpotUserData::EventType::OrderUpdate;
                }


                SpotUserData userData(type);

                if (type != SpotUserData::EventType::Unknown)
                {
                    switch (type)
                    {

                    case SpotUserData::EventType::AccountUpdate:
                    {
                        getJsonValues(jsonVal, userData.data, { "e", "E", "u" });

                        for (auto& balance : jsonVal[BalancesField].as_array())
                        {
                            map<string, string> values;
                            getJsonValues(balance, values, { "a", "f", "l" });

                            userData.au.balances[values["a"]] = std::move(values);
                        }
                    }
                    break;


                    case SpotUserData::EventType::BalanceUpdate:
                        getJsonValues(jsonVal, userData.data, { "e", "E", "a", "d", "T" });
                        break;


                    case SpotUserData::EventType::OrderUpdate:
                        getJsonValues(jsonVal, userData.data, { "e", "E", "s", "c", "S", "o", "f", "q", "p", "P", "F", "g", "C", "x", "X", "r", "i", "l", "z",
                                                                "L", "n", "N", "T", "t", "I", "w", "m", "M", "O", "Z", "Y", "Q" });
                        break;


                    default:
                        // handled above
                        break;
                    }


                    session->onSpotUserDataCallback(std::move(userData));
                }
            }
        }
    };




    class SpotTestMarket : public SpotMarket
    {
    public:
        SpotTestMarket(const string& apiKey = {}, const string& secretKey = {}) : SpotMarket(MarketType::SpotTest, TestSpotWebSockUri, apiKey, secretKey)
        {

        }

        virtual ~SpotTestMarket()
        {
        }
    };

}

#endif