#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <any>
#include <set>
#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include "Logger.hpp"

// Binance Web Sockets
namespace binancews
{
    namespace ws = web::websockets;
    namespace json = web::json;

    using std::string;
    using std::vector;
    using std::shared_ptr;
    using std::map;
    using std::set;


    /// <summary>
    /// Provides an API to the Binance exchange. Currently only websocket streams are available, see the monitor*() functions.
    /// 
    /// A monitor function requires an std::function which is your callback function. There are two types of callback args: 
    ///     
    /// 1) std::function<void(BinanceKeyValueData)>
    /// - Functions which take the BinanceKeyValueData put market data as plain key/value as returned by the API:  map<string, string>
    /// 
    /// {"s", "GRTUSDT}, {"p", "1.99867000"} ... etc
    /// 
    /// 
    /// 2) std::function<void(BinanceKeyMultiValueData)>
    /// - Functions which take the BinanceKeyMultiValueData put data in:  map<string, map<string, string>>. The outer key is the symbol. The value (inner map) is
    ///   key/value data for that symbol.
    /// 
    ///  { "ZENUSDT", {"l", "49.79400000"}, {"o", "50.52900000"}, ... etc},
    ///  { "GRTUSDT", {"l", "1249.45340000"}, {"o", "1251.25340000"}, ... etc},
    /// 
    /// </summary>
    class Binance 
    {
    public:
        enum class Market { Spot, Futures };


        typedef size_t MonitorTokenId;

        struct BinanceKeyValueData
        {
            BinanceKeyValueData() = default;

            BinanceKeyValueData(map<string, string>&& vals) : values(std::move(vals))
            {

            }


            map<string, string> values;
        };

        struct BinanceKeyMultiValueData
        {
            BinanceKeyMultiValueData() = default;

            BinanceKeyMultiValueData(map<string, map<string, string>>&& vals) : values(std::move(vals))
            {

            }

            map<string, map<string, string>> values;
        };

        struct UserDataStreamData
        {
            enum class EventType { Unknown, AccountUpdate, BalanceUpdate, OrderUpdate };

            UserDataStreamData() = delete;

            UserDataStreamData(const EventType t) : type(t)
            {

            }

            map<string, string> data;
            map<string, map<string, string>> balances; // only for EventType::AccountUpdate

            EventType type;
        };



        enum class UserDataStreamMode { Spot };


    private:
        const string SpotWebSockUri         = "wss://stream.binance.com:9443";
        const string FuturestWebSockAddress = "wss://fstream.binance.com";


        struct WebSocketSession
        {
            WebSocketSession() : connected(false), id(0), cancelToken(cancelTokenSource.get_token())
            {

            }

            WebSocketSession(WebSocketSession&& other) noexcept : uri(std::move(uri)), client(std::move(other.client)), receiveTask(std::move(other.receiveTask)),
                cancelTokenSource(std::move(other.cancelTokenSource)), id(other.id),
                onDataUserCallback(std::move(other.onDataUserCallback)), onMultiValueDataUserCallback(std::move(other.onMultiValueDataUserCallback)), cancelToken(std::move(other.cancelToken))
            {
                connected.store(other.connected ? true : false);
            }

            WebSocketSession(const WebSocketSession&) = delete;
            WebSocketSession operator=(const WebSocketSession&) = delete;

            string uri;
            std::atomic_bool connected;

            ws::client::websocket_client client;
            pplx::task<void> receiveTask;

            std::function<void(BinanceKeyValueData)> onDataUserCallback;
            std::function<void(BinanceKeyMultiValueData)> onMultiValueDataUserCallback;
            std::function<void(UserDataStreamData)> onUserDataStreamCallback;


            MonitorTokenId id;


            void cancel()
            {
                cancelTokenSource.cancel();
            }


            pplx::cancellation_token getCancelToken()
            {
                return cancelToken;
            }


        private:
            pplx::cancellation_token_source cancelTokenSource;
            pplx::cancellation_token cancelToken;
        };

        typedef map<string, set<string>> JsonKeys;

    public:
        
        /// <summary>
        /// Returned by monitor functions, containing an ID for use with cancelMonitor() to close this stream.
        /// </summary>
        struct MonitorToken
        {
            MonitorToken() : id(0) {}
            MonitorToken(MonitorTokenId mId) : id(mId) {}
            
            MonitorTokenId id;

            bool isValid() const { return id > 0; }
        };


        Binance(const Market market) : m_connected(false), m_running(false), m_monitorId(1)
        {
            m_exchangeBaseUri = market == Market::Spot ? SpotWebSockUri : FuturestWebSockAddress;
        }


        ~Binance()
        {
            disconnect();
        }


        Binance(const Binance&) = delete;
        Binance(Binance&&) = delete;    // TODO implement this
        Binance operator=(const Binance&) = delete;



        /// <summary>
        /// Receives from the miniTicker stream for all symbols
        /// Updates every 1000ms (limited by the Binance API).
        /// </summary>
        /// <param name="onData">Your callback function. See this classes docs for an explanation</param>
        /// <returns>A MonitorToken. If MonitorToken::isValid() is a problem occured.</returns>
        MonitorToken monitorMiniTicker(std::function<void(BinanceKeyMultiValueData)> onData)
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

    
        /// <summary>
        /// Receives from the symbol mini ticker
        /// Updated every 1000ms (limited by the Binance API).
        /// </summary>
        /// <param name="symbol">The symbtol to monitor</param>
        /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
        /// <returns></returns>
        MonitorToken monitorSymbol(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
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
        /// Receives from the Individual Symbol Book stream for a given symbol.
        /// </summary>
        /// <param name="symbol">The symbol</param>
        /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
        /// <returns></returns>
        MonitorToken monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
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


        /// <summary>
        /// Receives from the 
        /// </summary>
        /// <param name="symbol"></param>
        /// <param name="onData"></param>
        /// <returns></returns>
        MonitorToken monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(BinanceKeyMultiValueData)> onData)
        {
            static const JsonKeys keys
            {
                {"e", {}},
                {"E", {}},
                {"s", {}},
                {"k", {"t", "T", "s", "i", "f", "L", "o", "c", "h", "l", "v", "n", "x", "q", "V", "Q", "B"}}
            };


            auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + symbol + "@kline_"+ interval, keys);

            if (std::get<0>(tokenAndSession).isValid())
            {
                std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
            }

            return std::get<0>(tokenAndSession);
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
        /// CLose stream for the given token.
        /// </summary>
        /// <param name="mt"></param>
        void cancelMonitor(const MonitorToken& mt)
        {
            if (auto it = m_idToSession.find(mt.id); it != m_idToSession.end())
            {
                disconnect(mt, true);
            }
        }


        /// <summary>
        /// Close all streams.
        /// </summary>
        void cancelMonitors()
        {
            disconnect();
        }


        // User Data Stream
        MonitorToken monitorUserData(const string& apiKey, std::function<void(UserDataStreamData)> onData, const UserDataStreamMode mode = UserDataStreamMode::Spot)
        {
            m_apiKey = apiKey;
            MonitorToken monitorToken;

            if (createListenKey())
            {
                if (auto session = connect(m_exchangeBaseUri + "/ws/" + m_listenKey); session)
                {
                    try
                    {
                        auto token = session->getCancelToken();

                        session->receiveTask = pplx::create_task([session, token, this]
                        {
                            while (!token.is_canceled())
                            {
                                session->client.receive().then([=](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
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


                                            // we will receive a 'ping' from bianance, which cpprestsdk sends to here but we can ignore it
                                            if (web::json::value jsonVal = web::json::value::parse(strMsg); jsonVal.size())
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
                                                    


                                                    UserDataStreamData::EventType type = UserDataStreamData::EventType::Unknown;

                                                    if (jsonVal.at(EventTypeField).as_string() == EventOutboundAccountPosition)
                                                    {
                                                        type = UserDataStreamData::EventType::AccountUpdate;
                                                    }
                                                    else if (jsonVal.at(EventTypeField).as_string() == EventBalanceUpdate)
                                                    {
                                                        type = UserDataStreamData::EventType::BalanceUpdate;
                                                    }
                                                    else if (jsonVal.at(EventTypeField).as_string() == EventExecutionReport)
                                                    {
                                                        type = UserDataStreamData::EventType::OrderUpdate;
                                                    }

                                                    
                                                    UserDataStreamData userData(type);

                                                    if (type != UserDataStreamData::EventType::Unknown)
                                                    {
                                                        switch (type)
                                                        {

                                                        case UserDataStreamData::EventType::AccountUpdate:
                                                        {
                                                            getJsonValues(jsonVal, userData.data, { "e", "E", "u" });

                                                            for (auto& balance : jsonVal[BalancesField].as_array())
                                                            {
                                                                map<string, string> values;
                                                                getJsonValues(balance, values, { "a", "f", "l" });

                                                                userData.balances[values["a"]] = std::move(values);
                                                            }
                                                        }
                                                        break;


                                                        case UserDataStreamData::EventType::BalanceUpdate:
                                                            getJsonValues(jsonVal, userData.data, { "e", "E", "a", "d", "T" });
                                                            break;


                                                        case UserDataStreamData::EventType::OrderUpdate:
                                                            getJsonValues(jsonVal, userData.data, { "e", "E", "s", "c", "S", "o", "f", "q", "p", "P", "F", "g", "C", "x", "X", "r", "i", "l", "z",
                                                                                                    "L", "n", "N", "T", "t", "I", "w", "m", "M", "O", "Z", "Y", "Q" });
                                                            break;


                                                        default:
                                                            // handled above
                                                            break;
                                                        }


                                                        session->onUserDataStreamCallback(std::move(userData));
                                                    }
                                                }                                                
                                            }                                            
                                        }
                                        catch (...)
                                        {

                                        }
                                    }
                                    else
                                    {
                                        pplx::cancel_current_task();
                                    }

                                }, token).wait();
                            }

                            pplx::cancel_current_task();

                        }, token);

                        monitorToken.id = m_monitorId++;
                        session->id = monitorToken.id;

                        m_sessions.push_back(session);                        
                        m_idToSession[monitorToken.id] = session;                        
                    }
                    catch (...)
                    {

                    }
                }
            }

            return monitorToken;
        }


    private:
        shared_ptr<WebSocketSession> connect(const string& uri)
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


        std::tuple<MonitorToken, shared_ptr<WebSocketSession>> createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey = {})
        {
            std::tuple<MonitorToken, shared_ptr<WebSocketSession>> tokenAndSession;

            if (auto session = connect(uri); session)
            {
                auto extractFunction = std::bind(&Binance::extractKeys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

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


        void disconnect(const MonitorToken& mt, const bool deleteSession)
        {
            if (auto itIdToSession = m_idToSession.find(mt.id); itIdToSession != m_idToSession.end())
            {
                auto& session = itIdToSession->second;

                session->cancel();
                session->receiveTask.wait();

                session->client.close(ws::client::websocket_close_status::normal).then([&session]()
                {
                    session->connected = false;
                }).wait();

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


        void disconnect()
        {
            vector<pplx::task<void>> disconnectTasks;

            for (const auto& idToSession : m_idToSession)
            {
                disconnectTasks.emplace_back(pplx::create_task([&idToSession, this]() { disconnect(idToSession.first, false);  }));
            }

            pplx::when_all(disconnectTasks.begin(), disconnectTasks.end()).wait();

            m_idToSession.clear();
            m_sessions.clear();
        }


        void extractKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const JsonKeys& keys, const string& arrayKey = {})
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
        

        void getJsonValues(web::json::value& jsonVal, map<string, string>& values, const std::set<string>& keys)
        {
            for (auto& k : keys)
            {
                getJsonValues(jsonVal, values, k);
            }
        }


        void getJsonValues(web::json::object& jsonObj, map<string, string>& values, const std::set<string>& keys)
        {
            for (auto& v : jsonObj)
            {
                auto keyUtf8String = utility::conversions::to_utf8string(v.first);

                if (keys.find(utility::conversions::to_utf8string(v.first)) != keys.cend())
                {
                    auto& keyJsonString = utility::conversions::to_string_t(v.first);

                    string valueString;

                    switch (auto t = v.second.type(); t)
                    {
                        // [[likely]] TODO attribute in C++20
                    case json::value::value_type::String:
                        valueString = utility::conversions::to_utf8string(v.second.as_string());
                        break;

                    case json::value::value_type::Number:
                        valueString = std::to_string(v.second.as_number().to_int64());
                        break;

                        // [[unlikely]] TODO attribute in C++20
                    case json::value::value_type::Boolean:
                        valueString = v.second.as_bool() ? utility::conversions::to_utf8string("true") : utility::conversions::to_utf8string("false");
                        break;

                    default:
                        logg("No handler for JSON type: " + std::to_string(static_cast<int>(t)));
                        break;
                    }

                    values[keyUtf8String] = std::move(valueString);
                }                
            }
        }


        void getJsonValues(web::json::value& jsonVal, map<string, string>& values, const string& key)
        {
            auto keyJsonString = utility::conversions::to_string_t(key);

            if (jsonVal.has_field(keyJsonString))
            {
                string valueString;

                switch (auto t = jsonVal[keyJsonString].type(); t)
                {
                    // [[likely]] TODO attribute in C++20
                case json::value::value_type::String:
                    valueString = utility::conversions::to_utf8string(jsonVal[keyJsonString].as_string());
                    break;

                case json::value::value_type::Number:
                    valueString = std::to_string(jsonVal[keyJsonString].as_number().to_int64());
                    break;

                    // [[unlikely]] TODO attribute in C++20
                case json::value::value_type::Boolean:
                    valueString = jsonVal[keyJsonString].as_bool() ? utility::conversions::to_utf8string("true") : utility::conversions::to_utf8string("false");
                    break;

                default:
                    logg("No handler for JSON type: " + std::to_string(static_cast<int>(t)));
                    break;
                }

                values[key] = std::move(valueString);
            }
        }


        MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey)
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



        // user data stream
        bool createListenKey()
        {
            const auto HeaderKeyName = utility::conversions::to_string_t("X-MBX-APIKEY");
            const auto SpotRequestUri = utility::conversions::to_string_t("/api/v3/userDataStream");
            const auto ListenKeyName = utility::conversions::to_string_t("listenKey");


            bool ok = false;

            web::uri userDataUri (utility::conversions::to_string_t("https://api.binance.com"));
            web::http::client::http_client client{ userDataUri };

            web::http::http_request request{ web::http::methods::POST };
            request.headers().add(HeaderKeyName, utility::conversions::to_string_t(m_apiKey));
            request.set_request_uri(SpotRequestUri);

            client.request(request).then([&ok, &ListenKeyName, this](web::http::http_response response)
            {
                if (response.status_code() == web::http::status_codes::OK)
                {
                    ok = true;
                    m_listenKey = utility::conversions::to_utf8string(response.extract_json().get()[ListenKeyName].as_string());
                }
            }).wait();

            return ok;
        }


    private:
        vector<shared_ptr<WebSocketSession>> m_sessions;
        map<size_t, shared_ptr<WebSocketSession>> m_idToSession;

        std::atomic_size_t m_monitorId;
        string m_exchangeBaseUri;
        std::atomic_bool m_connected;
        std::atomic_bool m_running;
        string m_apiKey;
        string m_listenKey;

    };
}