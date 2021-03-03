# Binance Futures C++

**This is an active project in the early stages, beginning mid Feb 2021, so I don't recommend relying on the library until it's fully tested and the API is stable.**

## Update
**3rd Match 2021**
- Added accountInformation() : https://binance-docs.github.io/apidocs/futures/en/#account-information-v2-user_data
- Added allOrders() : https://binance-docs.github.io/apidocs/futures/en/#all-orders-user_data

**1st March 2021**
- I've decided to drop support for the Spot market to concentrate on futures
- Spot code removed from repo
- Repo renamed to "binance-futures-cpp", code namespace now "bfcpp"

---
## Summary
binancews is a C++17 library which receives market data from the Binance crypto currency exchange. 

The project uses Microsoft's cpprestsdk for asynchronous websocket functionality to receive the market data.

---
## Design
**bfcpplib**
The library which handles all communications with the exchange


**bfcpptest**
A test app to show how to use the library. 

### API
The API is thin - it expects and returns data in maps in rather than encapsulating data in classes/structs, e.g:

```cpp
class BinanceOrder : public Order
{

 Symbol m_symbol;
 MarketPrice m_price;
 OrderType m_type; 
 // etc
};
```
This is to avoid creating and populating objects when most likely users will either already have or intend to create a class structure for their needs.

Function return objects and callback args are by value, taking advantage of move-semantics and RVO.


### WebSocket Monitor Functions
Websocket streams are opened using the monitor functions, such as ```monitorMarkPrice()```.
The monitor functions require a callback function/lambda and are async, e.g.

```cpp
MonitorToken monitorMarkPrice(std::function<void(BinanceKeyMultiValueData)> onData)
```

### Rest Functions
The Rest calls are synchronous, returning an appropriate object, e.g.:  

```cpp
AllOrdersResult allOrders(map<string, string>&& query)
```

Most/all of the Rest functions expect an rvalue so that the query string can be built by moving values rather than copying.


## Examples

### Monitor Mark Price and Mini Ticker
This example monitors the mark price and mini tickers for all symbols.
We can use the same callback function here because it's only printing the values.

```cpp
#include <iostream>
#include <future>
#include <Futures.hpp>
#include <Logger.hpp>


int main(int argc, char** argv)
{
  // lambda for a map<string, map<string, string>>  monitor function
  auto handleKeyMultipleValueData = [](Binance::BinanceKeyMultiValueData data)
  {
    std::stringstream ss;

    for (auto& s : data.values)
    {
      ss << s.first << "\n{";

      for (auto& value : s.second)
      {
        ss << "\n" << value.first << "=" << value.second;
      }

      ss << "\n}";
    }

    logg(ss.str());
  };
  
  UsdFuturesMarket usdFutures;

  usdFutures.monitorMarkPrice(handleKeyMultipleValueData);
  usdFutures.monitorMiniTicker(handleKeyMultipleValueData);

  std::this_thread::sleep_for(10s);

  return 0;
}
```

### New Order
This example uses the mark price monitor to wait for the first mark price, then uses this to create an order.

```cpp
#include <iostream>
#include <future>
#include <Futures.hpp>
#include <Logger.hpp>

int main(int argc, char** argv)
{
  string symbol = "BTCUSDT";
  string markPriceString;
  std::condition_variable priceSet;

  auto handleMarkPrice = [&](Market::BinanceKeyMultiValueData data)
  {
      if (auto sym = data.values.find(symbol); sym != data.values.cend())
      {
          markPriceString = sym->second["p"];
          priceSet.notify_all();
      }
  };


  map<string, string> order =
  {
      {"symbol", symbol},
      {"side", "BUY"},        
      {"timeInForce", "GTC"},
      {"type", "LIMIT"},
      {"quantity", "1"},
      {"newClientOrderId", "1234"}, // set to a value you can refer to later, see docs for newOrder()
      {"price",""} // updated below with mark price 
  };

  UsdFuturesTestMarket futuresTest { Market::ApiAccess {"YOUR API KEY", "YOUR SECRET KEY"} };

  futuresTest.monitorMarkPrice(handleMarkPrice);  // to get an accurate price
  futuresTest.monitorUserData(handleUserDataUsdFutures); // to get order updates

  // wait on handleMakrPrice callback to signal it has price 
  logg("Waiting to receive a mark price for " + symbol);

  std::mutex mux;
  std::unique_lock lock (mux);
  priceSet.wait(lock);

  // update price then send order. priceTransform() to ensure price is suitable for the API
  order["price"] = Market::priceTransform(markPriceString);

  logg("Done. Sending order");

  // a blocking call, this waits for the Rest call to return
  // 1) 'result' contains the reply from the Rest call: 
  // 2) if the order is successful, the user data stream will also receive updates 
  auto result = futuresTest.newOrder(std::move(order));

  stringstream ss;
  ss << "\nnewOrder() returned:\n";
  for (const auto& val : result.result)
  {
      ss << val.first + "=" + val.second << "\n";
  }
  logg(ss.str());

  std::this_thread::sleep_for(60s);
  
  return 0;
}
```

### Get All Orders
```cpp
UsdFuturesTestMarket futuresTest { access };

framework::ScopedTimer timer;
auto result = futuresTest.allOrders({ {"symbol", "BTCUSDT"} });

stringstream ss;
ss << "\nFound " << result.response.size() << " orders in " << timer.stopLong() << " ms";

for (const auto& order : result.response)
{
  ss << "\n{";
  for (const auto& values : order)
  {
    ss << "\n\t" << values.first << "=" << values.second;
  }
  ss << "\n}";
}
logg(ss.str());
```

![output](https://user-images.githubusercontent.com/74328784/109874739-69985a00-7c67-11eb-961d-a43c9e46192c.png)
---


## Build

Dependencies are handled by vcpkg, a cross platform package manager.

### Windows
1. Build vcpkg: open a command prompt in vcpkg_win and run:   bootstrap-vcpkg.bat
2. Install dependencies: in the same prompt run:
```
   .\vcpkg install cpprestsdk[websockets] poco boost-asio --triplet x64-windows-static
```
3. Open the VS solution binancews/binancews.sln
4. Change to 'Release', right-click on the 'binancews' project and select "Setup as startup project"
5. Run


### Linux
_NOTE: testing on Linux has been limited, I hope to improve this in the coming weeks_

1. Build vcpkg: open shell in vcpkg_linux and run:  bootstrap-vcpkg.sh
2. Install dependencies: in the same prompt run:
```
./vcpkg install cpprestsdk[websockets] poco boost-asio --triplet x64-linux
```
3. Go up a directory then into 'binancews' directory and run:   ```cmake . && make```
4. The binary is in the 'binancews' sub-dir ('binancews/binancews' from the top level directory) 


# Run
The provided ```bfcpptest/bfcpptest.cpp``` has a few functions to show the basics.
