# Binance WebSockets

**This is an active project in the early stages, beginning mid Feb 2021, so I don't recommend relying on the library until it's fully tested and the API is stable.**

---

binancews is a C++17 library which receives market data from the Binance crypto currency exchange. 

The project uses Microsoft's cpprestsdk for asynchronous websocket functionality to receive the market data.

---

**binancewslib**
The library which handles all communications with the exchange


**binancews**
A test app to show how to use the library. 

Use the monitor functions to receive updates from streams via callback.

There are two types of callback signatures which differ in there argument, either Binance::BinanceKeyValueData or Binance::BinanceKeyMultiValueData.

Binance::BinanceKeyValueData contains a map<string, string> with the key being what the Binance API returns:

```
s=GRTUSDT
t=17825723
E=1613316912873
M=true
T=1613316912872
a=157236111
b=157236141
e=trade
m=false
p=1.99867000
q=32.30000000
```


Binance::BinanceKeyMultiValueData contains a map<string, map<string, string>> with the outer key being the symbol:

```
ZENUSDT
{
  E=1613317084088
  c=50.54400000
  e=24hrMiniTicker
  h=58.13300000
  l=49.79400000
  o=50.52900000
  q=18580149.39067900
  s=ZENUSDT
  v=337519.15900000ZENUSDT
}
```

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
      {"newClientOrderId", "1234"}, // set to a value you can refer to later, see docs for monitorUserData()
      {"price",""} // updated below with mark price from user data stream
  };

  UsdFuturesTestMarket futuresTest{ "YOUR API KEY", "YOUR SECRET KEY" };

  futuresTest.monitorMarkPrice(handleMarkPrice);  // to get an accurate price
  futuresTest.monitorUserData(handleUserDataUsdFutures); // to get order updates

  // wait on handleMakrPrice callback to signal it has price 
  logg("Waiting to receive a mark price for " + symbol);

  std::mutex mux;
  std::unique_lock lock (mux);
  priceSet.wait(lock);

  // update price then send order
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
1. Build vcpkg: open shell in vcpkg_linux and run:  bootstrap-vcpkg.sh
2. Install dependencies: in the same prompt run:
```
./vcpkg install cpprestsdk[websockets] poco boost-asio --triplet x64-linux
```
3. Go up a directory then into 'binancews' directory and run:   ```cmake . && make```
4. The binary is in the 'binancews' sub-dir ('binancews/binancews' from the top level directory) 


# Run
The provided ```binancews/binancews.cpp``` has a few functions to show the basics.
