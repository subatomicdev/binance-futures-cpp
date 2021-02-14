#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <any>
#include <set>
#include <cpprest/ws_client.h>
#include <cpprest/json.h>
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



    class Binance 
    {
    public:
        typedef size_t MonitorTokenId;

        struct BinanceKeyValueData
        {
            BinanceKeyValueData(map<string, string>&& vals) : values(std::move(vals))
            {

            }


            map<string, string> values;
        };

        struct BinanceKeyMultiValueData
        {
            BinanceKeyMultiValueData(map<string, map<string, string>>&& vals) : values(std::move(vals))
            {

            }

            map<string, map<string, string>> values;
        };


    private:
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


    public:
        

        struct MonitorToken
        {
            MonitorToken() : id(0) {}
            MonitorToken(MonitorTokenId mId) : id(mId) {}
            
            MonitorTokenId id;

            bool isValid() const { return id > 0; }
        };


        Binance(const string uri = "wss://stream.binance.com:9443") : m_connected(false), m_running(false), m_exchangeBaseUri(uri), m_monitorId(1)
        {

        }


        ~Binance()
        {
            disconnect();
        }


        Binance(const Binance&) = delete;
        Binance(Binance&&) = delete;    // TODO implement this
        Binance operator=(const Binance&) = delete;



        shared_ptr<WebSocketSession> connect(const string& uri)
        {
            auto session = std::make_shared<WebSocketSession>();
            session->uri = uri;

            try
            {
                web::uri wsUri (utility::conversions::to_string_t(uri));
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


        /// <summary>
        /// Receives from the miniTicker stream for all symbols (https://binance-docs.github.io/apidocs/spot/en/#all-market-mini-tickers-stream).
        /// he updates are every 1000ms (limited by the Binance API).
        /// </summary>
        /// <param name="onData">A map with pairs of: [symbol, close price] </param>
        /// <returns>A MonitorToken. If MonitorToken::isValid() is a problem occured.</returns>
        MonitorToken monitorAllSymbols(std::function<void(BinanceKeyMultiValueData)> onData)
        {
            MonitorToken monitor;

            if (auto session = connect(m_exchangeBaseUri + "/ws/!miniTicker@arr"); session)
            {
                static const std::set<string> keys = { "e", "E", "s", "c", "o", "h", "l", "v", "q" };

                auto extractFunction = std::bind(&Binance::extractAllKeys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

                if (monitor = createReceiveTask(session, extractFunction, keys);  monitor.isValid())
                {
                    session->id = monitor.id;
                    session->onMultiValueDataUserCallback = onData;

                    m_sessions.push_back(session);
                    m_idToSession[monitor.id] = session;
                }
            }

            return monitor;
        }

    
        /// <summary>
        /// Receives from the Trade Streams for a given symbol (https://binance-docs.github.io/apidocs/spot/en/#trade-streams).
        /// The updates in real time.
        /// </summary>
        /// <param name="symbol">The symbol to receive trades information</param>
        /// <param name="onData">A map with pairs of: [symbol, close price] </param>
        /// <returns>A MonitorToken. If MonitorToken::isValid() is a problem occured.</returns>
        MonitorToken monitorTradeStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
        {
            MonitorToken monitor;

            if (auto session = connect(m_exchangeBaseUri + "/ws/" + symbol + "@trade"); session)
            {   
                static const std::set<string> keys = { "e", "E", "s", "t", "p", "q", "b", "a", "T", "m", "M" };

                auto extractFunction = std::bind(&Binance::extractAllKeys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

                if (monitor = createReceiveTask(session, extractFunction, keys);  monitor.isValid())
                {
                    session->id = monitor.id;
                    session->onDataUserCallback = onData;

                    m_sessions.push_back(session);
                    m_idToSession[monitor.id] = session;
                }
            }

            return monitor;
        }

        
        /// <summary>
        /// Receives from the Individual Symbol Book stream for a given symbol (https://binance-docs.github.io/apidocs/spot/en/#individual-symbol-book-ticker-streams)
        /// </summary>
        /// <param name="symbol"></param>
        /// <param name="onData"></param>
        /// <returns></returns>
        MonitorToken monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
        {
            MonitorToken monitor;

            if (auto session = connect(m_exchangeBaseUri + "/ws/" + symbol + "@bookTicker"); session)
            {
                static const std::set<string> keys = { "U", "s", "b", "B", "a", "A"};

                auto extractFunction = std::bind(&Binance::extractAllKeys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

                if (monitor = createReceiveTask(session, extractFunction, keys);  monitor.isValid())
                {
                    session->id = monitor.id;
                    session->onDataUserCallback = onData;

                    m_sessions.push_back(session);
                    m_idToSession[monitor.id] = session;
                }
            }

            return monitor;
        }


        void cancelMonitor(const MonitorToken& mt)
        {
            if (auto it = m_idToSession.find(mt.id); it != m_idToSession.end())
            {
                disconnect(mt, true);
            }
        }


        void cancelMonitors()
        {
            disconnect();
        }


    private:
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


        void extractAllTradesMessage(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
        {
            try
            {
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
                    const utility::string_t CodeField = utility::conversions::to_string_t("Code");
                    const utility::string_t MsgField = utility::conversions::to_string_t("msg");
                    const utility::string_t SymbolField = utility::conversions::to_string_t("s");
                    const utility::string_t CloseField = utility::conversions::to_string_t("c");


                    if (jsonVal.has_string_field(CodeField) && jsonVal.has_string_field(MsgField))
                    {
#ifdef WIN32
                        std::wcout << "\nError: " << jsonVal.at(CodeField).as_string() << " : " << jsonVal.at(MsgField).as_string();
#else
                        std::cout << "\nError: " << jsonVal.at(CodeField).as_string() << " : " << jsonVal.at(MsgField).as_string();
#endif                        
                    }
                    else if (session->onDataUserCallback)
                    {
                        if (auto arr = jsonVal.as_array(); arr.size())
                        {
                            map<string, string> values;

                            for (const auto& v : arr)
                            {
                                if (v.has_string_field(SymbolField) && v.has_string_field(CloseField))
                                {
                                    auto& symbol = v.at(SymbolField).as_string();
                                    auto& price = v.at(CloseField).as_string();

                                    values[std::string{ symbol.begin(), symbol.end() }] = std::string{ price.begin(), price.end() };
                                }
                            }

                            session->onDataUserCallback(std::move(values));    // TODO async?
                        }
                    }
                }
            }
            catch (...)
            {

            }
        }


        void extractAllKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const std::set<string>& keys)
        {
            try
            {
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
                    const utility::string_t CodeField = utility::conversions::to_string_t("Code");
                    const utility::string_t MsgField = utility::conversions::to_string_t("msg");

                    if (jsonVal.has_string_field(CodeField) && jsonVal.has_string_field(MsgField))
                    {
#ifdef WIN32
                        std::wcout << "\nError: " << jsonVal.at(CodeField).as_string() << " : " << jsonVal.at(MsgField).as_string();
#else
                        std::cout << "\nError: " << jsonVal.at(CodeField).as_string() << " : " << jsonVal.at(MsgField).as_string();
#endif
                    }
                    else if (session->onDataUserCallback)
                    {
                        map<string, string> values;

                        if (jsonVal.is_array())
                        {
                            for (auto& val : jsonVal.as_array())
                            {
                                getJsonValues(val, values, keys);
                            }
                        }
                        else
                        {
                            getJsonValues(jsonVal, values, keys);
                        }
                        

                        session->onDataUserCallback(std::move(values));    // TODO async?
                    }
                    else if (session->onMultiValueDataUserCallback)
                    {
                        if (jsonVal.is_array())
                        {
                            map<string, map<string, string>> values;

                            for (auto& val : jsonVal.as_array())
                            {
                                map<string, string> innerValues;

                                getJsonValues(val, innerValues, keys);

                                values[innerValues["s"]] = std::move(innerValues);
                            }

                            session->onMultiValueDataUserCallback(std::move(values));    // TODO async?
                        }
                        else
                        {
                            //TODO don't think this is possible, decide
                        }
                    }
                }                
            }
            catch (...)
            {

            }
        }


        

        void getJsonValues(web::json::value jsonVal, map<string, string>& values, const std::set<string>& keys)
        {
            for (const auto& k : keys)
            {
                auto keyJsonString = utility::conversions::to_string_t(k);

                if (jsonVal.has_field(keyJsonString))
                {
                    utility::string_t valueString;

                    switch (auto t = jsonVal[keyJsonString].type(); t)
                    {
                        // [[likely]] TODO attribute in C++20
                    case json::value::value_type::String:
                        valueString = jsonVal[keyJsonString].as_string();
                        break;

                    case json::value::value_type::Number:
                        valueString = std::to_wstring(jsonVal[keyJsonString].as_number().to_int64());
                        break;

                        // [[unlikely]] TODO attribute in C++20
                    case json::value::value_type::Boolean:
                        valueString = jsonVal[keyJsonString].as_bool() ? L"true" : L"false";
                        break;

                    default:
                        logg("No handler for JSON type: " + std::to_string(static_cast<int>(t)));
                        break;
                    }

                    values[k] = string{ valueString.cbegin(), valueString.cend() };
                }
            }
        }


        MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const std::set<string>&)> extractFunc, const std::set<string>& keys = std::set<string>{})
        {
            MonitorToken monitorToken;

            try
            {
                auto token = session->getCancelToken();

                session->receiveTask = pplx::create_task([session, token, extractFunc, keys, this]   // capture by value so 'session' shared_ptr ref count incremented
                {
                    while (!token.is_canceled())
                    {
                        session->client.receive().then([=](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
                        {
                            if (!token.is_canceled())
                            {
                                extractFunc(websocketInMessage.get(), session, keys);
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


    private:
        vector<shared_ptr<WebSocketSession>> m_sessions;
        map<size_t, shared_ptr<WebSocketSession>> m_idToSession;

        std::atomic_size_t m_monitorId;
        string m_exchangeBaseUri;
        std::atomic_bool m_connected;
        std::atomic_bool m_running;

    };
}