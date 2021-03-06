#include "Futures.hpp"


namespace bfcpp
{
  // -- monitors --

  MonitorToken UsdFuturesMarket::monitorMiniTicker(std::function<void(BinanceKeyMultiValueData)> onData)
  {
    static const JsonKeys keys
    {
      { {"s"}, {"e", "E", "s", "c", "o", "h", "l", "v", "q"} }
    };

    if (onData == nullptr)
    {
      throw BfcppException{ "monitorMiniTicker callback function null" };
    }

    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/!miniTicker@arr", keys, "s");

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorKlineCandlestickStream(const string& symbol, const string& interval, std::function<void(BinanceKeyMultiValueData)> onData)
  {
    static const JsonKeys keys
    {
      {"e", {}},
      {"E", {}},
      {"s", {}},
      {"k", {"t", "T", "s", "i", "f", "L", "o", "c", "h", "l", "v", "n", "x", "q", "V", "Q", "B"}}
    };

    if (onData == nullptr)
    {
      throw BfcppException{ "monitorKlineCandlestickStream callback function null" };
    }


    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@kline_" + interval, keys);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorSymbol(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
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

    if (onData == nullptr)
    {
      throw BfcppException{ "monitorSymbol callback function null" };
    }


    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@miniTicker", keys);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->onDataUserCallback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorSymbolBookStream(const string& symbol, std::function<void(BinanceKeyValueData)> onData)
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

    if (onData == nullptr)
    {
      throw BfcppException{ "monitorSymbolBookStream callback function null" };
    }


    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/" + strToLower(symbol) + "@bookTicker", keys);

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->onDataUserCallback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorMarkPrice(std::function<void(BinanceKeyMultiValueData)> onData)
  {
    static const JsonKeys keys
    {
        {"s", {"e", "E","s","p","i","P","r","T"}}
    };

    if (onData == nullptr)
    {
      throw BfcppException{ "monitorMarkPrice callback function null" };
    }

    auto tokenAndSession = createMonitor(m_exchangeBaseUri + "/ws/!markPrice@arr@1s", keys, "s");

    if (std::get<0>(tokenAndSession).isValid())
    {
      std::get<1>(tokenAndSession)->onMultiValueDataUserCallback = onData;
    }

    return std::get<0>(tokenAndSession);
  }



  MonitorToken UsdFuturesMarket::monitorUserData(std::function<void(UsdFutureUserData)> onData)
  {
    using namespace std::chrono_literals;

    if (onData == nullptr)
    {
      throw BfcppException{"monitorUserData callback function null"};
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
          session->onUsdFuturesUserDataCallback = onData;

          m_idToSession[monitorToken.id] = session;
          m_sessions.push_back(session);

          auto token = session->getCancelToken();
          session->receiveTask = pplx::create_task([session, token, &onData, this]
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
              throw BfcppException (ex.what());
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




  // -- REST  --


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


        getJsonValues(json, info.data, set<string> {  "feeTier", "canTrade", "canDeposit", "canWithdraw", "updateTime", "totalInitialMargin", "totalMaintMargin", "totalWalletBalance",
                                                      "totalUnrealizedProfit", "totalMarginBalance", "totalPositionInitialMargin", "totalOpenOrderInitialMargin", "totalCrossWalletBalance",
                                                      "totalCrossUnPnl", "availableBalance", "maxWithdrawAmount"});


        auto& assetArray = json[AssetField].as_array();
        for (const auto& entry : assetArray)
        {
          map<string, string> order;
          getJsonValues(entry, order, set<string> { "asset", "walletBalance", "unrealizedProfit", "marginBalance", "maintMargin", "initialMargin", "positionInitialMargin",
                                                    "openOrderInitialMargin", "crossWalletBalance", "crossUnPnl", "availableBalance", "maxWithdrawAmount"});

          info.assets.emplace_back(std::move(order));
        }


        auto& positionArray = json[PositionsField].as_array();
        for (const auto& entry : positionArray)
        {
          map<string, string> position;
          getJsonValues(entry, position, set<string> {  "symbol", "initialMargin", "maintMargin", "unrealizedProfit", "positionInitialMargin", "openOrderInitialMargin",
                                                        "leverage", "isolated", "entryPrice", "maxNotional", "positionSide", "positionAmt"});

          info.positions.emplace_back(std::move(position));
        }

        return info;
      };

      return sendRestRequest<AccountInformation>(RestCall::AccountInfo, web::http::methods::GET, true, m_marketType, handler).get();
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
          getJsonValues(entry, order, set<string> { "accountAlias", "asset", "balance", "crossWalletBalance", "crossUnPnl", "availableBalance", "maxWithdrawAmount"});

          balance.balances.emplace_back(std::move(order));
        }

        return balance;
      };

      return sendRestRequest<AccountBalance>(RestCall::AccountBalance, web::http::methods::GET, true, m_marketType, handler).get();
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
          getJsonValues(entry, order, set<string> { "buySellRatio", "buyVol", "sellVol", "timestamp"});

          result.response.emplace_back(std::move(order));
        }

        return result;
      };

      return sendRestRequest<TakerBuySellVolume>(RestCall::TakerBuySellVolume, web::http::methods::GET, true, m_marketType, handler, std::move(query)).get();
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

      return sendRestRequest<KlineCandlestick>(RestCall::KlineCandles, web::http::methods::GET, true, m_marketType, handler, std::move(query)).get();
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



  NewOrderResult UsdFuturesMarket::newOrder(map<string, string>&& order)
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

      return sendRestRequest<NewOrderResult>(RestCall::NewOrder, web::http::methods::POST, true, m_marketType, handler, std::move(order)).get();
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
          getJsonValues(entry, order, set<string> { "avgPrice", "clientOrderId", "cumQuote", "executedQty", "orderId", "origQty", "origType", "price", "reduceOnly", "side", "positionSide", "status",
                                                    "stopPrice", "closePosition", "symbol", "time", "timeInForce", "type", "activatePrice", "priceRate", "updateTime", "workingType", "priceProtect"});

          result.response.emplace_back(std::move(order));
        }

        return result;
      };

      return sendRestRequest<AllOrdersResult>(RestCall::AllOrders, web::http::methods::GET, true, m_marketType, handler, std::move(query)).get();
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



  CancelOrderResult UsdFuturesMarket::cancelOrder(map<string, string>&& order)
  {   
    try
    {
      auto handler = [](web::http::http_response response)
      {
        CancelOrderResult result;

        auto json = response.extract_json().get();
        getJsonValues(json, result.response, set<string> {"clientOrderId", "cumQty", "cumQuote", "executedQty", "orderId", "origQty", "origType", "price", "reduceOnly", "side", "positionSide",
                                                          "status", "stopPrice", "closePosition", "symbol", "timeInForce", "type", "activatePrice", "priceRate", "updateTime", "workingType", "workingType"});

        return result;
      };

      return sendRestRequest<CancelOrderResult>(RestCall::CancelOrder, web::http::methods::DEL, true, m_marketType, handler, std::move(order)).get();
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




  // -- connection/session ---

  void UsdFuturesMarket::disconnect(const MonitorToken& mt, const bool deleteSession)
  {
    if (auto itIdToSession = m_idToSession.find(mt.id); itIdToSession != m_idToSession.end())
    {
      auto& session = itIdToSession->second;

      session->cancel();

      // calling wait() on a task that's already cancelled throws an exception
      if (!session->receiveTask.is_done())
      {
        session->receiveTask.wait();
      }

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



  shared_ptr<WebSocketSession> UsdFuturesMarket::connect(const string& uri)
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



  std::tuple<MonitorToken, shared_ptr<WebSocketSession>> UsdFuturesMarket::createMonitor(const string& uri, const JsonKeys& keys, const string& arrayKey)
  {
    std::tuple<MonitorToken, shared_ptr<WebSocketSession>> tokenAndSession;

    if (auto session = connect(uri); session)
    {
      auto extractFunction = std::bind(&UsdFuturesMarket::extractKeys, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

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



  MonitorToken UsdFuturesMarket::createReceiveTask(shared_ptr<WebSocketSession> session, std::function<void(ws::client::websocket_incoming_message, shared_ptr<WebSocketSession>, const JsonKeys&, const string&)> extractFunc, const JsonKeys& keys, const string& arrayKey)
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


      monitorToken.id = m_monitorId++;
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

      auto lk = sendRestRequest<ListenKey>(RestCall::ListenKey, web::http::methods::POST, true, marketType, handler).get();
      
      m_listenKey = lk.listenKey;

      return lk.valid() && !m_listenKey.empty();
    }
    catch (const pplx::task_canceled tc)
    {
      // task cancelling is not a problem, it's how the websockets library works to signal the task has quit                               
    }
    catch (const std::exception ex)
    {
      throw BfcppException(ex.what());
    }
  }



  // -- data/util --
  
  void UsdFuturesMarket::extractKeys(ws::client::websocket_incoming_message websocketInMessage, shared_ptr<WebSocketSession> session, const JsonKeys& keys, const string& arrayKey)
  {
    try
    {
      std::string strMsg;
      websocketInMessage.extract_string().then([&strMsg, cancelToken = session->getCancelToken()](pplx::task<std::string> str_tsk)
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
          throw BfcppException(utility::conversions::to_utf8string(jsonVal.at(CodeField).as_string()) + " : " + utility::conversions::to_utf8string(jsonVal.at(MsgField).as_string()));
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


        session->onUsdFuturesUserDataCallback(std::move(userData));
      }
    }
  }


}