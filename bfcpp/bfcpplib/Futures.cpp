#include "Futures.hpp"


namespace bfcpp
{
  // -- Websocket monitors --

  MonitorToken UsdFuturesMarket::monitorMiniTicker(std::function<void(std::any)> onData)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG(" callback function null") };
    }

    auto handler = [](ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
    {
      auto json = web::json::value::parse(websocketInMessage.extract_string().get());

      AllMarketMiniTickerStream mtt;

      auto& data = json.as_array();
      for (auto& entry : data)
      {
        map<string, string> values;
        getJsonValues(entry, values, { "e", "E", "s", "c", "o", "h", "l", "v", "q" });

        mtt.data.emplace_back(std::move(values));
      }

      session->callback(std::any{ std::move(mtt) });
    };

    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/!miniTicker@arr", handler);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->callback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(std::any)> onData)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG(" callback function null")};
    }


    auto handler = [](ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
    {
      auto json = web::json::value::parse(websocketInMessage.extract_string().get());

      CandleStream cs;

      cs.eventTime = jsonValueToString(json[utility::conversions::to_string_t("E")]);
      cs.symbol = jsonValueToString(json[utility::conversions::to_string_t("s")]);

      auto& candleData = json[utility::conversions::to_string_t("k")].as_object();
      getJsonValues(candleData, cs.candle, { "t", "T", "s", "i", "f", "L", "o", "c", "h", "l", "v", "n", "x", "q", "V", "Q", "B" });

      session->callback( std::any{ std::move(cs) });
    };
        
    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@kline_" + interval, handler);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->callback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorSymbol(const string& symbol, std::function<void(std::any)> onData)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG(" callback function null") };
    }

    auto handler = [](ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
    {
      auto json = web::json::value::parse(websocketInMessage.extract_string().get());

      SymbolMiniTickerStream symbol;
     
      getJsonValues(json, symbol.data, { "e", "E", "s", "c", "o", "h", "l", "v", "q" });

      session->callback(std::any{ std::move(symbol) });
    };


    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@miniTicker", handler);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->callback = onData;
    }

    return std::get<0>(tokenAndSession);
  }
  


  MonitorToken UsdFuturesMarket::monitorSymbolBookStream(const string& symbol, std::function<void(std::any)> onData)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG("callback function null") };
    }

    auto handler = [](ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
    {
      auto json = web::json::value::parse(websocketInMessage.extract_string().get());

      SymbolBookTickerStream symbol;

      getJsonValues(json, symbol.data, { "e", "u","E","T","s","b","B", "a", "A" });

      session->callback(std::any{ std::move(symbol) });
    };
    

    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@bookTicker", handler);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->callback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorMarkPrice(std::function<void(std::any)> onData, const string& symbol)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG(" callback function null") };
    }


    auto handler = [](ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
    {
      auto json = web::json::value::parse(websocketInMessage.extract_string().get());

      MarkPriceStream mp;
      
      if (json.is_array())
      {
        auto& prices = json.as_array();
        for (auto& price : prices)
        {
          map<string, string> values;
          getJsonValues(price, values, { "e", "E","s","p","i","P","r","T" });

          mp.prices.emplace_back(std::move(values));
        }
      }
      else
      {
        // called with the symbol set, so not an array
        map<string, string> values;
        getJsonValues(json, values, { "e", "E","s","p","i","P","r","T" });

        mp.prices.emplace_back(std::move(values));
      }
      
      session->callback(std::any{ std::move(mp) });
    };

    string uri = "/ws/!markPrice@arr@1s";

    if (!symbol.empty())
      uri = "/ws/"+ strToLower(symbol)+"@markPrice@1s";

    auto tokenAndSession = createMonitor(m_exchangeBaseUri + uri, handler);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->callback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorUserData(std::function<void(std::any)> onData)
  {
    using namespace std::chrono_literals;

    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG("callback function null")};
    }

    MonitorToken monitorToken;

    if (createListenKey(m_marketType))
    {
      if (auto session = connect(m_exchangeBaseUri + "/ws/" + m_listenKey); session)
      {
        try
        {
          monitorToken.id = m_monitorId++;

          session->id = monitorToken.id;
          session->callback = onData;

          m_idToSession[monitorToken.id] = session;
          m_sessions.push_back(session);

          auto token = session->getCancelToken();
          session->receiveTask = pplx::create_task([session, token, onData, this]
          {
            try
            {
              handleUserDataStream(session, onData);
            }
            catch (BfcppDisconnectException)
            {
              throw;
            }
            catch (std::exception ex)
            {
              pplx::cancel_current_task();
            }            
          }, token);


          auto timerFunc = std::bind(&UsdFuturesMarket::onUserDataTimer, this);

          if (m_marketType == MarketType::FuturesTest)
          {
            //TODO ISSUE this doesn't seem to please the testnet, creating orders on the site keeps the connection alive
            m_userDataStreamTimer.start(timerFunc, 45s); // the test net seems to kick us out after 60s of no activity
          }
          else
          {
            m_userDataStreamTimer.start(timerFunc, 60s * 45); // 45 mins
          }
        }
        catch (BfcppDisconnectException)
        {
          throw;
        }
        catch (std::exception ex)
        {
          throw BfcppException(ex.what());
        }
      }
    }

    return monitorToken;
  }



  MonitorToken UsdFuturesMarket::monitorPartialBookDepth(const string& symbol, const string& level, const string& interval, std::function<void(std::any)> onData)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG(" callback function null") };
    }

    return doMonitorBookDepth(symbol, level, interval, onData);
  }



  MonitorToken UsdFuturesMarket::monitorDiffBookDepth(const string& symbol, const string& interval, std::function<void(std::any)> onData)
  {
    if (onData == nullptr)
    {
      throw BfcppException{ BFCPP_FUNCTION_MSG(" callback function null") };
    }

    return doMonitorBookDepth(symbol, "", interval, onData);
  }



  MonitorToken UsdFuturesMarket::doMonitorBookDepth(const string& symbol, const string& level, const string& interval, std::function<void(std::any)> onData)
  {
    auto handler = [](ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session)
    {
      static const utility::string_t SymbolField = utility::conversions::to_string_t("s");
      static const utility::string_t EventTimeField = utility::conversions::to_string_t("E");
      static const utility::string_t TransactionTimeField = utility::conversions::to_string_t("T");
      static const utility::string_t FirstUpdateIdField = utility::conversions::to_string_t("U");
      static const utility::string_t FinalUpdateIdField = utility::conversions::to_string_t("u");
      static const utility::string_t PreviousFinalUpdateIdField = utility::conversions::to_string_t("pu");
      static const utility::string_t BidsField = utility::conversions::to_string_t("b");
      static const utility::string_t AsksField = utility::conversions::to_string_t("a");


      BookDepthStream result;

      auto json = web::json::value::parse(websocketInMessage.extract_string().get());

      result.symbol = jsonValueToString(json[utility::conversions::to_string_t(SymbolField)]);
      result.eventTime = jsonValueToString(json[utility::conversions::to_string_t(EventTimeField)]);
      result.transactionTime = jsonValueToString(json[utility::conversions::to_string_t(TransactionTimeField)]);
      result.firstUpdateId = jsonValueToString(json[utility::conversions::to_string_t(FirstUpdateIdField)]);
      result.finalUpdateId = jsonValueToString(json[utility::conversions::to_string_t(FinalUpdateIdField)]);
      result.previousFinalUpdateId = jsonValueToString(json[utility::conversions::to_string_t(PreviousFinalUpdateIdField)]);

      // bids
      auto& bidsArray = json[BidsField].as_array();

      for (auto& bid : bidsArray)
      {
        auto& bidValue = bid.as_array();
        result.bids.emplace_back(std::make_pair(jsonValueToString(bidValue[0]), jsonValueToString(bidValue[1])));
      }

      // asks 
      auto& asksArray = json[AsksField].as_array();

      for (auto& ask : asksArray)
      {
        auto& askValue = ask.as_array();
        result.asks.emplace_back(std::make_pair(jsonValueToString(askValue[0]), jsonValueToString(askValue[1])));
      }

      session->callback(std::any{ std::move(result) });
    };

    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@depth" + level + "@" + interval, handler);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->callback = onData;
    }

    return std::get<0>(tokenAndSession);
  }


  // -- REST Calls --


  AccountInformation UsdFuturesMarket::accountInformation()
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        AccountInformation info;

        auto json = response.extract_json().get();

        const utility::string_t AssetField = utility::conversions::to_string_t("assets");
        const utility::string_t PositionsField = utility::conversions::to_string_t("positions");


        getJsonValues(json, info.data, vector<string> {  "feeTier", "canTrade", "canDeposit", "canWithdraw", "updateTime", "totalInitialMargin", "totalMaintMargin", "totalWalletBalance",
                                                      "totalUnrealizedProfit", "totalMarginBalance", "totalPositionInitialMargin", "totalOpenOrderInitialMargin", "totalCrossWalletBalance",
                                                      "totalCrossUnPnl", "availableBalance", "maxWithdrawAmount"});


        auto& assetArray = json[AssetField].as_array();
        for (const auto& entry : assetArray)
        {
          map<string, string> order;
          getJsonValues(entry, order, vector<string> { "asset", "walletBalance", "unrealizedProfit", "marginBalance", "maintMargin", "initialMargin", "positionInitialMargin",
                                                    "openOrderInitialMargin", "crossWalletBalance", "crossUnPnl", "availableBalance", "maxWithdrawAmount"});

          info.assets.emplace_back(std::move(order));
        }


        auto& positionArray = json[PositionsField].as_array();
        for (const auto& entry : positionArray)
        {
          map<string, string> position;
          getJsonValues(entry, position, vector<string> {  "symbol", "initialMargin", "maintMargin", "unrealizedProfit", "positionInitialMargin", "openOrderInitialMargin",
                                                        "leverage", "isolated", "entryPrice", "maxNotional", "positionSide", "positionAmt"});

          info.positions.emplace_back(std::move(position));
        }

        return info;
      };

      return sendRestRequest<AccountInformation>(RestCall::AccountInfo, web::http::methods::GET, true, m_marketType, handler, receiveWindow(RestCall::AccountInfo)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("accountInformation");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }



  AccountBalance UsdFuturesMarket::accountBalance()
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        AccountBalance balance;

        auto json = response.extract_json().get();
        for (const auto& entry : json.as_array())
        {
          map<string, string> order;
          getJsonValues(entry, order, { "accountAlias", "asset", "balance", "crossWalletBalance", "crossUnPnl", "availableBalance", "maxWithdrawAmount"});

          balance.balances.emplace_back(std::move(order));
        }

        return balance;
      };

      return sendRestRequest<AccountBalance>(RestCall::AccountBalance, web::http::methods::GET, true, m_marketType, handler, receiveWindow(RestCall::AccountBalance)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("accountBalance");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }



  TakerBuySellVolume UsdFuturesMarket::takerBuySellVolume(map<string, string>&& query)
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        TakerBuySellVolume result;

        auto json = response.extract_json().get();

        for (const auto& entry : json.as_array())
        {
          map<string, string> order;
          getJsonValues(entry, order, { "buySellRatio", "buyVol", "sellVol", "timestamp"});

          result.response.emplace_back(std::move(order));
        }

        return result;
      };

      return sendRestRequest<TakerBuySellVolume>(RestCall::TakerBuySellVolume, web::http::methods::GET, true, m_marketType, handler, receiveWindow(RestCall::TakerBuySellVolume), std::move(query)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("takerBuySellVolume");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }



  KlineCandlestick UsdFuturesMarket::klines(map<string, string>&& query)
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        KlineCandlestick result;

        auto json = response.extract_json().get();

        // kline does not return a key/value map, instead an array of arrays. 
        // the outer array has an entry per interval, with each inner array containing 12 fields for that interval (i.e. open time, close time, open price, close price, etc)

        auto& intervalPeriod = json.as_array();
        for (auto& interval : intervalPeriod)
        {
          vector<string> stickValues;
          stickValues.reserve(12);

          auto& sticksArray = interval.as_array();
          for (auto& stick : sticksArray)
          {
            stickValues.emplace_back(jsonValueToString(stick));
          }

          result.response.emplace_back(std::move(stickValues));
        }

        return result;
      };

      return sendRestRequest<KlineCandlestick>(RestCall::KlineCandles, web::http::methods::GET, true, m_marketType, handler, receiveWindow(RestCall::KlineCandles), std::move(query)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("klines");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }

  

  AllOrdersResult UsdFuturesMarket::allOrders(map<string, string>&& query)
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        AllOrdersResult result;

        auto json = response.extract_json().get();
        
        for (const auto& entry : json.as_array())
        {
          map<string, string> order;
          getJsonValues(entry, order, { "avgPrice", "clientOrderId", "cumQuote", "executedQty", "orderId", "origQty", "origType", "price", "reduceOnly", "side", "positionSide", "status",
                                        "stopPrice", "closePosition", "symbol", "time", "timeInForce", "type", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

          result.response.emplace_back(std::move(order));
        }

        return result;
      };

      return sendRestRequest<AllOrdersResult>(RestCall::AllOrders, web::http::methods::GET, true, m_marketType, handler, receiveWindow(RestCall::AllOrders), std::move(query)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("allOrders");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }



  ExchangeInfo UsdFuturesMarket::exchangeInfo()
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        ExchangeInfo result;

        auto json = response.extract_json().get();

        result.timezone = jsonValueToString(json[utility::conversions::to_string_t("timezone")]);
        result.serverTime = jsonValueToString(json[utility::conversions::to_string_t("serverTime")]);

        // rate limits
        auto& rateLimits = json[utility::conversions::to_string_t("rateLimits")].as_array();
        for (auto& rate : rateLimits)
        {
          map<string, string> values;
          getJsonValues(rate, values, { "rateLimitType", "interval", "intervalNum", "limit"});

          result.rateLimits.emplace_back(std::move(values));
        }
        

        // symbols
        auto& symbols = json[utility::conversions::to_string_t("symbols")].as_array();
        for (auto& symbol : symbols)
        {
          ExchangeInfo::Symbol sym;

          getJsonValues(symbol, sym.data, { "symbol", "pair", "contractType", "deliveryDate", "onboardDate", "status", "maintMarginPercent", "requiredMarginPercent", "baseAsset",
                                            "quoteAsset", "marginAsset", "pricePrecision", "quantityPrecision", "baseAssetPrecision", "quotePrecision", "underlyingType",
                                            "settlePlan", "triggerProtect"});


          auto& subType = symbol[utility::conversions::to_string_t("underlyingSubType")].as_array();
          for (auto& st : subType)
          {
            sym.underlyingSubType.emplace_back(jsonValueToString(st));
          }
          

          auto& filters = symbol[utility::conversions::to_string_t("filters")].as_array();
          for (auto& filter : filters)
          {
            map<string, string> values;
            getJsonValues(filter, values,  {"filterType", "maxPrice", "minPrice", "tickSize", "stepSize", "maxQty", "minQty", "notional", "multiplierDown", "multiplierUp", "multiplierDecimal"});

            sym.filters.emplace_back(std::move(values));
          }


          auto& orderTypes = symbol[utility::conversions::to_string_t("orderTypes")].as_array();
          for (auto& ot : orderTypes)
          {
            sym.orderTypes.emplace_back(jsonValueToString(ot));
          }


          auto& tif = symbol[utility::conversions::to_string_t("timeInForce")].as_array();
          for (auto& t : tif)
          {
            sym.timeInForce.emplace_back(jsonValueToString(t));
          }

          result.symbols.emplace_back(sym);
        }

        return result;
      };

      return sendRestRequest<ExchangeInfo>(RestCall::ExchangeInfo, web::http::methods::GET, true, m_marketType, handler, receiveWindow(RestCall::ExchangeInfo)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("klines");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }



  OrderBook UsdFuturesMarket::orderBook(map<string, string>&& query)
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        OrderBook result;

        auto json = response.extract_json().get();

        static const utility::string_t BidsField = utility::conversions::to_string_t("bids");
        static const utility::string_t AsksField = utility::conversions::to_string_t("asks");

        result.messageOutputTime = jsonValueToString(json[utility::conversions::to_string_t("E")]);
        result.transactionTime = jsonValueToString(json[utility::conversions::to_string_t("T")]);
        result.lastUpdateId = jsonValueToString(json[utility::conversions::to_string_t("lastUpdateId")]);

        // bids
        auto& bidsArray = json[BidsField].as_array();

        for (auto& bid : bidsArray)
        {
          auto& bidValue = bid.as_array();
          result.bids.emplace_back(std::make_pair(jsonValueToString(bidValue[0]), jsonValueToString(bidValue[1])));
        }

        // asks 
        auto& asksArray = json[AsksField].as_array();

        for (auto& ask : asksArray)
        {
          auto& askValue = ask.as_array();
          result.asks.emplace_back(std::make_pair(jsonValueToString(askValue[0]), jsonValueToString(askValue[1])));
        }

        return result;
      };

      return sendRestRequest<OrderBook>(RestCall::OrderBook , web::http::methods::GET, false, m_marketType, handler, receiveWindow(RestCall::OrderBook), std::move(query)).get();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("klines");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }

  




  // -- connection/session ---

  void UsdFuturesMarket::disconnect(const MonitorToken& mt, const bool deleteSession)
  {
    if (auto itIdToSession = m_idToSession.find(mt.id); itIdToSession != m_idToSession.end())
    {
      auto& session = itIdToSession->second;

      session->cancel();

      session->client.close(ws::client::websocket_close_status::going_away).then([&session]()
      {
        session->connected = false;
      }).wait();

      // calling wait() on a task that's already cancelled throws an exception
      if (!session->receiveTask.is_done())
      {
        session->receiveTask.wait();
      }
           

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



  void UsdFuturesMarket::disconnect()
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



  bool UsdFuturesMarket::createListenKey(const MarketType marketType)
  {
    try
    {
      auto handler = [](web::http::http_response response)
      {
        ListenKey result;

        auto json = response.extract_json().get();

        result.listenKey = utility::conversions::to_utf8string(json[utility::conversions::to_string_t(ListenKeyName)].as_string());

        return result;
      };

      auto lk = sendRestRequest<ListenKey>(RestCall::ListenKey, web::http::methods::POST, true, marketType, handler, receiveWindow(RestCall::ListenKey)).get();
      
      m_listenKey = lk.listenKey;

      return lk.valid() && !m_listenKey.empty();
    }
    catch (const pplx::task_canceled tc)
    {
      throw BfcppDisconnectException("createListenKey");
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }




  // -- data/util --

  void UsdFuturesMarket::extractUsdFuturesUserData(shared_ptr<WebSocketSession> session, web::json::value&& jsonVal)
  {
    const utility::string_t CodeField = utility::conversions::to_string_t("code");
    const utility::string_t MsgField = utility::conversions::to_string_t("msg");

    if (jsonVal.has_string_field(CodeField) && jsonVal.has_string_field(MsgField))
    {
      throw BfcppException(utility::conversions::to_utf8string(jsonVal.at(CodeField).as_string()) + " : " + utility::conversions::to_utf8string(jsonVal.at(MsgField).as_string()));
    }
    else
    {
      const utility::string_t EventTypeField = utility::conversions::to_string_t("e");
      const utility::string_t EventMarginCall = utility::conversions::to_string_t("MARGIN_CALL");
      const utility::string_t EventOrderTradeUpdate = utility::conversions::to_string_t("ORDER_TRADE_UPDATE");
      const utility::string_t EventAccountUpdate = utility::conversions::to_string_t("ACCOUNT_UPDATE");
      const utility::string_t EventStreamExpired = utility::conversions::to_string_t("listenKeyExpired");


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
      else if (eventValue == EventStreamExpired)
      {
        type = UsdFutureUserData::EventType::DataStreamExpired;
      }


      UsdFutureUserData userData(type);

      if (type != UsdFutureUserData::EventType::Unknown)
      {
        switch (type)
        {

        case UsdFutureUserData::EventType::MarginCall:
        {
          const utility::string_t BalancesField = utility::conversions::to_string_t("p");

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
          getJsonValues(jsonVal[OrdersField].as_object(), values, { "s", "c", "S", "o", "f", "q", "p", "ap", "sp", "x", "X", "i", "l", "z", "L", "N",
                                                                      "n", "T", "t", "b", "a", "m", "R", "wt", "ot", "ps", "cp", "AP", "cr", "rp" });

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


        case UsdFutureUserData::EventType::DataStreamExpired:
          throw BfcppException("Usd Futures user data stream has expired");
          break;


        default:
          // handled above
          break;
        }


        session->callback(std::any{ std::move(userData) });
      }
    }
  }

}