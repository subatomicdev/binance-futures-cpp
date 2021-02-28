#ifndef __BINANCE_FUTURES_HPP 
#define __BINANCE_FUTURES_HPP


#include "Market.hpp"


namespace binancews
{
    /// <summary>
    /// Access the USD-M Future's market. You must have a Futures account.
    /// The APis keys must be enabled for Futures in the API Management settings. 
    /// If you created the API key before you created your Futures account, you must create a new API key.
    /// </summary>
    class UsdFuturesMarket : public Market
    {
    protected:
        UsdFuturesMarket(MarketType mt, const string& exchangeUri, const string& apiKey = {}, const string& secretKey = {}) : Market(mt, exchangeUri, apiKey, secretKey)
        {
        }

    public:
        UsdFuturesMarket(const string& apiKey = {}, const string& secretKey = {}) : UsdFuturesMarket(MarketType::Futures, FuturestWebSockUri, apiKey, secretKey)
        {

        }


        virtual ~UsdFuturesMarket()
        {
        }

        /// <summary>
        /// Futures Only. Receives data from here: https://binance-docs.github.io/apidocs/futures/en/#mark-price-stream-for-all-market
        /// </summary>
        /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
        /// <returns></returns>
        MonitorToken monitorMarkPrice(std::function<void(BinanceKeyMultiValueData)> onData)
        {
            static const JsonKeys keys
            {
                {"s", {"e", "E","s","p","i","P","r","T"}}
            };

            auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/!markPrice@arr@1s", keys, "s");

            if (std::get<0>(tokenAndSession).isValid())
            {
                std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
            }

            return std::get<0>(tokenAndSession);
        }


        /// <summary>
        /// Monitor data on the spot market.
        /// </summary>
        /// <param name="apiKey"></param>
        /// <param name="onData"></param>
        /// <param name="mode"></param>
        /// <returns></returns>
        MonitorToken monitorUserData(std::function<void(UsdFutureUserData)> onData)
        {
            MonitorToken monitorToken;

            if (createListenKey(m_marketType))
            {
                if (auto session = connect(m_exchangeBaseUri + "/ws/" + m_listenKey); session)
                {
                    try
                    {
                        monitorToken.id = m_monitorId++;

                        session->id = monitorToken.id;
                        session->onUsdFuturesUserDataCallback = onData;

                        m_idToSession[monitorToken.id] = session;
                        m_sessions.push_back(session);

                        auto token = session->getCancelToken();
                        session->receiveTask = pplx::create_task([session, token, &onData, this]
                            {
                                try
                                {
                                    handleUserData(session, onData);
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

                            }, token);


                        auto timerFunc = std::bind(&UsdFuturesMarket::onUserDataTimer, this);

                        if (m_marketType == MarketType::FuturesTest)
                        {
                            //TODO ISSUE this doesn't seem to please the testnet, creating orders on the site keeps the connection alive

                            // the test net seems to kick us out after 60s of no activity
                            //m_userDataStreamTimer.start(timerFunc, 45s); 
                        }
                        else
                        {
                            m_userDataStreamTimer.start(timerFunc, 60s * 45); // 45 mins
                        }
                    }
                    catch (std::exception ex)
                    {
                        logg(string{ "ERROR: " } + ex.what());
                    }
                }
            }

            return monitorToken;
        }



        void onUserDataTimer()
        {
            logg("Sending keepalive");

            string uri;
            string path;

            switch (m_marketType)
            {
            case MarketType::Futures:
                uri = UsdFuturesRestUri;
                path = UsdFuturesRequestPath;
                break;

            case MarketType::FuturesTest:
                uri = TestUsdFuturestRestUri;
                path = UsdFuturesRequestPath;
                break;
            }

            web::uri requestUri(utility::conversions::to_string_t(path));

            web::http::http_request request{ web::http::methods::PUT };
            request.headers().add(utility::conversions::to_string_t(ContentTypeName), utility::conversions::to_string_t("application/json"));
            request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiKey));
            request.headers().add(utility::conversions::to_string_t(ClientSDKVersionName), utility::conversions::to_string_t("binancews_cpp_alpha"));
            request.set_request_uri(requestUri);

            web::http::client::http_client client{ web::uri{utility::conversions::to_string_t(uri)} };

            client.request(request).then([this](web::http::http_response response)
            {
                if (response.status_code() != web::http::status_codes::OK)
                {
                    logg("ERROR : keepalive for listen key failed");
                }
            }).wait();
        }


    private:

        void handleUserData(shared_ptr<WebSocketSession> session, std::function<void(UsdFutureUserData)> onData)
        {
            while (!session->getCancelToken().is_canceled())
            {
                try
                {
                    auto  rcv = session->client.receive().then([=, token = session->getCancelToken()](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
                    {
                        if (!token.is_canceled())
                        {
                            try
                            {
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
                                        throw;
                                    }
                                }, session->getCancelToken()).wait();


                                if (!strMsg.empty())
                                {
                                    std::error_code errCode;
                                    if (auto json = web::json::value::parse(strMsg, errCode); errCode.value() == 0)
                                    {
                                        extractUsdFuturesUserData(session, std::move(json));
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


        void extractUsdFuturesUserData(shared_ptr<WebSocketSession> session, web::json::value&& jsonVal)
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
                const utility::string_t EventMarginCall = utility::conversions::to_string_t("MARGIN_CALL");
                const utility::string_t EventOrderTradeUpdate = utility::conversions::to_string_t("ORDER_TRADE_UPDATE");
                const utility::string_t EventAccountUpdate = utility::conversions::to_string_t("ACCOUNT_UPDATE");


                UsdFutureUserData::EventType type = UsdFutureUserData::EventType::Unknown;

                auto& eventValue = jsonVal.at(EventTypeField).as_string();

                if (eventValue == EventMarginCall)
                {
                    type = UsdFutureUserData::EventType::MarginCall;
                }
                else if (eventValue == EventOrderTradeUpdate)
                {
                    type = UsdFutureUserData::EventType::OrderUpdate;
                }
                else if (eventValue == EventAccountUpdate)
                {
                    type = UsdFutureUserData::EventType::AccountUpdate;
                }

                UsdFutureUserData userData(type);

                if (type != UsdFutureUserData::EventType::Unknown)
                {
                    switch (type)
                    {

                    case UsdFutureUserData::EventType::MarginCall:
                    {

                        const utility::string_t BalancesField = utility::conversions::to_string_t("B");

                        getJsonValues(jsonVal, userData.mc.data, { "e", "E", "cw" });

                        for (auto& balance : jsonVal[BalancesField].as_array())
                        {
                            map<string, string> values;
                            getJsonValues(balance, values, { "s", "ps", "pa", "mt", "iw", "mp", "up", "mm" });

                            userData.mc.positions[values["s"]] = std::move(values);
                        }
                    }
                    break;


                    case UsdFutureUserData::EventType::OrderUpdate:
                    {
                        const utility::string_t OrdersField = utility::conversions::to_string_t("o");

                        getJsonValues(jsonVal, userData.ou.data, { "e", "E", "T" });

                        map<string, string> values;
                        getJsonValues(jsonVal[OrdersField].as_object(), values, { "s", "c", "S", "o", "f", "q", "p", "ap", "sp", "x", "X", "i", "l", "z", "L", "N", "n", "T", "t", "b", "a", "m", "R", "wt", "ot", "ps", "cp", "AP", "cr", "rp" });

                        userData.ou.orders[values["s"]] = std::move(values);
                    }
                    break;


                    case UsdFutureUserData::EventType::AccountUpdate:
                    {
                        const utility::string_t UpdateDataField = utility::conversions::to_string_t("a");
                        const utility::string_t ReasonDataField = utility::conversions::to_string_t("m");
                        const utility::string_t BalancesField = utility::conversions::to_string_t("B");
                        const utility::string_t PositionsField = utility::conversions::to_string_t("P");


                        getJsonValues(jsonVal, userData.au.data, { "e", "E", "T" });

                        auto& updateDataJson = jsonVal[UpdateDataField].as_object();

                        userData.au.reason = utility::conversions::to_utf8string(updateDataJson.at(ReasonDataField).as_string());

                        for (auto& balance : updateDataJson.at(BalancesField).as_array())
                        {
                            map<string, string> values;
                            getJsonValues(balance, values, { "a", "wb", "cw" });

                            userData.au.balances.emplace_back(std::move(values));
                        }

                        if (auto positions = updateDataJson.find(PositionsField); positions != updateDataJson.end())
                        {
                            for (auto& position : positions->second.as_array())
                            {
                                map<string, string> values;
                                getJsonValues(position, values, { "s", "pa", "ep", "cr", "up", "mt", "iw", "ps" });

                                userData.au.positions.emplace_back(std::move(values));
                            }
                        }
                    }
                    break;


                    default:
                        // handled above
                        break;
                    }


                    session->onUsdFuturesUserDataCallback(std::move(userData));
                }
            }
        }
    };



    /// <summary>
    ///  Uses Binance's Test Net market. Most endpoints are available, including data streams for orders. 
    ///  See:  https://testnet.binancefuture.com/en/futures/BTC_USDT
    ///  To use the TestNet you must:
    ///     1) Create/login to an account on https://testnet.binancefuture.com/en/futures/BTC_USDT
    ///     2) Unlike the 'real' accounts, there's no API Management page, instead there's an "API Key" section at the bottom of the trading page, to the right of Positions, Open Orders, etc
    /// </summary>
    class UsdFuturesTestMarket : public UsdFuturesMarket
    {
    public:
        UsdFuturesTestMarket(const string& apiKey = {}, const string& secretKey = {}) : UsdFuturesMarket(MarketType::FuturesTest, TestFuturestWebSockUri, apiKey, secretKey)
        {

        }

        virtual ~UsdFuturesTestMarket()
        {
        }
    };

}

#endif

