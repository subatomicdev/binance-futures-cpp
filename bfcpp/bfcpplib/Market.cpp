#include "Market.hpp"


namespace bfcpp 
{
    Market::Market(const MarketType market, const string& exchangeBaseUri, const ApiAccess& access) :
        m_connected(false),
        m_running(false),
        m_marketType(market),
        m_exchangeBaseUri(exchangeBaseUri),
        m_apiAccess(access)
    {
       m_monitorId = 1U;
    }


    Market::~Market()
    {
        disconnect();
    }

    

    MonitorToken Market::monitorMiniTicker(std::function<void(BinanceKeyMultiValueData)> onData)
    {
        static const JsonKeys keys
        {
            { {"s"}, {"e", "E", "s", "c", "o", "h", "l", "v", "q"} }
        };

        if (onData == nullptr)
        {
            throw std::runtime_error("monitorMiniTicker callback function null");
        }

        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/!miniTicker@arr", keys, "s");

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }



    MonitorToken Market::monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(BinanceKeyMultiValueData)> onData)
    {
        static const JsonKeys keys
        {
            {"e", {}},
            {"E", {}},
            {"s", {}},
            {"k", {"t", "T", "s", "i", "f", "L", "o", "c", "h", "l", "v", "n", "x", "q", "V", "Q", "B"}}
        };

        if (onData == nullptr)
        {
            throw std::runtime_error("monitorKlineCandlestickStream callback function null");
        }


        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@kline_" + interval, keys);

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }


   
    MonitorToken Market::monitorSymbol(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
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

        if (onData == nullptr)
        {
            throw std::runtime_error("monitorSymbol callback function null");
        }


        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@miniTicker", keys);

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }


    MonitorToken Market::monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
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

        if (onData == nullptr)
        {
            throw std::runtime_error("monitorSymbolBookStream callback function null");
        }


        auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@bookTicker", keys);

        if (std::get<0>(tokenAndSession).isValid())
        {
            std::get<1>(tokenAndSession)->onDataUserCallback = onData;
        }

        return std::get<0>(tokenAndSession);
    }


    void Market::disconnect(const MonitorToken& mt, const bool deleteSession)
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


    std::tuple<MonitorToken, shared_ptr<Market::WebSocketSession>> Market::createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey)
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


    MonitorToken Market::createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey)
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
            // build the request, with appropriate headers, the API key and the query string with the signature appended

            string uri = getApiUri();
            string path = getApiPath(RestCall::ListenKey);
            string queryString;

            if (marketType == MarketType::Futures || marketType == MarketType::FuturesTest)
            {
                queryString = createQueryString(map<string, string>{}, RestCall::ListenKey, true);
            }            
            
            auto request = createHttpRequest(web::http::methods::POST, path + "?" + queryString);

            web::http::client::http_client client{ web::uri{utility::conversions::to_string_t(uri)} };

            client.request(std::move(request)).then([&ok, this](web::http::http_response response)
            {
                auto json = response.extract_json().get();

                if (response.status_code() == web::http::status_codes::OK)
                {
                    m_listenKey = utility::conversions::to_utf8string(json[utility::conversions::to_string_t(ListenKeyName)].as_string());
                    ok = true;
                }
                else
                {
                    throw std::runtime_error{ "Failed to create listne key:  " + utility::conversions::to_utf8string(json.serialize()) };
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