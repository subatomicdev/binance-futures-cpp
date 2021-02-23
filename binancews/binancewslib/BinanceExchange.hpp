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
#include <openssl/hmac.h>
#include "Logger.hpp"
#include "IntervalTimer.hpp"


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

    using namespace std::chrono_literals;



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
    class Market
    {
    public:
        enum class MarketType { Spot, Futures, FuturesTest };

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

        struct SpotUserData
        {
            enum class EventType { Unknown, AccountUpdate, BalanceUpdate, OrderUpdate };

            SpotUserData() = delete;

            SpotUserData(const EventType t) : type(t)
            {

            }

            

            map<string, string> data;
            map<string, map<string, string>> balances; // only for when type is AccountUpdate
            EventType type;
        };

        struct UsdFutureUserData
        {
            enum class EventType { Unknown, MarginCall, OrderUpdate, AccountUpdate };

            UsdFutureUserData() = delete;

            UsdFutureUserData(const EventType t) : type(t)
            {

            }

            struct MarginCall
            {
                map<string, string> data;
                map<string, map<string, string>> positions;
            } mc;

            struct OrderUpdate
            {
                map<string, string> data;
                map<string, map<string, string>> orders;
            } ou;

            struct AccountUpdate
            {
                /*
                enum class UpdateReason {   Deposit, Withdraw, Order, FundingFee, WithdrawReject, Adjustment, InsuranceClear, AdminDeposit,
                                            AdminWithdraw, MarginTransfer, MarginTypeChange, AssetTransfer, OptionsPremiumFee, OptionsSettleProfit
                                        };
                inline static const map<string, UpdateReason> ReasonMap = {  {"DEPOSIT", UpdateReason::Deposit},{"WITHDRAW", UpdateReason::Withdraw},{"ORDER", UpdateReason::Order}, {"FUNDING_FEE", UpdateReason::FundingFee},{"WITHDRAW_REJECT", UpdateReason::WithdrawReject},{"ADJUSTMENT", UpdateReason::Adjustment},
                                                                    {"INSURANCE_CLEAR", UpdateReason::InsuranceClear},{"ADMIN_DEPOSIT", UpdateReason::AdminDeposit},{"ADMIN_WITHDRAW", UpdateReason::AdminWithdraw},{"MARGIN_TRANSFER", UpdateReason::MarginTransfer},{"MARGIN_TYPE_CHANGE", UpdateReason::MarginTypeChange},
                                                                    {"ASSET_TRANSFER", UpdateReason::AssetTransfer},{"OPTIONS_PREMIUM_FEE", UpdateReason::OptionsPremiumFee},{"OPTIONS_SETTLE_PROFIT", UpdateReason::OptionsSettleProfit} };
                */

                map<string, string> data;
                string reason;
                vector<map<string, string>> balances;
                vector<map<string, string>> positions;
            } au;

            EventType type;
        };


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


        enum class UserDataStreamMode { Spot };

        const string SpotWebSockUri             = "wss://stream.binance.com:9443";
        const string FuturestWebSockUri         = "wss://fstream.binance.com";
        const string TestFuturestWebSockUri     = "wss://stream.binancefuture.com";

        const string UsdFuturesRestUri      = "https://fapi.binance.com";
        const string TestUsdFuturestRestUri = "https://testnet.binancefuture.com";

        const string SpotRequestPath        =  "/api/v3/userDataStream";
        const string UsdFuturesRequestPath  = "/fapi/v1/listenKey";

        const string HeaderApiKeyName = "X-MBX-APIKEY";
        const string ListenKeyName = "listenKey";


        const string SpotRestUri = "https://api.binance.com";


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
            std::function<void(SpotUserData)> onSpotUserDataCallback;
            std::function<void(UsdFutureUserData)> onUsdFuturesUserDataCallback;

            

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

        

    public:

        Market(const MarketType market, const string& exchangeBaseUri) : m_connected(false), m_running(false), m_monitorId(1) , m_marketType(market), m_exchangeBaseUri(exchangeBaseUri)
        {
            //m_exchangeBaseUri = market == MarketType::Spot ? SpotWebSockUri : FuturestWebSockUri;
        }


        virtual ~Market()
        {
            disconnect();
        }


        Market(const Market&) = delete;
        Market(Market&&) = delete;  
        Market operator=(const Market&) = delete;



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


            auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + symbol + "@kline_" + interval, keys);

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



    protected:
        void disconnect(const MonitorToken& mt, const bool deleteSession)
        {
            if (auto itIdToSession = m_idToSession.find(mt.id); itIdToSession != m_idToSession.end())
            {
                auto& session = itIdToSession->second;

                session->cancel();

                // calling wait() on a task that's already cancelled throws an exception
                if (!session->receiveTask.is_done())
                    session->receiveTask.wait(); 

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


        /// <summary>
        /// Disconnects all websocket sessions then clears session maps.
        /// </summary>
        void disconnect()
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


        void getJsonValues(const web::json::value& jsonVal, map<string, string>& values, const std::set<string>& keys)
        {
            for (auto& k : keys)
            {
                getJsonValues(jsonVal, values, k);
            }
        }


        void getJsonValues(const web::json::object& jsonObj, map<string, string>& values, const std::set<string>& keys)
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


        void getJsonValues(const web::json::value& jsonVal, map<string, string>& values, const string& key)
        {
            auto keyJsonString = utility::conversions::to_string_t(key);

            if (jsonVal.has_field(keyJsonString))
            {
                string valueString;

                switch (auto t = jsonVal.at(keyJsonString).type(); t)
                {
                    // [[likely]] TODO attribute in C++20
                case json::value::value_type::String:
                    valueString = utility::conversions::to_utf8string(jsonVal.at(keyJsonString).as_string());
                    break;

                case json::value::value_type::Number:
                    valueString = std::to_string(jsonVal.at(keyJsonString).as_number().to_int64());
                    break;

                    // [[unlikely]] TODO attribute in C++20
                case json::value::value_type::Boolean:
                    valueString = jsonVal.at(keyJsonString).as_bool() ? utility::conversions::to_utf8string("true") : utility::conversions::to_utf8string("false");
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


        inline int gettimeofday(struct timeval* tp, struct timezone* tzp)
        {
            namespace sc = std::chrono;
            sc::system_clock::duration d = sc::system_clock::now().time_since_epoch();
            sc::seconds s = sc::duration_cast<sc::seconds>(d);
            tp->tv_sec = s.count();
            tp->tv_usec = sc::duration_cast<sc::microseconds>(d - s).count();

            return 0;
        }


        unsigned long get_current_ms_epoch()
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            return tv.tv_sec * 1000 + tv.tv_usec / 1000;
        }


        string b2a_hex(char* byte_arr, int n)
        {
            const static std::string HexCodes = "0123456789abcdef";
            string HexString;
            for (int i = 0; i < n; ++i)
            {
                unsigned char BinValue = byte_arr[i];
                HexString += HexCodes[(BinValue >> 4) & 0x0F];
                HexString += HexCodes[BinValue & 0x0F];
            }
            return HexString;
        }


        string createSignature(const string& key, const string& data)
        {
            // TODO check this for memory not being released

            auto& dataString = utility::conversions::to_utf8string(data);

            unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), key.size(), (unsigned char*)dataString.c_str(), dataString.size(), NULL, NULL);
            return b2a_hex((char*)digest, 32);
        }


        // user data stream
        bool createListenKey(const MarketType marketType)
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
                {
                    std::stringstream query;
                    query << "timestamp=" << get_current_ms_epoch() << "&recvWindow=5000";
                                                            
                    queryString = query.str() + "&signature=" + createSignature(m_secretKey, query.str());
                }
                
                
                // build the request, with appropriate headers, the API key and the query string with the signature appended
                web::uri requstUri(utility::conversions::to_string_t(path + "?" + queryString));
                
                web::http::http_request request { web::http::methods::POST };
                request.headers().add(utility::conversions::to_string_t("Content-Type"), utility::conversions::to_string_t("application/json"));
                request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiKey));
                request.headers().add(utility::conversions::to_string_t("client_SDK_Version"), utility::conversions::to_string_t("binancews_cpp_alpha"));
                request.set_request_uri(requstUri);
                

                // send HTTP POST
                web::http::client::http_client client{ web::uri{utility::conversions::to_string_t(uri)} };

                client.request(request).then([&ok, this](web::http::http_response response)
                {
                    if (response.status_code() == web::http::status_codes::OK)
                    {
                        ok = true;
                        m_listenKey = utility::conversions::to_utf8string(response.extract_json().get()[utility::conversions::to_string_t(ListenKeyName)].as_string());
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


    protected:
        shared_ptr<WebSocketSession> m_session;
        MarketType m_marketType;
    
        vector<shared_ptr<WebSocketSession>> m_sessions;
        map<size_t, shared_ptr<WebSocketSession>> m_idToSession;

        std::atomic_size_t m_monitorId;
        string m_exchangeBaseUri;
        std::atomic_bool m_connected;
        std::atomic_bool m_running;
        string m_apiKey;
        string m_listenKey;
        string m_secretKey;


        IntervalTimer m_userDataStreamTimer;
    };


    /// <summary>
    /// Access the Spot Market.
    /// </summary>
    class SpotMarket : public Market
    {
    public:
        SpotMarket() : Market(MarketType::Spot, SpotWebSockUri)
        {

        }


        virtual ~SpotMarket()
        {
            disconnect();
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
        /// 
        /// </summary>
        /// <param name="apiKey"></param>
        /// <param name="onData"></param>
        /// <param name="mode"></param>
        /// <returns></returns>
        MonitorToken monitorUserData(const string& apiKey, const string& secretKey, std::function<void(SpotUserData)> onData)
        {
            m_apiKey = apiKey;
            m_secretKey = secretKey;

            MonitorToken monitorToken;

            if (createListenKey(MarketType::Spot))
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
                    catch (...)
                    {

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

                                                userData.balances[values["a"]] = std::move(values);
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
                        }
                        catch (...)
                        {

                        }
                    }
                    else
                    {
                        pplx::cancel_current_task();
                    }

                }, session->getCancelToken()).wait();
            }

            pplx::cancel_current_task();
        }
    };




    /// <summary>
    /// Access the USD-M Future's market. You must have a Futures account.
    /// The APis keys must be enabled for Futures in the API Management settings. 
    /// If you created the API key before you created your Futures account, you must create a new API key.
    /// </summary>
    class UsdFuturesMarket : public Market
    {
    protected:
        UsdFuturesMarket(MarketType mt, const string& exchangeUri) : Market(mt, exchangeUri)
        {
        }

    public:
        UsdFuturesMarket() : UsdFuturesMarket(MarketType::Futures, FuturestWebSockUri)
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
        /// 
        /// </summary>
        /// <param name="apiKey"></param>
        /// <param name="onData"></param>
        /// <param name="mode"></param>
        /// <returns></returns>
        MonitorToken monitorUserData(const string& apiKey, const string& secretKey, std::function<void(UsdFutureUserData)> onData)
        {
            m_apiKey = apiKey;
            m_secretKey = secretKey;

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
            request.headers().add(utility::conversions::to_string_t("Content-Type"), utility::conversions::to_string_t("application/json"));
            request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiKey));
            request.headers().add(utility::conversions::to_string_t("client_SDK_Version"), utility::conversions::to_string_t("binancews_cpp_alpha"));
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
                                    extractUsdFuturesUserData(session, web::json::value::parse(strMsg));
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
        UsdFuturesTestMarket() : UsdFuturesMarket(MarketType::FuturesTest, TestFuturestWebSockUri)
        {

        }

        virtual ~UsdFuturesTestMarket()
        {
            disconnect();
        }

        
    };
}