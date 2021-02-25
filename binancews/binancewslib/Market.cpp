#include "Market.hpp"


namespace binancews
{
    Market::Market(const MarketType market, const string& exchangeBaseUri) : m_connected(false), m_running(false), m_monitorId(1), m_marketType(market), m_exchangeBaseUri(exchangeBaseUri)
    {
    }


    Market::~Market()
    {
        disconnect();
    }

    

    Market::MonitorToken Market::monitorMiniTicker(std::function<void(BinanceKeyMultiValueData)> onData)
    {
        static const JsonKeys keys
        {
            { {"s"}, {"e", "E", "s", "c", "o", "h", "l", "v", "q"} }
        };

        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/!miniTicker@arr", keys, "s");

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }



    Market::MonitorToken Market::monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(BinanceKeyMultiValueData)> onData)
    {
        static const JsonKeys keys
        {
            {"e", {}},
            {"E", {}},
            {"s", {}},
            {"k", {"t", "T", "s", "i", "f", "L", "o", "c", "h", "l", "v", "n", "x", "q", "V", "Q", "B"}}
        };

        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + symbol + "@kline_" + interval, keys);

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }


   
    Market::MonitorToken Market::monitorSymbol(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
    {
        static const JsonKeys keys
        {
            {"e", {}},
            {"E", {}},
            {"s", {}},
            {"c", {}},
            {"o", {}},
            {"h", {}},
            {"l", {}},
            {"v", {}},
            {"q", {}}
        };

        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + symbol + "@miniTicker", keys);

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }


    Market::MonitorToken Market::monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
    {
        static const JsonKeys keys
        {
            {"u", {}},
            {"s", {}},
            {"b", {}},
            {"B", {}},
            {"a", {}},
            {"A", {}}
        };

        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + symbol + "@bookTicker", keys);

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }


    void Market::disconnect(const Market::MonitorToken& mt, const bool deleteSession)
    {
        if (auto itIdToSession = m_idToSession.find(mt.id); itIdToSession != m_idToSession.end())
        {
            auto& session = itIdToSession->second;

            session->cancel();

            // calling wait() on a task that's already cancelled throws an exception
            if (!session->receiveTask.is_done())
            {
                session->receiveTask.wait();
            }                

            session->client.close(ws::client::websocket_close_status::normal).then([&session]()
            {
                session->connected = false;
            }).wait();

            // when called from disconnect() this flag is false to avoid invalidating iterators in m_idToSession, 
            // this is tidier than returning the new iterator from erase()
            if (deleteSession)
            {
                if (auto storedSessionIt = std::find_if(m_sessions.cbegin(), m_sessions.cend(), [this, &mt](auto& sesh) { return sesh->id == mt.id; });  storedSessionIt != m_sessions.end())
                {
                    m_sessions.erase(storedSessionIt);
                }

                m_idToSession.erase(itIdToSession);
            }
        }
    }


    void Market::disconnect()
    {
        vector<pplx::task<void>> disconnectTasks;

        for (const auto& idToSession : m_idToSession)
        {
            disconnectTasks.emplace_back(pplx::create_task([&idToSession, this]
            {
                disconnect(idToSession.first, false);
            }));
        }

        pplx::when_all(disconnectTasks.begin(), disconnectTasks.end()).wait();

        m_idToSession.clear();
        m_sessions.clear();
    }



    shared_ptr<Market::WebSocketSession> Market::connect(const string& uri)
    {
        auto session = std::make_shared<WebSocketSession>();
        session->uri = uri;

        try
        {
            web::uri wsUri(utility::conversions::to_string_t(uri));
            session->client.connect(wsUri).then([&session]
            {
                session->connected = true;
            }).wait();
        }
        catch (const web::websockets::client::websocket_exception we)
        {
            std::cout << we.what();
        }
        catch (const std::exception ex)
        {
            std::cout << ex.what();
        }

        return session;
    }


    std::tuple<Market::MonitorToken, shared_ptr<Market::WebSocketSession>> Market::createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey)
    {
        std::tuple<MonitorToken, shared_ptr<WebSocketSession>> tokenAndSession;

        if (auto session = connect(uri); session)
        {
            auto extractFunction = std::bind(&Market::extractKeys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

            if (MonitorToken monitor = createReceiveTask(session, extractFunction, keys, arrayKey);  monitor.isValid())
            {
                session->id = monitor.id;

                m_sessions.push_back(session);
                m_idToSession[monitor.id] = session;

                tokenAndSession = std::make_tuple(monitor, session);
            }
        }

        return tokenAndSession;
    }


    void Market::extractKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const JsonKeys& keys, const string& arrayKey)
    {
        try
        {
            // get the payload synchronously
            std::string strMsg;
            websocketInMessage.extract_string().then([=, &strMsg, cancelToken = session->getCancelToken()](pplx::task<std::string> str_tsk)
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


            // we have the message as a string, pass to the json parser and extract fields if no error
            if (web::json::value jsonVal = web::json::value::parse(strMsg); jsonVal.size())
            {
                const utility::string_t CodeField = utility::conversions::to_string_t("code");
                const utility::string_t MsgField = utility::conversions::to_string_t("msg");

                if (jsonVal.has_string_field(CodeField) && jsonVal.has_string_field(MsgField))
                {
                    std::cout << "\nError: " << utility::conversions::to_utf8string(jsonVal.at(CodeField).as_string()) << " : " << utility::conversions::to_utf8string(jsonVal.at(MsgField).as_string());
                }
                else if (session->onDataUserCallback)
                {
                    map<string, string> values;

                    for (const auto& key : keys)
                    {
                        getJsonValues(jsonVal, values, key.first);
                    }

                    session->onDataUserCallback(std::move(values));    // TODO async?
                }
                else if (session->onMultiValueDataUserCallback)
                {
                    map<string, map<string, string>> values;

                    if (jsonVal.is_array())
                    {
                        for (auto& val : jsonVal.as_array())
                        {
                            map<string, string> innerValues;

                            getJsonValues(val, innerValues, keys.find(arrayKey)->second);

                            values[innerValues[arrayKey]] = std::move(innerValues);
                        }
                    }
                    else
                    {
                        for (const auto& key : keys)
                        {
                            if (key.second.empty())
                            {
                                map<string, string> inner;

                                getJsonValues(jsonVal, inner, key.first);

                                values[key.first] = std::move(inner);
                            }
                            else
                            {
                                // key has nested keys
                                map<string, string> inner;

                                if (jsonVal.at(utility::conversions::to_string_t(key.first)).is_object())
                                {
                                    getJsonValues(jsonVal.at(utility::conversions::to_string_t(key.first)).as_object(), inner, key.second);
                                    values[key.first] = std::move(inner);
                                }
                            }
                        }
                    }

                    session->onMultiValueDataUserCallback(std::move(values));    // TODO async?
                }
            }
        }
        catch (...)
        {

        }
    }


    Market::MonitorToken Market::createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey)
    {
        MonitorToken monitorToken;

        try
        {
            auto token = session->getCancelToken();

            session->receiveTask = pplx::create_task([session, token, extractFunc, keys, arrayKey, this]
            {
                while (!token.is_canceled())
                {
                    session->client.receive().then([=](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
                    {
                        if (!token.is_canceled())
                        {
                            extractFunc(websocketInMessage.get(), session, keys, arrayKey);
                        }
                        else
                        {
                            pplx::cancel_current_task();
                        }

                    }, token).wait();
                }

                pplx::cancel_current_task();

            }, token);


            monitorToken.id = m_monitorId;

            ++m_monitorId;
        }
        catch (const web::websockets::client::websocket_exception we)
        {
            std::cout << we.what();
        }
        catch (const std::exception ex)
        {
            std::cout << ex.what();
        }

        return monitorToken;
    }


    bool Market::createListenKey(const MarketType marketType)
    {
        bool ok = false;

        try
        {
            string uri;
            string path;

            switch (marketType)
            {
            case MarketType::Spot:
                uri = SpotRestUri;
                path = SpotRequestPath;
                break;

            case MarketType::SpotTest:
                uri = TestSpotRestUri;
                path = SpotRequestPath;
                break;

            case MarketType::Futures:
                uri = UsdFuturesRestUri;
                path = UsdFuturesRequestPath;
                break;

            case MarketType::FuturesTest:
                uri = TestUsdFuturestRestUri;
                path = UsdFuturesRequestPath;
                break;
            }

            string queryString;
            if (marketType == MarketType::Futures || marketType == MarketType::FuturesTest)
            {
                std::stringstream query;
                query << "timestamp=" << get_current_ms_epoch() << "&recvWindow=5000";

                queryString = query.str() + "&signature=" + createSignature(m_secretKey, query.str());
            }

            // build the request, with appropriate headers, the API key and the query string with the signature appended
            web::uri requstUri(utility::conversions::to_string_t(path + "?" + queryString));

            web::http::http_request request{ web::http::methods::POST };
            request.headers().add(utility::conversions::to_string_t(ContentTypeName), utility::conversions::to_string_t("application/json"));
            request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiKey));
            request.headers().add(utility::conversions::to_string_t(ClientSDKVersionName), utility::conversions::to_string_t("binancews_cpp_alpha"));
            request.set_request_uri(requstUri);


            web::http::client::http_client client{ web::uri{utility::conversions::to_string_t(uri)} };
            client.request(request).then([&ok, this](web::http::http_response response)
            {
                if (response.status_code() == web::http::status_codes::OK)
                {
                    ok = true;
                    m_listenKey = utility::conversions::to_utf8string(response.extract_json().get()[utility::conversions::to_string_t(ListenKeyName)].as_string());
                }
                else if (response.status_code() == web::http::status_codes::Unauthorized)
                {
                    throw std::runtime_error{"Binance returned HTTP 401 error whilst creating listen key. Ensure your API and secret keys have permissions enabled for this market"};
                }
            }).wait();
        }
        catch (const web::websockets::client::websocket_exception we)
        {
            logg(we.what());
        }
        catch (const std::exception ex)
        {
            logg(ex.what());
        }

        return ok;
    }
}