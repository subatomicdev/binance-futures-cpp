#ifndef __BINANCE_MARKET_HPP 
#define __BINANCE_MARKET_HPP


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


namespace binancews
{
    namespace ws = web::websockets;
    namespace json = web::json;

    using std::string;
    using std::vector;
    using std::shared_ptr;
    using std::map;
    using std::set;
    using std::stringstream;


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
        typedef std::string MarketStringType;

        enum class MarketType { Spot, Futures, FuturesTest, SpotTest };

        enum class RestCall { NewOrder, ListenKey, CancelOrder };

        inline static const map<RestCall, string> FuturesCallPathMap =  { {RestCall::NewOrder, "/fapi/v1/order"}, {RestCall::ListenKey, "/fapi/v1/listenKey"}, {RestCall::CancelOrder, "/fapi/v1/order"} };
        inline static const map<RestCall, string> SpotCallPathMap =     { {RestCall::NewOrder, "/api/v3/order"}, {RestCall::ListenKey, "/api/v3/userDataStream"}, {RestCall::CancelOrder, "/api/v3/order"} };


        typedef size_t MonitorTokenId;


        /// <summary>
        /// Struct used in some of the monitor functions to store a direct key/value pair.
        /// </summary>
        struct BinanceKeyValueData
        {
            BinanceKeyValueData() = default;

            BinanceKeyValueData(map<string, string>&& vals) : values(vals)
            {

            }

            map<string, string> values;
        };


        /// <summary>
        /// Used in the monitor functions where a simple key/value pair is not suitable.
        /// The top level is typically the symbol, with the value being that symbol's relevant data, i.e.:
        /// 
        ///     ["ZENUSDT", ["E", "1613317084088"], ["c", "50.54400000"], ["e", "2hrMiniTicker"], ... etc]
        /// 
        /// </summary>
        struct BinanceKeyMultiValueData
        {
            BinanceKeyMultiValueData() = default;

            BinanceKeyMultiValueData(map<string, map<string, string>>&& vals) : values(std::move(vals))
            {

            }

            map<string, map<string, string>> values;
        };


        /// <summary>
        /// Contains data from the SpotUser user data stream.
        /// Data contains key/value pairs as described on https://binance-docs.github.io/apidocs/spot/en/#user-data-streams.
        /// If type is EventType::AccountUpdate then au.balances can be populated.
        /// </summary>
        struct SpotUserData
        {
            enum class EventType { Unknown, AccountUpdate, BalanceUpdate, OrderUpdate };

            SpotUserData() = delete;

            SpotUserData(const EventType t) : type(t)
            {

            }

            struct AccountUpdate
            {
                map<string, map<string, string>> balances; // only for when type is AccountUpdate
            } au;

            map<string, string> data;   // key/value 
            EventType type;
        };


        /// <summary>
        /// Contains data from the USD-M Futures stream.
        /// 
        /// First check the type which determines member is populated:
        ///  EventType::MarginCall - mc
        ///  EventType::OrderUpdate - ou
        ///  EventType::AccountUpdate - au
        /// 
        /// See https://binance-docs.github.io/apidocs/futures/en/#user-data-streams
        /// </summary>
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
                map<string, string> data;
                string reason;
                vector<map<string, string>> balances;
                vector<map<string, string>> positions;
            } au;

            EventType type;
        };


        /// <summary>
        /// Returned by newOrder(). 
        /// The key/values in 'result' are here: https://binance-docs.github.io/apidocs/testnet/en/#new-order-trade
        /// </summary>
        struct NewOrderResult
        {
            NewOrderResult() = default;
            NewOrderResult(NewOrderResult&&) noexcept = default;
            NewOrderResult(const NewOrderResult&) = default;
            NewOrderResult& operator=(const NewOrderResult&) = default;
            NewOrderResult& operator=(NewOrderResult&&) = default;


            NewOrderResult(map<string, string>&& data) : result(data)
            {
            }

            map<string, string> result;
        };



        struct CancelOrderResult
        {
            CancelOrderResult() = default;
            CancelOrderResult(CancelOrderResult&&) noexcept = default;
            CancelOrderResult(const CancelOrderResult&) = default;
            CancelOrderResult& operator=(const CancelOrderResult&) = default;
            CancelOrderResult& operator=(CancelOrderResult&&) = default;

            CancelOrderResult(map<string, string>&& data) : result(data)
            {

            }

            map<string, string> result;
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


        /// <summary>
        /// Holds data required for API access. 
        /// You require an API key, but the API is only require for certain features.
        /// </summary>
        struct ApiAccess
        {
            ApiAccess() = default;
            ApiAccess(const string& api, const string& secret = {}) : apiKey(api), secretKey(secret)
            {

            }
            ApiAccess(string&& api, string&& secret = {}) : apiKey(api), secretKey(secret)
            {

            }

            string apiKey;
            string secretKey;
        };



        const string SpotWebSockUri = "wss://stream.binance.com:9443";
        const string TestSpotWebSockUri = "wss://testnet.binance.vision";
        const string FuturestWebSockUri = "wss://fstream.binance.com";
        const string TestFuturestWebSockUri = "wss://stream.binancefuture.com";

        
        const string SpotRestUri = "https://api.binance.com";
        const string TestSpotRestUri = "https://testnet.binance.vision";
        const string UsdFuturesRestUri = "https://fapi.binance.com";
        const string TestUsdFuturestRestUri = "https://testnet.binancefuture.com";


        const string SpotRequestPath = "/api/v3/userDataStream";
        const string UsdFuturesRequestPath = "/fapi/v1/listenKey";

        const string HeaderApiKeyName = "X-MBX-APIKEY";
        const string ListenKeyName = "listenKey";
        const string ClientSDKVersionName = "client_SDK_Version";
        const string ContentTypeName = "Content-Type";
               


        struct WebSocketSession
        {
        private:
            WebSocketSession(const WebSocketSession&) = delete;
            WebSocketSession& operator=(const WebSocketSession&) = delete;


        public:
            WebSocketSession() : connected(false), id(0), cancelToken(cancelTokenSource.get_token())
            {

            }

            WebSocketSession(WebSocketSession&& other) noexcept : uri(std::move(uri)), client(std::move(other.client)), receiveTask(std::move(other.receiveTask)),
                cancelTokenSource(std::move(other.cancelTokenSource)), id(other.id),
                onDataUserCallback(std::move(other.onDataUserCallback)), onMultiValueDataUserCallback(std::move(other.onMultiValueDataUserCallback)),
                cancelToken(std::move(other.cancelToken))
            {
                connected.store(other.connected ? true : false);
            }



            // end point
            string uri;

            // client for the websocket
            ws::client::websocket_client client;
            // the task which receives the websocket messages
            pplx::task<void> receiveTask;

            // callback functions for user functions
            std::function<void(BinanceKeyValueData)> onDataUserCallback;
            std::function<void(BinanceKeyMultiValueData)> onMultiValueDataUserCallback;
            std::function<void(SpotUserData)> onSpotUserDataCallback;
            std::function<void(UsdFutureUserData)> onUsdFuturesUserDataCallback;

            // the monitor id. The MonitorToken is returned to the caller which can be used to cancel the monitor
            MonitorTokenId id;
            std::atomic_bool connected;


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

        Market(const MarketType market, const string& exchangeBaseUri, const ApiAccess& access = {});

        virtual ~Market();


        Market(const Market&) = delete;
        Market(Market&&) = delete;
        Market& operator=(const Market&) = delete;



        // --- monitor functions


        /// <summary>
        /// Receives from the miniTicker stream for all symbols
        /// Updates every 1000ms (limited by the Binance API).
        /// </summary>
        /// <param name="onData">Your callback function. See this classes docs for an explanation</param>
        /// <returns>A MonitorToken. If MonitorToken::isValid() is a problem occured.</returns>
        MonitorToken monitorMiniTicker(std::function<void(BinanceKeyMultiValueData)> onData);


        /// <summary>
        /// Receives from the 
        /// </summary>
        /// <param name="symbol"></param>
        /// <param name="onData"></param>
        /// <returns></returns>
        MonitorToken monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(BinanceKeyMultiValueData)> onData);


        /// <summary>
        /// Receives from the symbol mini ticker
        /// Updated every 1000ms (limited by the Binance API).
        /// </summary>
        /// <param name="symbol">The symbtol to monitor</param>
        /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
        /// <returns></returns>
        MonitorToken monitorSymbol(const string& symbol, std::function<void(BinanceKeyValueData)> onData);


        /// <summary>
        /// Receives from the Individual Symbol Book stream for a given symbol.
        /// </summary>
        /// <param name="symbol">The symbol</param>
        /// <param name = "onData">Your callback function.See this classes docs for an explanation< / param>
        /// <returns></returns>
        MonitorToken monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData);



        // --- order management


        /// <summary>
        /// Create a new order. 
        /// 
        /// The NewOrderResult is returned which contains the response from the Rest call,
        /// see https://binance-docs.github.io/apidocs/futures/en/#new-order-trade.
        /// 
        /// If the order is successful, the User Data Stream will be updated.
        /// 
        /// Use the priceTransform() function to make the price value suitable.
        /// </summary>
        /// <param name="order">Order params, see link above.</param>
        /// <returns>See 'response' Rest, see link above.</returns>
        NewOrderResult newOrder(map<string, string>&& order)
        {
            NewOrderResult result {};

            string queryString{ createQueryString(std::move(order), true) };

            web::http::http_request request{ web::http::methods::POST };
            request.set_request_uri(utility::conversions::to_string_t(getApiPath(RestCall::NewOrder) + "?" + queryString));
            request.headers().add(utility::conversions::to_string_t(ContentTypeName), utility::conversions::to_string_t("application/json"));
            request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiAccess.apiKey));
            request.headers().add(utility::conversions::to_string_t(ClientSDKVersionName), utility::conversions::to_string_t("binancews_cpp_alpha"));

            web::uri uri { utility::conversions::to_string_t(getApiUri()) };
            web::http::client::http_client client{ uri };

            try
            {
                client.request(request).then([this, &result](web::http::http_response response) mutable
                {
                    auto json = response.extract_json().get();

                    if (response.status_code() == web::http::status_codes::OK)
                    {
                        getJsonValues(json, result.result, set<string> {"clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "avgPrice", "origQty", "price", "reduceOnly", "side", "positionSide", "status",
                                                                        "stopPrice", "closePosition", "symbol", "timeInForce", "type", "origType", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});
                    }
                    else
                    {
                        throw std::runtime_error{ "Binance returned error in newOrder():\n " + utility::conversions::to_utf8string(json.serialize()) };
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
            
            return result;
        }


        // --- utils


        /// <summary>
        /// Set the API key(s). 
        /// All calls require the API key. You only need secret key set if using a call which requires signing, such as newOrder.
        /// </summary>
        /// <param name="apiKey"></param>
        /// <param name="secretKey"></param>
        void setApiKeys(const ApiAccess access = {})
        {
            m_apiAccess = access;
        }


        
        /// <summary>
        /// Ensure price is in a suitable format for the exchange, i.e. changing precision.
        /// </summary>
        /// <param name="price">The unformatted price</param>
        /// <param name="precision">The precision</param>
        /// <returns>The price in a suitable format</returns>
        static string priceTransform(const string& price, const std::streamsize precision = 2)
        {
            string p;

            stringstream ss;
            ss.precision(precision);
            ss << std::fixed << std::stod(price);
            ss >> p;

            return p;
        }

    protected:

        constexpr bool mustConvertStringT()
        {
            return std::is_same_v<utility::string_t, MarketStringType> == false;
        }


        void disconnect(const MonitorToken& mt, const bool deleteSession);


        /// <summary>
        /// Disconnects all websocket sessions then clears session maps.
        /// </summary>
        void disconnect();


        shared_ptr<WebSocketSession> connect(const string& uri);


        std::tuple<MonitorToken, shared_ptr<WebSocketSession>> createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey = {});


        void extractKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const JsonKeys& keys, const string& arrayKey = {});


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


        MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey);


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
            string hash;

            auto& dataString = utility::conversions::to_utf8string(data);

            if (unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.size()), (unsigned char*)dataString.c_str(), dataString.size(), NULL, NULL); digest)
            {
                hash = b2a_hex((char*)digest, 32);
            }

            return hash;
        }


        // User data stream functions
        auto getTimestamp() -> std::chrono::system_clock::duration::rep
        {            
            return std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count();
        }

        
        bool createListenKey(const MarketType marketType);


        string createQueryString(map<string, string>&& queryValues, const bool sign)
        {            
            stringstream ss;

            // can leave a trailing '&' without borking the internets
            std::for_each(queryValues.begin(), queryValues.end(), [&ss](auto& it)
            {
                ss << std::move(it.first) << "=" << std::move(it.second) << "&";
            });
            
            if (sign)
            {
                auto ts = getTimestamp();
                ss << "recvWindow=5000&timestamp=" << ts;

                string qs = ss.str();
                return qs + "&signature=" + createSignature(m_apiAccess.secretKey, qs);
            }
            else
            {
                return ss.str();
            }
        }


        string getApiUri()
        {
            switch (m_marketType)
            {
            case MarketType::Spot:
                return SpotRestUri;
                break;

            case MarketType::SpotTest:
                return TestSpotRestUri;
                break;

            case MarketType::Futures:
                return UsdFuturesRestUri;
                break;

            case MarketType::FuturesTest:
                return TestUsdFuturestRestUri;
                break;

            default:
                return {};
                break;
            }
        }


        string getApiPath(const RestCall call)
        {
            switch (m_marketType)
            {
            case MarketType::Spot:
            case MarketType::SpotTest:
                return SpotCallPathMap.at(call);
                break;

            case MarketType::Futures:
            case MarketType::FuturesTest:
                return FuturesCallPathMap.at(call);
                break;

            default:
                return {};
                break;
            }
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
        string m_listenKey;
        ApiAccess m_apiAccess;

        IntervalTimer m_userDataStreamTimer;
    };
}

#endif
