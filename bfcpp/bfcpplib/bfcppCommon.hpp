#ifndef __BINANCE_COMMON_HPP 
#define __BINANCE_COMMON_HPP

#include <functional>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <map>
#include <set>
#include <future>
#include <chrono>
#include <sstream>
#include <string>
#include <any>
#include <cpprest/json.h>
#include <cpprest/ws_client.h>
#include <cpprest/http_client.h>
#include <openssl/hmac.h>



namespace bfcpp
{
  using std::shared_ptr;
  using std::vector;
  using std::string;
  using std::map;
  using std::future;
  using std::pair;
  using std::set;
  using std::stringstream;

  namespace fs = std::filesystem;
  namespace ws = web::websockets;
  namespace json = web::json;


  typedef size_t MonitorTokenId;
  typedef std::chrono::system_clock Clock;
  typedef map<string, set<string>> JsonKeys;
  typedef std::string MarketStringType;



  #define BFCPP_FUNCTION std::string {__func__}
  #define BFCPP_FUNCTION_MSG(msg) std::string {__func__} + msg


  enum class RestCall
  {
    None,
    NewOrder,
    ListenKey,
    CancelOrder,
    AllOrders,
    AccountInfo,
    AccountBalance,
    TakerBuySellVolume,
    KlineCandles,
    Ping,
    NewBatchOrder,
    ExchangeInfo
  };


  enum class StreamCall
  {
    None,
    Candlesticks,
    MarkPrice,
    SymbolMiniTicker,
    SymbolBookTicker,
    AllMarketMiniTicker
  };
  
  enum class MarketType
  {
    Futures,
    FuturesTest
  };

  enum class OrderStatus
  {
    None,
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected,
    Expired
  };


  const string FuturestWebSockUri = "wss://fstream.binance.com";
  const string TestFuturestWebSockUri = "wss://stream.binancefuture.com";

  const string UsdFuturesRestUri = "https://fapi.binance.com";
  const string TestUsdFuturestRestUri = "https://testnet.binancefuture.com";

  const string HeaderApiKeyName = "X-MBX-APIKEY";
  const string ListenKeyName = "listenKey";
  const string ClientSDKVersionName = "client_SDK_Version";
  const string ContentTypeName = "Content-Type";


  inline static const map<string, OrderStatus> OrderStatusMap =
  {
    {"None", OrderStatus::None},
    {"NEW", OrderStatus::New},
    {"PARTIALLY_FILLED", OrderStatus::PartiallyFilled},
    {"FILLED", OrderStatus::Filled},
    {"CANCELED", OrderStatus::Cancelled},
    {"REJECTED", OrderStatus::Rejected},
    {"EXPIRED", OrderStatus::Expired}
  };


  inline static const map<RestCall, string> PathMap =
  {
      {RestCall::NewOrder,     "/fapi/v1/order"},
      {RestCall::ListenKey,    "/fapi/v1/listenKey"},
      {RestCall::CancelOrder,  "/fapi/v1/order"},
      {RestCall::AllOrders,    "/fapi/v1/allOrders"},
      {RestCall::AccountInfo,  "/fapi/v2/account"},
      {RestCall::AccountBalance, "/fapi/v2/balance"},
      {RestCall::TakerBuySellVolume, "/futures/data/takerlongshortRatio"},
      {RestCall::KlineCandles, "/fapi/v1/klines"},
      {RestCall::Ping, "/fapi/v1/ping"},
      {RestCall::NewBatchOrder, "/fapi/v1/batchOrders"},
      {RestCall::ExchangeInfo, "/fapi/v1/exchangeInfo"}
  };


  /// <summary>
  /// Struct used in some of the monitor functions to store a direct key/value pair.
  /// </summary>
  
  /*
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
  /// 
  /// NOTE: when handling price data, such as markPrice, you should sort the inner map by 'E', the event time.
  /// 
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
  */


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
    enum class EventType { Unknown, MarginCall, OrderUpdate, AccountUpdate, DataStreamExpired };

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


  struct RestResult
  {
    bool valid() const { return m_valid; }
    
    void valid(const bool v, string&& msg = {})
    {
      m_valid = v;
      m_msg.assign(std::move(msg));
    }

    const string& msg() const { return m_msg; }


  protected:
    RestResult(RestCall rc, bool valid = true) : m_rc(rc), m_valid{ valid } {}
    virtual ~RestResult(){}


    RestCall m_rc;
    bool m_valid;
    string m_msg;
  };



  template<class RestResultT>
  inline RestResultT createInvalidRestResult(string&& msg)
  {
    RestResultT r{};
    r.valid(false, std::move(msg));
    return r;
  }


  /// <summary>
  /// Returned by newOrder(). 
  /// The key/values in response are here: https://binance-docs.github.io/apidocs/testnet/en/#new-order-trade
  /// </summary>
  struct NewOrderResult : public RestResult
  {
    NewOrderResult() : RestResult(RestCall::NewOrder) {}

    NewOrderResult(map<string, string>&& data) : RestResult(RestCall::NewOrder), response(data)
    {
    }

    map<string, string> response;
  };


  struct NewOrderBatchResult : public RestResult
  {
    NewOrderBatchResult() : RestResult(RestCall::NewBatchOrder) {}

    NewOrderBatchResult(vector<map<string, string>>&& data) : RestResult(RestCall::NewBatchOrder), response(data)
    {
    }

    vector<map<string, string>> response;
  };


  struct NewOrderPerformanceResult : public NewOrderResult
  {
    std::chrono::high_resolution_clock::duration restApiCall;
    std::chrono::high_resolution_clock::duration restQueryBuild;
    std::chrono::high_resolution_clock::duration restResponseHandler;
    std::chrono::high_resolution_clock::duration bfcppTotalProcess;
    std::chrono::high_resolution_clock::duration total ;
  };

  /// <summary>
  /// See https://binance-docs.github.io/apidocs/futures/en/#cancel-order-trade
  /// </summary>
  struct CancelOrderResult : public RestResult
  {
    CancelOrderResult() : RestResult(RestCall::CancelOrder) {}

    CancelOrderResult(map<string, string>&& data) :RestResult(RestCall::CancelOrder), response(data)
    {

    }

    map<string, string> response;
  };


  /// <summary>
  /// See https://binance-docs.github.io/apidocs/futures/en/#all-orders-user_data
  /// </summary>
  struct AllOrdersResult : public RestResult
  {
    AllOrdersResult() : RestResult(RestCall::AllOrders) {}

    vector<map<string, string>> response;
  };


  /// <summary>
  /// See https://binance-docs.github.io/apidocs/futures/en/#account-information-v2-user_data
  /// </summary>
  struct AccountInformation : public RestResult
  {
    AccountInformation() : RestResult(RestCall::AccountInfo) {}

    map<string, string> data;
    vector<map<string, string>> assets;
    vector<map<string, string>> positions;
  };


  /// <summary>
  /// See https://binance-docs.github.io/apidocs/futures/en/#futures-account-balance-v2-user_data
  /// </summary>
  struct AccountBalance : public RestResult
  {
    AccountBalance() : RestResult(RestCall::AccountBalance) {}

    vector<map<string, string>> balances;
  };

  
  /// <summary>
  /// See https://binance-docs.github.io/apidocs/futures/en/#long-short-ratio
  /// </summary>
  struct TakerBuySellVolume : public RestResult
  {
    TakerBuySellVolume() : RestResult(RestCall::TakerBuySellVolume) {}

    vector<map<string, string>> response;
  };


  /// <summary>
  /// See https://binance-docs.github.io/apidocs/futures/en/#kline-candlestick-data
  /// </summary>
  struct KlineCandlestick : public RestResult
  {
    KlineCandlestick() : RestResult(RestCall::KlineCandles) {}

    vector<vector<string>> response;
  };


  struct ListenKey : public RestResult
  {
    ListenKey() : RestResult(RestCall::ListenKey) {}

    string listenKey;
  };


  struct ExchangeInfo : public RestResult
  {
    ExchangeInfo() : RestResult(RestCall::ExchangeInfo) {}

    struct Symbol
    {
      map<string, string> data; // top level key/value pairs with in teh symbol, i.e. "status", "pricePrecision", etc
      vector<map<string, string>> filters;
      vector<string> orderTypes;
      vector<string> timeInForce;
      vector<string> underlyingSubType;
    };

    string timezone;
    string serverTime;
    vector<map<string, string>> exchangeFilters;
    vector<map<string, string>> rateLimits;
    vector<ExchangeInfo::Symbol> symbols;
  };




  // Data used in Monitor callbacks

  struct StreamCallbackData
  {
    StreamCallbackData(const StreamCall sc) : call(sc)
    {

    }

    StreamCall call;
  };

  struct CandleStream : public StreamCallbackData
  {
    CandleStream() : StreamCallbackData(StreamCall::Candlesticks)
    {
    }

    string eventTime;
    string symbol;
    map<string, string> candle;
  };

  struct MarkPriceStream : public StreamCallbackData
  {
    MarkPriceStream() : StreamCallbackData(StreamCall::MarkPrice)
    {
    }
    
    vector<map<string, string>> prices;
  };


  struct SymbolMiniTickerStream : public StreamCallbackData
  {
    SymbolMiniTickerStream() : StreamCallbackData(StreamCall::SymbolMiniTicker)
    {
    }

    map<string, string> data;
  };


  struct SymbolBookTickerStream : public StreamCallbackData
  {
    SymbolBookTickerStream() : StreamCallbackData(StreamCall::SymbolBookTicker)
    {
    }

    map<string, string> data;
  };


  struct AllMarketMiniTickerStream : public StreamCallbackData
  {
    AllMarketMiniTickerStream() : StreamCallbackData(StreamCall::AllMarketMiniTicker)
    {
    }

    vector<map<string, string>> data;
  };


  /// <summary>
  /// Returned by monitor functions, containing an ID for use with cancelMonitor() to close this stream.
  /// </summary>
  struct MonitorToken
  {
    MonitorToken() : id (0) {}
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


  class BfcppException : public std::runtime_error
  {
  public:
    BfcppException(const string& msg) : runtime_error(msg)
    {

    }

    BfcppException(string&& msg) : runtime_error(msg)
    {

    }
  };

  class BfcppDisconnectException : public std::runtime_error
  {
  public:
    BfcppDisconnectException(const string& source) : runtime_error("Disconnect: "+source), m_source(source)
    {
      
    }

    const string& source() const { return m_source;  }

  private:
    string m_source;
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

    WebSocketSession(WebSocketSession&& other) = default;


    // end point
    string uri;

    // client for the websocket
    ws::client::websocket_client client;
    // the task which receives the websocket messages
    pplx::task<void> receiveTask;

    // callback functions for user functions
    std::function<void(std::any)>  callback;

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




  template <typename T>
  string toString(const T a_value, const int n = 6)
  {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
  }


  inline string jsonValueToString(const web::json::value& jsonVal)
  {
    switch (auto t = jsonVal.type(); t)
    {
      // [[likely]] TODO attribute in C++20
    case json::value::value_type::String:
      return utility::conversions::to_utf8string(jsonVal.as_string());
      break;

    case json::value::value_type::Number:
      return std::to_string(jsonVal.as_number().to_int64());
      break;

      // [[unlikely]] TODO attribute in C++20
    case json::value::value_type::Boolean:
      return jsonVal.as_bool() ? utility::conversions::to_utf8string("true") : utility::conversions::to_utf8string("false");
      break;

    default:
      throw std::runtime_error("No handler for JSON type: " + std::to_string(static_cast<int>(t)));
      break;
    }
  }


  inline void getJsonValues(const web::json::value& jsonVal, map<string, string>& values, const string& key)
  {
    auto keyJsonString = utility::conversions::to_string_t(key);

    if (jsonVal.has_field(keyJsonString))
    {
      values[key] = jsonValueToString(jsonVal.at(keyJsonString));
    }
  }



  inline void getJsonValues(const web::json::object& jsonObj, map<string, string>& values, const set<string>& keys)
  {
    for (const auto& v : jsonObj)
    {
      auto keyUtf8String = utility::conversions::to_utf8string(v.first);

      if (keys.find(utility::conversions::to_utf8string(v.first)) != keys.cend())
      {
        auto& keyJsonString = utility::conversions::to_string_t(v.first);

        values[keyUtf8String] = jsonValueToString(v.second);
      }
    }
  }


  inline void getJsonValues(const web::json::value& jsonVal, map<string, string>& values, const set<string>& keys)
  {
    for (const auto& k : keys)
    {
      getJsonValues(jsonVal, values, k);
    }
  }



  inline string getApiUri(const MarketType mt)
  {
    switch (mt)
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


  inline string getApiPath(const MarketType mt, const RestCall call)
  {
    switch (mt)
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


  inline string strToLower(const std::string& str)
  {
    string lower;
    lower.resize(str.size());

    std::transform(str.cbegin(), str.cend(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower;
  }


  inline string strToLower(std::string&& str)
  {
    string lower{ std::move(str) };

    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower;
  }



  /// <summary>
  /// Notice, this function taken from BinaCPP
  /// </summary>
  /// <param name="byte_arr"></param>
  /// <param name="n"></param>
  /// <returns></returns>
  inline string b2a_hex(char* byte_arr, int n)
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
  inline string createSignature(const string& key, const string& data)
  {
    string hash;

    auto& dataString = utility::conversions::to_utf8string(data);

    if (unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.size()), (unsigned char*)dataString.c_str(), dataString.size(), NULL, NULL); digest)
    {
      hash = b2a_hex((char*)digest, 32);
    }

    return hash;
  }


  /// <summary>
  /// Ensure price is in a suitable format for the exchange, i.e. changing precision.
  /// You can get the precision for a symbol from exchangeInfo.
  /// </summary>
  /// <param name="price">The unformatted price</param>
  /// <param name="precision">The precision</param>
  /// <returns>The price in a suitable format</returns>
  inline string priceTransform(const string& price, const std::streamsize precision = 2)
  {
    string p;

    stringstream ss;
    ss.precision(precision);
    ss << std::fixed << std::stod(price);
    ss >> p;

    return p;
  }


  /// <summary>
  /// Get a Binance API timestamp for the given time.
  /// </summary>
  /// <returns></returns>
  inline auto getTimestamp(Clock::time_point t) -> Clock::duration::rep
  {
    return std::chrono::duration_cast<std::chrono::milliseconds> (t.time_since_epoch()).count();
  }


  /// <summary>
  /// Get a Binance API timestamp for now.
  /// </summary>
  /// <returns></returns>
  inline auto getTimestamp() -> Clock::duration::rep
  {
    return getTimestamp(Clock::now());
  }

}

#endif
