# Binance WebSockets

**This is an active project in the early stages so I don't recommend relying on the API until it's stable.**


# Current State
General:
- API needs tidying because not all streams are valid for both Spot and Futures

Spot:
- Many streams for Spot market
- User data stream for Spot market added, requires more testing

Futures:
- Added mark price

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

```cpp
#include <BinanceExchange.hpp>
#include <Logger.hpp>


int main(int argc, char** argv)
{
  // lambda for a map<string, string> monitor function
  auto handleKeyValueData = [](Binance::BinanceKeyValueData data)
  {
    for (auto& p : data.values)
    {
      logg(p.first + "=" + p.second);
    }
  };


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

      ss << s.first << "\n}";
    }

    logg(ss.str());
  };


  
  Binance be;
  
  // symbols always lower case
  if (auto valid = be.monitorTradeStream("grtusdt", handleKeyValueData); !valid.isValid())
  {
    logg("monitorTradeStream failed");
  }

  if (auto valid = be.monitorMiniTicker(handleKeyMultipleValueData); !valid.isValid())
  {
    logg("monitorAllSymbols failed");
  }


  bool run = true;
  std::string cmd;
  while (run && std::getline(std::cin, cmd))
  {
    run = (cmd != "stop");
  }

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
3. Go up a directory then into 'binancews' directory and run:   ```cmake .```
4. Then run: ```make```
5. The binary is in the 'binancews' sub-dir ('binancews/binancews' from the top level directory) 


# Run
This expects the Binance exchange websocket URI at wss://stream.binance.com:9443 .
This is unlikely to change, but if so it can be updated in BinanceExchange.hpp.
