#ifndef __BINANCE_COMMON_HPP 
#define __BINANCE_COMMON_HPP

#include <functional>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <future>
#include <chrono>
#include <sstream>
#include <cpprest/ws_client.h>
#include <cpprest/http_client.h>


namespace bfcpp
{
  using std::shared_ptr;
  using std::vector;
  using std::string;
  using std::map;
  using std::future;

  namespace fs = std::filesystem;

  using PGClock = std::chrono::system_clock;

  //

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

    NewOrderResult(map<string, string>&& data) : response(data)
    {
    }

    map<string, string> response;
  };



  struct CancelOrderResult
  {
    CancelOrderResult() = default;

    CancelOrderResult(map<string, string>&& data) : response(data)
    {

    }

    map<string, string> response;
  };



  struct AllOrdersResult
  {
    AllOrdersResult() = default;

    vector<map<string, string>> response;
  };


  struct AccountInformation
  {
    map<string, string> data;
    vector<map<string, string>> assets;
    vector<map<string, string>> positions;
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


  template <typename T>
  string toString(const T a_value, const int n = 6)
  {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
  }
}

#endif
