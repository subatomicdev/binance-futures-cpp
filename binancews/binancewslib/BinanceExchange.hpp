#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <any>
#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include "Logger.hpp"

// Binance Web Sockets
namespace binancews
{
    namespace ws = web::websockets;

    using std::string;
    using std::vector;
    using std::shared_ptr;
    using std::map;


    class Exchange
    {
    public:
        typedef size_t MonitorTokenId;

        struct MonitorToken
        {
            MonitorToken() {}
            MonitorToken(MonitorTokenId mId) : id(mId) {}

            MonitorTokenId id;

            bool isValid() { return id > 0; }
        };

    protected:
        struct WebSocketSession
        {
            WebSocketSession() : connected(false), id(0), cancelToken(cancelTokenSource.get_token())
            {

            }

            WebSocketSession(WebSocketSession&& other) noexcept : uri(std::move(uri)), client(std::move(other.client)), receiveTask(std::move(other.receiveTask)),
                cancelTokenSource(std::move(other.cancelTokenSource)), id(other.id),
                onDataUserCallback(std::move(other.onDataUserCallback)), cancelToken(std::move(other.cancelToken))
            {
                connected.store(other.connected ? true : false);
            }

            WebSocketSession(const WebSocketSession&) = delete;
            WebSocketSession operator=(const WebSocketSession&) = delete;

            string uri;
            std::atomic_bool connected;

            ws::client::websocket_client client;
            pplx::task<void> receiveTask;

            std::function<void(std::map<string, string>)> onDataUserCallback;

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
        virtual ~Exchange()
        {

        }
                
        virtual void disconnect() = 0;
        virtual map<size_t, shared_ptr<WebSocketSession>>::iterator disconnect(const MonitorToken& mt) = 0;

        virtual MonitorToken monitorAllSymbols(std::function<void(std::map<string, string>)> onData) = 0;
        virtual void cancelMonitor(const MonitorToken& mt) = 0;

    protected:
        Exchange(string name) : m_name(name)
        {

        }


    protected:
        string m_name;
    };



    class Binance : public Exchange
    {
    

       


    public:
        Binance(const string uri = "wss://stream.binance.com:9443") : Exchange("Binance"), m_connected(false), m_running(false), m_exchangeBaseUri(uri), m_monitorId(1)
        {

        }


        virtual ~Binance()
        {
            disconnect();
        }


        Binance(const Binance&) = delete;
        Binance(Binance&&) = delete;    // TODO implement this
        Binance operator=(const Binance&) = delete;



        virtual void disconnect() override
        {
            for (auto sessionIt = m_idToSession.begin() ; sessionIt != m_idToSession.end() ; )
            {
                sessionIt  = disconnect(sessionIt->first);
            }
        }


        map<size_t, shared_ptr<WebSocketSession>>::iterator disconnect(const MonitorToken& mt)
        {
            map<size_t, shared_ptr<WebSocketSession>>::iterator newIdToSessionIterator = m_idToSession.end();

            if (auto itIdToSession = m_idToSession.find(mt.id); itIdToSession != m_idToSession.end())
            {
                auto& session = itIdToSession->second;

                session->cancel();
                session->receiveTask.wait();

                session->client.close(ws::client::websocket_close_status::normal).then([&session]()
                {
                    session->connected = false;
                }).wait();


                auto storedSessionIt = std::find_if(m_sessions.cbegin(), m_sessions.cend(), [this, &mt](auto& sesh) { return sesh->id == mt.id; });

                if (storedSessionIt != m_sessions.end())
                {
                    m_sessions.erase(storedSessionIt);
                }

                newIdToSessionIterator = m_idToSession.erase(itIdToSession);
            }

            return newIdToSessionIterator;
        }


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



        virtual MonitorToken monitorAllSymbols(std::function<void(std::map<string, string>)> onData) override
        {
            auto session = connect(m_exchangeBaseUri + "/ws/!miniTicker@arr");
            auto monitor = createReceiveTask(session);

            if (monitor.isValid())
            {
                session->id = monitor.id;
                session->onDataUserCallback = onData;

                m_sessions.push_back(session);
                m_idToSession[monitor.id] = session;
            }            

            return monitor;
        }


        
        virtual void cancelMonitor(const MonitorToken& mt) override
        {
            if (auto it = m_idToSession.find(mt.id); it != m_idToSession.end())
            {
                disconnect(mt);
            }
        }

    private:
        void extractAndPublishMessage(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
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
                    else
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

                            if (session->onDataUserCallback)
                            {
                                session->onDataUserCallback(std::move(values));    // TODO async?
                            }
                        }
                    }
                }
            }
            catch (...)
            {

            }
        }


        MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session)
        {
            MonitorToken monitorToken;

            try
            {
                auto token = session->getCancelToken();

                session->receiveTask = pplx::create_task([session, token, this]   // capture by value so 'session' shared_ptr ref count incremented
                {
                    while (!token.is_canceled())
                    {
                        session->client.receive().then([=](pplx::task<ws::client::websocket_incoming_message> websocketInMessage)
                        {
                            if (!token.is_canceled())
                            {
                                extractAndPublishMessage(websocketInMessage.get(), session);
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