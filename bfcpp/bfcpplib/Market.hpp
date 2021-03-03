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


namespace bfcpp
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
  /// Provides an API to the Binance futures exchange.
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
    enum class RestCall { NewOrder, ListenKey, CancelOrder, AllOrders, AccountInfo };


  public:
    typedef std::string MarketStringType;

    enum class MarketType { Futures, FuturesTest };


  protected:
    inline static const map<RestCall, string> PathMap =
    {
        {RestCall::NewOrder,     "/fapi/v1/order"},
        {RestCall::ListenKey,    "/fapi/v1/listenKey"},
        {RestCall::CancelOrder,  "/fapi/v1/order"},
        {RestCall::AllOrders,    "/fapi/v1/allOrders"},
        {RestCall::AccountInfo,  "/fapi/v2/account"}
    };


    // default receive windows. NOTE: the user can change there at runtime with setReceiveWindow()
    inline static map<RestCall, string> ReceiveWindowMap =
    {
        {RestCall::NewOrder,    "5000"},
        {RestCall::ListenKey,   "5000"},    // no affect for RestCall::ListenKey, here for completion
        {RestCall::CancelOrder, "5000"},
        {RestCall::AllOrders,   "5000"},
        {RestCall::AccountInfo, "5000"}
    };


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


  public:
    const string FuturestWebSockUri = "wss://fstream.binance.com";
    const string TestFuturestWebSockUri = "wss://stream.binancefuture.com";

    const string UsdFuturesRestUri = "https://fapi.binance.com";
    const string TestUsdFuturestRestUri = "https://testnet.binancefuture.com";

    const string HeaderApiKeyName = "X-MBX-APIKEY";
    const string ListenKeyName = "listenKey";
    const string ClientSDKVersionName = "client_SDK_Version";
    const string ContentTypeName = "Content-Type";



    typedef map<string, set<string>> JsonKeys;

    typedef std::chrono::system_clock Clock;

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



    AccountInformation accountInformation();



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
      NewOrderResult result;

      string queryString{ createQueryString(std::move(order), RestCall::NewOrder, true) };

      try
      {
        auto request = createHttpRequest(web::http::methods::POST, getApiPath(RestCall::NewOrder) + "?" + queryString);

        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri()) } };
        client.request(std::move(request)).then([this, &result](web::http::http_response response) mutable
          {
            auto json = response.extract_json().get();

            if (response.status_code() == web::http::status_codes::OK)
            {
              getJsonValues(json, result.response, set<string> { "clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "avgPrice", "origQty", "price", "reduceOnly", "side", "positionSide", "status",
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


    /// <summary>
    /// Returns all orders. What is returned is dependent on the status and order time, read:
    /// https://binance-docs.github.io/apidocs/futures/en/#all-orders-user_data
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    AllOrdersResult allOrders(map<string, string>&& query)
    {
      AllOrdersResult result;

      string queryString{ createQueryString(std::move(query), RestCall::AllOrders, true) };

      try
      {
        auto request = createHttpRequest(web::http::methods::GET, getApiPath(RestCall::AllOrders) + "?" + queryString);

        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri()) } };
        client.request(std::move(request)).then([this, &result](web::http::http_response response) mutable
          {
            auto json = response.extract_json().get();

            if (response.status_code() == web::http::status_codes::OK)
            {
              auto& jsonArray = json.as_array();

              for (const auto& entry : jsonArray)
              {
                map<string, string> order;
                getJsonValues(entry, order, set<string> { "avgPrice", "clientOrderId", "cumQuote", "executedQty", "orderId", "origQty", "origType", "price", "reduceOnly", "side", "positionSide", "status",
                  "stopPrice", "closePosition", "symbol", "time", "timeInForce", "type", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

                result.response.emplace_back(std::move(order));
              }
            }
            else
            {
              throw std::runtime_error{ "Binance returned error in allOrders():\n " + utility::conversions::to_utf8string(json.serialize()) };
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
    /// Close stream for the given token.
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
    /// Sets the receive window. For defaults see member ReceiveWindowMap.
    /// Read about receive window in the "Timing Security" section at: https://binance-docs.github.io/apidocs/futures/en/#endpoint-security-type
    /// Note the receive window for RestCall::ListenKey has no affect
    /// </summary>
    /// <param name="call">The call for which this will set the time</param>
    /// <param name="ms">time in milliseconds</param>
    void setReceiveWindow(const RestCall call, std::chrono::milliseconds ms)
    {
      ReceiveWindowMap[call] = std::to_string(ms.count());
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

    /// <summary>
    /// Get a Binance API timestamp for now.
    /// </summary>
    /// <returns></returns>
    static auto getTimestamp() -> Clock::duration::rep
    {
      return getTimestamp(Clock::now());
    }

    /// <summary>
    /// Get a Binance API timestamp for the given time.
    /// </summary>
    /// <returns></returns>
    static auto getTimestamp(Clock::time_point t) -> Clock::duration::rep
    {
      return std::chrono::duration_cast<std::chrono::milliseconds> (t.time_since_epoch()).count();
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


    MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey);


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


    web::http::http_request createHttpRequest(const web::http::method method, string uri)
    {
      web::http::http_request request{ method };
      request.headers().add(utility::conversions::to_string_t(HeaderApiKeyName), utility::conversions::to_string_t(m_apiAccess.apiKey));
      request.headers().add(utility::conversions::to_string_t(ContentTypeName), utility::conversions::to_string_t("application/json"));
      request.headers().add(utility::conversions::to_string_t(ClientSDKVersionName), utility::conversions::to_string_t("binance_futures_cpp"));
      request.set_request_uri(web::uri{ utility::conversions::to_string_t(uri) });
      return request;
    }


    /// <summary>
    /// Notice, this function taken from BinaCPP
    /// </summary>
    /// <param name="byte_arr"></param>
    /// <param name="n"></param>
    /// <returns></returns>
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


    /// <summary>
    /// Notice, this function taken from BinaCPP
    /// </summary>
    /// <param name="byte_arr"></param>
    /// <param name="n"></param>
    /// <returns></returns>
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


    bool createListenKey(const MarketType marketType);


    string createQueryString(map<string, string>&& queryValues, const RestCall call, const bool sign)
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
        ss << "recvWindow=" << ReceiveWindowMap.at(call) << "&timestamp=" << ts;

        string qs = ss.str();
        return qs + "&signature=" + createSignature(m_apiAccess.secretKey, qs);
      }
      else
      {
        return ss.str();
      }
    }



    // Utils

    string getApiUri()
    {
      switch (m_marketType)
      {
      case MarketType::Futures:
        return UsdFuturesRestUri;
        break;

      case MarketType::FuturesTest:
        return TestUsdFuturestRestUri;
        break;

      default:
        throw std::runtime_error("Unknown market type");
        break;
      }
    }


    string getApiPath(const RestCall call)
    {
      switch (m_marketType)
      {
      case MarketType::Futures:
      case MarketType::FuturesTest:
        return PathMap.at(call);
        break;

      default:
        throw std::runtime_error("Unknown market type");
        break;
      }
    }


    string strToLower(const std::string& str)
    {
      string lower;
      lower.resize(str.size());

      std::transform(str.cbegin(), str.cend(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
      return lower;
    }


    string strToLower(std::string&& str)
    {
      string lower{ str };

      std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
      return lower;
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
