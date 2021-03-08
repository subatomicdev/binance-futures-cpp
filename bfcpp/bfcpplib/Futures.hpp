#ifndef __BINANCE_FUTURES_HPP 
#define __BINANCE_FUTURES_HPP


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
#include "IntervalTimer.hpp"
#include "bfcppCommon.hpp"


namespace bfcpp
{
  /// <summary>
  /// Access the USD-M Future's market. You must have a Futures account.
  /// The APis keys must be enabled for Futures in the API Management settings. 
  /// If you created the API key before you created your Futures account, you must create a new API key.
  /// </summary>
  class UsdFuturesMarket
  {
    inline const static string DefaultReceiveWindwow = "5000";


  protected:
    UsdFuturesMarket(MarketType mt, const string& exchangeUri, const ApiAccess& access) : m_marketType(mt), m_exchangeBaseUri(exchangeUri), m_apiAccess(access)
    {
      m_monitorId = 1;
    }


  public:
    UsdFuturesMarket(const ApiAccess& access = {}) : UsdFuturesMarket(MarketType::Futures, FuturestWebSockUri, access)
    {

    }


    virtual ~UsdFuturesMarket()
    {
      disconnect();
    }

    string receiveWindow(const RestCall rc)
    {
      if (auto it = m_receiveWindowMap.find(rc); it == m_receiveWindowMap.cend())
        return DefaultReceiveWindwow;
      else
        return it->second;
    }

    /// <summary>
    /// This measures the time it takes to send a "PING" request to the exchange and receive a reply.
    /// It includes near zero processing time by bfcpp, so the returned duration can be assumed to be network latency and Binance's processing time.
    /// Testing has seen this latency range from 300ms to 750ms between calls, whilst an ICMP ping is 18ms.
    /// See https://binance-docs.github.io/apidocs/futures/en/#test-connectivity.
    /// </summary>
    /// <returns>The latency in milliseconds</returns>
    std::chrono::milliseconds ping()
    {
      try
      {
        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri(m_marketType)) } };

        auto request = createHttpRequest(web::http::methods::POST, getApiPath(m_marketType, RestCall::Ping) + "?" + createQueryString({}, RestCall::Ping, false, receiveWindow(RestCall::Ping)));

        auto send = Clock::now();
        auto rcv = client.request(std::move(request)).then([](web::http::http_response response) { return Clock::now(); }).get();

        return std::chrono::duration_cast<std::chrono::milliseconds>(rcv - send);
      }
      catch (const pplx::task_canceled tc)
      {
        throw BfcppDisconnectException("ping");
      }
      catch (const std::exception ex)
      {
        throw BfcppException(ex.what());
      }
    }


    /// <summary>
    /// Futures Only. Receives data from here: https://binance-docs.github.io/apidocs/futures/en/#mark-price-stream-for-all-market
    /// </summary>
    /// <param name = "onData">Your callback function. See this classes docs for an explanation </param>
    /// <returns></returns>
    MonitorToken monitorMarkPrice(std::function<void(BinanceKeyMultiValueData)> onData);


    /// <summary>
    /// Monitor data on the spot market.
    /// </summary>
    /// <param name="apiKey"></param>
    /// <param name="onData"></param>
    /// <param name="mode"></param>
    /// <returns></returns>
    MonitorToken monitorUserData(std::function<void(UsdFutureUserData)> onData);


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




    /// <summary>
    /// See See https://binance-docs.github.io/apidocs/futures/en/#long-short-ratio
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    virtual TakerBuySellVolume takerBuySellVolume(map<string, string>&& query);


    /// <summary>
    /// Becareful with the LIMIT value, it determines the weight of the API call and you want to only handle
    /// the data you require. Default LIMIT is 500.
    /// See https://binance-docs.github.io/apidocs/futures/en/#kline-candlestick-data
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    KlineCandlestick klines(map<string, string>&& query);

    
    
    // --- account/useful/info

    /// <summary>
    /// See https://binance-docs.github.io/apidocs/futures/en/#account-information-v2-user_data
    /// </summary>
    /// <returns></returns>
    AccountInformation accountInformation();


    /// <summary>
    /// See https://binance-docs.github.io/apidocs/futures/en/#futures-account-balance-v2-user_data
    /// </summary>
    /// <returns></returns>
    AccountBalance accountBalance();



    ExchangeInfo exchangeInfo();



    // --- order management


    /// <summary>
    /// Create a new order synchronously. 
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
      return doNewOrder(std::move(order)).get();
    }


    /// <summary>
    /// As newOrder() but async.
    /// </summary>
    /// <param name="order"></param>
    /// <returns>The NewOrderResult in a task.</returns>
    pplx::task<NewOrderResult> newOrderAsync(map<string, string>&& order)
    {
      return doNewOrder(std::move(order));
    }


    /// <summary>
    /// Allows up to a MAX of 5 orders in a single call. 
    /// </summary>
    /// <param name="order">A vector of orders, i.e. a vector of the same map you'd create for newOrder()</param>
    /// <returns></returns>
    NewOrderBatchResult newOrderBatch(vector<map<string, string>>&& order)
    {
      return doNewOrderBatch(std::move(order)).get();
    }


    /// <summary>
    /// As newOrderBatch() but async.
    /// </summary>
    /// <param name="order"></param>
    /// <returns>The NewOrderBatchResult in a task.</returns>
    pplx::task<NewOrderBatchResult> newOrderBatchAsync(vector<map<string, string>>&& order)
    {
      return doNewOrderBatch(std::move(order));
    }


    /// <summary>
    /// Returns all orders. What is returned is dependent on the status and order time, read:
    /// https://binance-docs.github.io/apidocs/futures/en/#all-orders-user_data
    /// </summary>
    /// <param name="query"></param>
    /// <returns></returns>
    AllOrdersResult allOrders(map<string, string>&& query);


    /// <summary>
    /// Sends a cancel order message synchronously.
    /// See https://binance-docs.github.io/apidocs/futures/en/#cancel-order-trade
    /// </summary>
    /// <returns></returns>
    CancelOrderResult cancelOrder(map<string, string>&& order)
    {
      return doCancelOrder(std::move(order)).get();
    }


    /// <summary>
    /// As cancelOrder() but asynchronously.
    /// </summary>
    /// <param name="order"></param>
    /// <returns>The CancelOrderResult in a task.</returns>
    pplx::task<CancelOrderResult> cancelOrderAsync(map<string, string>&& order)
    {
      return doCancelOrder(std::move(order));
    }


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
      m_receiveWindowMap[call] = std::to_string(ms.count());
    }

    MarketType marketType() const { return m_marketType; }

  private:

    constexpr bool mustConvertStringT()
    {
      return std::is_same_v<utility::string_t, MarketStringType> == false;
    }


    pplx::task<NewOrderResult> doNewOrder(map<string, string>&& order)
    {
      try
      {
        auto handler = [](web::http::http_response response)
        {
          NewOrderResult result;

          auto json = response.extract_json().get();

          getJsonValues(json, result.response, set<string> {  "clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "avgPrice", "origQty", "price", "reduceOnly", "side", "positionSide", "status",
                                                              "stopPrice", "closePosition", "symbol", "timeInForce", "type", "origType", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

          return result;
        };

        return sendRestRequest<NewOrderResult>(RestCall::NewOrder, web::http::methods::POST, true, m_marketType, handler, receiveWindow(RestCall::NewOrder), std::move(order));
      }
      catch (const pplx::task_canceled tc)
      {
        throw BfcppDisconnectException("newOrder");
      }
      catch (const std::exception ex)
      {
        throw BfcppException(ex.what());
      }
    }


    pplx::task<CancelOrderResult> doCancelOrder(map<string, string>&& order)
    {
      try
      {
        auto handler = [](web::http::http_response response)
        {
          CancelOrderResult result;

          auto json = response.extract_json().get();
          getJsonValues(json, result.response, set<string> {"clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "origQty", "origType", "price", "reduceOnly", "side", "positionSide",
                                                            "status", "stopPrice", "closePosition", "symbol", "timeInForce", "type", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

          return result;
        };

        return sendRestRequest<CancelOrderResult>(RestCall::CancelOrder, web::http::methods::DEL, true, m_marketType, handler, receiveWindow(RestCall::CancelOrder), std::move(order));
      }
      catch (const pplx::task_canceled tc)
      {
        throw BfcppDisconnectException("cancelOrder");
      }
      catch (const std::exception ex)
      {
        throw BfcppException(ex.what());
      }
    }


    pplx::task<NewOrderBatchResult> doNewOrderBatch(vector<map<string, string>>&& orders)
    {
      try
      {
        auto handler = [](web::http::http_response response)
        {
          NewOrderBatchResult result;

          auto json = response.extract_json().get();

          for (auto& order : json.as_array())
          {
            map<string, string> orderValues;
            getJsonValues(order, orderValues, set<string> {  "clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "avgPrice", "origQty", "price", "reduceOnly", "side", "positionSide", "status",
                                                              "stopPrice", "closePosition", "symbol", "timeInForce", "type", "origType", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

            result.response.emplace_back(std::move(orderValues));
          }

          return result;
        };


        // convert the vector of orders to single JSON string and create the query string from that
        const static map<string, web::json::value::value_type> NonStringTypes = { {"orderId", web::json::value::Number}, {"reduceOnly", web::json::value::Boolean},
                                                                                  {"updateTime", web::json::value::Number}, {"priceProtect", web::json::value::Boolean}
                                                                                };

        web::json::value list = web::json::value::array();
        size_t i = 0;

        for (auto& order : orders)
        {
          auto entry = list.object();
          
          for (auto& pair : order)
          {
            auto key = utility::conversions::to_string_t(pair.first);

            if (auto typeEntry = NonStringTypes.find(pair.first); typeEntry == NonStringTypes.end())
            {
              entry[key] = web::json::value::string(utility::conversions::to_string_t(pair.second));
            }
            else
            {
              if (typeEntry->second == web::json::value::Number)
              {
                entry[key] = web::json::value::number(static_cast<int64_t>(std::stoll(utility::conversions::to_string_t(pair.second)))); // TODO confirm long long correct
              }
              else if (typeEntry->second == web::json::value::Boolean)
              {
                entry[key] = web::json::value::boolean(pair.second == "true" || pair.second == "TRUE");
              }
            }
          }

          list[i++] = std::move(entry);
        }

        map<string, string> query;
        query["batchOrders"] = utility::conversions::to_utf8string(web::http::uri::encode_data_string(list.serialize()));

        return sendRestRequest<NewOrderBatchResult>(RestCall::NewBatchOrder, web::http::methods::POST, true, m_marketType, handler, receiveWindow(RestCall::NewBatchOrder), std::move(query));
      }
      catch (const pplx::task_canceled tc)
      {
        throw BfcppDisconnectException("doNewOrderBatch");
      }
      catch (const std::exception ex)
      {
        throw BfcppException(ex.what());
      }
    }


    void onUserDataTimer()
    {
      auto request = createHttpRequest(web::http::methods::PUT, getApiPath(m_marketType, RestCall::ListenKey));

      web::http::client::http_client client{ web::uri{utility::conversions::to_string_t(getApiUri(m_marketType))} };
      client.request(std::move(request)).then([this](web::http::http_response response)
      {
        if (response.status_code() != web::http::status_codes::OK)
        {
          throw BfcppException("ERROR : keepalive for listen key failed");
        }
      }).wait();
    }


    void handleUserDataStream(shared_ptr<WebSocketSession> session, std::function<void(UsdFutureUserData)> onData)
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
                  std::error_code errCode;
                  if (auto json = web::json::value::parse(strMsg, errCode); errCode.value() == 0)
                  {
                    extractUsdFuturesUserData(session, std::move(json));
                  }
                  else
                  {
                    throw BfcppException("Invalid json: " + strMsg); // TODO should this be an exception or just ignore?
                  }
                }
              }
              catch (pplx::task_canceled tc)
              {
                throw BfcppDisconnectException(session->uri);
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


    void extractUsdFuturesUserData(shared_ptr<WebSocketSession> session, web::json::value&& jsonVal);
  

    shared_ptr<WebSocketSession> connect(const string& uri);
    
    void disconnect(const MonitorToken& mt, const bool deleteSession);
    void disconnect();


    std::tuple<MonitorToken, shared_ptr<WebSocketSession>> createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey = {});


    bool createListenKey(const MarketType marketType);

    
    MonitorToken createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey);


    void extractKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const JsonKeys& keys, const string& arrayKey = {});


protected:

    string createQueryString(map<string, string>&& queryValues, const RestCall call, const bool sign, const string& rcvWindow)
    {
      stringstream ss;

      // can leave a trailing '&' without borking the internets
      std::for_each(queryValues.begin(), queryValues.end(), [&ss](auto& it)
      {
        ss << std::move(it.first) << "=" << std::move(it.second) << "&"; // TODO doing a move() with operator<< here  - any advantage?
      });

      if (sign)
      {
        ss << "recvWindow=" << rcvWindow << "&timestamp=" << getTimestamp();

        string qs = ss.str();
        return qs + "&signature=" + (createSignature(m_apiAccess.secretKey, qs));
      }
      else
      {
        return ss.str();
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


    template<class RestResultT>
    pplx::task<RestResultT> sendRestRequest(const RestCall call, const web::http::method method, const bool sign, const MarketType mt, std::function<RestResultT(web::http::http_response)> handler, const string& rcvWindow, map<string, string>&& query = {})
    {
      try
      {
        string queryString{ createQueryString(std::move(query), call, true, rcvWindow) };

        auto request = createHttpRequest(method, getApiPath(mt, call) + "?" + queryString);

        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri(mt)) } };

        return client.request(std::move(request)).then([handler](web::http::http_response response)
        {
          if (response.status_code() == web::http::status_codes::OK)
          {
            return handler(response);
          }
          else
          {
            auto json = response.extract_json().get();
            return createInvalidRestResult<RestResultT>(utility::conversions::to_utf8string(json.serialize()));
          }
        });
      }
      catch (const pplx::task_canceled tc)
      {
        throw;
      }
      catch (const std::exception ex)
      {
        throw;
      }
    }


private:
    shared_ptr<WebSocketSession> m_session;
    MarketType m_marketType;

    vector<shared_ptr<WebSocketSession>> m_sessions;
    map<MonitorTokenId, shared_ptr<WebSocketSession>> m_idToSession;

    std::atomic_size_t m_monitorId;
    string m_exchangeBaseUri;
    std::atomic_bool m_connected;
    std::atomic_bool m_running;
    string m_listenKey;
    ApiAccess m_apiAccess;

    IntervalTimer m_userDataStreamTimer;
    map<RestCall, string> m_receiveWindowMap;
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
    UsdFuturesTestMarket(const ApiAccess& access = {}) : UsdFuturesMarket(MarketType::FuturesTest, TestFuturestWebSockUri, access)
    {

    }

    virtual ~UsdFuturesTestMarket()
    {
    }


  public:
    virtual TakerBuySellVolume takerBuySellVolume(map<string, string>&& query)
    {
      throw BfcppException("Function unavailable on Testnet");
    }
  };



  class UsdFuturesTestMarketPerfomance : public UsdFuturesTestMarket
  {
  public:
    UsdFuturesTestMarketPerfomance(const ApiAccess& access) : UsdFuturesTestMarket(access)
    {

    }

    virtual ~UsdFuturesTestMarketPerfomance()
    {
    }


    NewOrderPerformanceResult newOrderPerfomanceCheck(map<string, string>&& order)
    {
      return doNewOrderPerfomanceCheck(std::move(order)).get();
    }


    pplx::task<NewOrderPerformanceResult>  newOrderPerfomanceCheckAsync(map<string, string>&& order)
    {
      return doNewOrderPerfomanceCheck(std::move(order));
    }


  private:
    pplx::task<NewOrderPerformanceResult> doNewOrderPerfomanceCheck(map<string, string>&& order)
    {
      try
      {
        Clock::time_point handlerStart, handlerStop;

        auto handler = [&handlerStart, &handlerStop](web::http::http_response response)
        {
          handlerStart = Clock::now();

          NewOrderPerformanceResult result;

          auto json = response.extract_json().get();

          getJsonValues(json, result.response, set<string> {  "clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "avgPrice", "origQty", "price", "reduceOnly", "side", "positionSide", "status",
                                                              "stopPrice", "closePosition", "symbol", "timeInForce", "type", "origType", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

          return result;
        };

        return sendRestRequestPerformanceCheck(RestCall::NewOrder, web::http::methods::POST, true, marketType(), handler, receiveWindow(RestCall::NewOrder), std::move(order));
      }
      catch (const pplx::task_canceled tc)
      {
        throw BfcppDisconnectException("newOrder");
      }
      catch (const std::exception ex)
      {
        throw BfcppException(ex.what());
      }
    }


    pplx::task<NewOrderPerformanceResult> sendRestRequestPerformanceCheck(const RestCall call, const web::http::method method, const bool sign, const MarketType mt, std::function<NewOrderPerformanceResult (web::http::http_response)> handler, const string& rcvWindow, map<string, string>&& query = {})
    {
      try
      {
        auto start = std::chrono::high_resolution_clock::now();

        string queryString{ createQueryString(std::move(query), call, true, rcvWindow) };

        auto request = createHttpRequest(method, getApiPath(mt, call) + "?" + queryString);

        web::http::client::http_client client{ web::uri { utility::conversions::to_string_t(getApiUri(mt)) } };

        auto requestSent = std::chrono::high_resolution_clock::now();
        return client.request(std::move(request)).then([handler, start, requestSent](web::http::http_response response)
        {
          auto restCallTime = std::chrono::high_resolution_clock::now() - requestSent;

          if (response.status_code() == web::http::status_codes::OK)
          { 
            auto handlerCalled = std::chrono::high_resolution_clock::now();
            auto result = handler(response);
            auto handlerDone = std::chrono::high_resolution_clock::now();

            result.restQueryBuild = requestSent - start;
            result.restResponseHandler = handlerDone - handlerCalled;
            // requestSent - Start: is the time it takes to build the request
            // handlerDone - handlerCalled: time for the handler 
            result.bfcppTotalProcess = result.restQueryBuild + result.restResponseHandler;
            result.restApiCall = restCallTime;
            return result;
          }
          else
          {
            NewOrderPerformanceResult result{};
            result.bfcppTotalProcess = std::chrono::high_resolution_clock::now() - start;
            result.restApiCall = restCallTime;

            result.valid(false, utility::conversions::to_utf8string(response.extract_json().get().serialize()));
            return result;
          }
        });
      }
      catch (const pplx::task_canceled tc)
      {
        throw;
      }
      catch (const std::exception ex)
      {
        throw;
      }
    }

  };
}

#endif
