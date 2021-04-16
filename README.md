# Binance Futures C++

**This is an active project in the early stages, beginning mid Feb 2021, so I don't recommend relying on the library until it's had more testing and the API is stable.**
# Summary
Binance Futures C++ is a C++17 library for Binance's REST and websockets API.

The project uses Microsoft's cpprestsdk for asynchronous websockets/HTTP functionality.

---

# Recent Updates
**11th April 2021**
- Fixed closing/shutdown for market monitors and user data stream

**4th April 2021**
- Added optional symbol arg to ```monitorMarkPrice()``` to monitor one symbol rather than all

**3rd April 2021**
- Fixed bug causing websockets to stop responding when a ping was received

---

# Performance
To accurately record timings there's a specific class, ```UsdFuturesTestMarketPerformance```, with functions to create a new order whilst adding timings. This is done on the **TestNet** exchange.

The bfcpptest.cpp has ```performanceCheckSync()``` and ```performanceCheckAsync()``` functions showing how to use the performance check.

As seen from the results below, each call to newOrder() is mostly dependent on the Binance API latency, which is the ping plus Binance's processing time, ranging from 300ms to 750ms.

See "New Order - Async" for a code example.


## Results
All times in nanoseconds. Ran on an Intel i7-6700 (8Mb L3 cache, 4 cores/8 threads). 

- Avg. Rest Query Build: time to build the HTTP request objects
- Avg. Rest Call Latency: time between sending the HTTP request to Binance and receiving the response
- Avg. Rest Response Handler: time to parse/extract the JSON in the HTTP response which populates the return object


### Sync
**5 Orders**
```
Total: 5 orders in 5940 milliseconds

|                       | time (nanoseconds) |
------------------------------------------
Avg. Rest Query Build:              58440
Avg. Rest Call Latency:         593600960 (Min:370410500, Max: 652192600)
Avg. Rest Response Handler:        305500
------------------------------------------
```

**10 Orders**
```
Total: 10 orders in 13564 milliseconds

|                    | time (nanoseconds) |
------------------------------------------
Avg. Rest Query Build:         52170
Avg. Rest Call Latency:	   677898590 (Min:647961400, Max: 835470300)
Avg. Rest Response Handler:   241510
------------------------------------------
```


### Async
**5 Orders**
```
Total: 5 orders in 771 milliseconds

|                    | time (nanoseconds) |
------------------------------------------
Avg. Rest Query Build:        102700
Avg. Rest Call Latency:    769450300 (Min:769039600, Max: 770068700)
Avg. Rest Response Handler:   630020
------------------------------------------
```

**10 Orders**

```
Total: 10 orders in 809 milliseconds

|                    | time (nanoseconds) |
------------------------------------------
Avg. Rest Query Build:         55650
Avg. Rest Call Latency:    702554320 (Min:361157400, Max: 808256700)
Avg. Rest Response Handler:   206550
```

---

## Design
**bfcpplib**
The library which handles all communications with the exchange


**bfcpptest**
A test app to show how to use the library. 

### API
The API is thin - it returns data in structs, which are wrappers for vectors and maps which contain the JSON.

This is to avoid creating and populating objects when users will either already have, or intend to, create a class structure for their needs.

### WebSocket Monitor Functions
Websocket streams are opened using the monitor functions, such as ```monitorMarkPrice()```.

The monitor functions return a ```MonitorToken``` and take an ```std::function<std::any>``` argument.  The MonitorToken is used when cancelling the monitor function.

```cpp
MonitorToken monitorMarkPrice(std::function<void(std::any)> onData);
```


### Rest Functions
Most of the Rest calls are synchronous, returning an appropriate object, e.g.:  

```cpp
AllOrdersResult allOrders(map<string, string>&& query)
```

There are some which have an asynchronous version, such as ```newOrderAsync() ```


## Examples

### Websockets - Monitor Mark Price and Mini Ticker
This monitors the mark price and mini tickers for all symbols.

```cpp
#include <iostream>
#include <future>
#include <Futures.hpp>
#include <Logger.hpp>

int main(int argc, char** argv)
{
   std::cout << "\n\n--- USD-M Futures Multiple Streams on Futures ---\n";

   auto markPriceHandler = [](std::any data)
   {
      auto priceData = std::any_cast<MarkPriceStream> (data);

      std::stringstream ss;

      for (const auto& pair : priceData.prices)
      {
         std::for_each(std::begin(pair), std::end(pair), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
      }

      logg(ss.str());
    };


   auto onSymbolMiniTickHandler = [](std::any data)
   {
      auto ticker = std::any_cast<AllMarketMiniTickerStream> (data);
      stringstream ss;

      for (auto& tick : ticker.data)
      {
         std::for_each(std::begin(tick), std::end(tick), [&ss](auto pair) { ss << "\n" << pair.first << "=" << pair.second; });
      }
		
      logg(ss.str());
   };

   UsdFuturesMarket usdFutures;
   usdFutures.monitorMarkPrice(markPriceHandler);
   usdFutures.monitorMiniTicker(onSymbolMiniTickHandler);

   std::this_thread::sleep_for(10s);

   return 0;
}
```

### New Order - Async
This shows how to create orders asynchronously. The ```newOrder()``` returns a ```pplx::task``` which contains the API result (NewOrderResult). 
Each task is stored in a vector then we use ```pplx::when_all()``` to wait for all to complete.


```cpp
#include <iostream>
#include <future>
#include <Futures.hpp>
#include <Logger.hpp>

static size_t NumNewOrders = 5;

int main(int argc, char** argv)
{
   std::cout << "\n\n--- USD-M Futures New Order Async ---\n"; 

   UsdFuturesTestMarket market{ {"YOUR API KEY", "YOUR SECRET KEY"} };

   vector<pplx::task<NewOrderResult>> results;
   results.reserve(NumNewOrders);

   logg("Sending orders");

   for (size_t i = 0; i < NumNewOrders; ++i)
   {
      map<string, string> order =
      {
         {"symbol", "BTCUSDT"},
	 {"side", "BUY"},
	 {"type", "MARKET"},
	 {"quantity", "0.001"}
      };

      results.emplace_back(std::move(market.newOrderAsync(std::move(order))));  
   }

   logg("Waiting for all to complete");

   // note: you could use pplx::when_any() to handle each task as it completes, 
   //       then call when_any() until all are finished.

   // wait for the new order tasks to return, the majority of which is due to the REST call latency
   pplx::when_all(std::begin(results), std::end(results)).wait();

  logg("Done: ");

  stringstream ss;
  ss << "\nOrder Ids: ";
  
  for (auto& task : results)
  {
     NewOrderResult result = task.get();

     if (result.valid())
     {
        // do stuff with result
	ss << "\n" << result.response["orderId"];
     }
   }

   std::cout << ss.str();
  
   return 0;
}
```
Output:
```
[18:31:50.436] Sending orders
[18:31:50.438] Waiting for all to complete
[18:31:51.193] Done:
[18:31:51.194]
Order Ids:
2649069688
2649069693
2649069694
2649069692
2649069691
```

### Get All Orders
```cpp
int main(int argc, char** argv)
{
  UsdFuturesTestMarket futuresTest { ApiAccess {"YOUR API KEY", "YOUR SECRET KEY"} };

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
  std::cout << ss.str();

  return 0;
}


```
---


## Build

Dependencies are handled by vcpkg, a cross platform package manager.

### Windows
1. Build vcpkg: open a command prompt in vcpkg_win and run:   ```bootstrap-vcpkg.bat```
2. Install dependencies: in the same prompt run:
```
   .\vcpkg install cpprestsdk[websockets] poco boost-asio --triplet x64-windows-static
```
3. Open the VS solution ```bfcpp/bfcpp.sln```
4. Change to 'Release', right-click on the 'bfcpptest' project and select "Setup as startup project"
5. Run


### Linux
_NOTE: testing on Linux has been limited, I hope to improve this in the coming weeks_

1. Build vcpkg: open shell in vcpkg_linux and run:  ```bootstrap-vcpkg.sh```
2. Install dependencies: in the same prompt run:
```
./vcpkg install cpprestsdk[websockets] poco boost-asio --triplet x64-linux
```
3. Go up a directory then into 'bfcpp' directory and run:   ```cmake . && make```
4. The binary is in the 'bfcpptest' sub-dir ('bfcpp/bfcpptest' from the top level directory) 


# Run
The provided ```bfcpptest/bfcpptest.cpp``` has a few functions to show the basics.

- Some functions can run without an API or secret key, such as Kline/Candlesticks
- You can pass an api/secret key by editing code or using a key file. The key file has 3 lines:
 ```
 <live | test>
 <api key>
 <secret key>
  ```
e.g. :  
```
test
myapiKeyMyKey723423Ju&jNhuayNahas617238Jaiasjd31as52v46523435vs
8LBwbPvcub5GHtxLgWDZnm23KFcXwXwXwXwLBwbLBwbAABBca-sdasdasdas123
```

Run: ```>./bfcpptest /path/to/mykeyfile.txt```
