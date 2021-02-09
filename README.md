# Binance WebSockets
binancews is a C++ library which receives market data from the Binance crypto currency exchange. 

The project uses Microsoft's cpprestsdk for asynchronous websocket functionality to receive the market data which is then published to Redis using the RedisPlusPlus library.

These dependencies are handled by vcpkg, a cross platform package manager.

**Note**
1. This expects a Redis server on 127.22.253.65  :  Change in bincancews.cpp 
2. This expects the Binance exchange websocket URI at wss://stream.binance.com:9443  :  Change in BinanceExchange.hpp

## Build

### Windows
1. Build vcpkg: open a command prompt in vcpkg_win and run:   bootstrap-vcpkg.bat
2. Install dependencies: in the same prompt run:
```
   .\vcpkg.exe install cpprestsdk[websockets]:x64-windows-static poco:x64-windows-static boost-asio:x64-windows-static redis-plus-plus[cxx17]:x64-windows-static
```
3. Open the VS solution binancews/binancews.sln
4. Change to 'Release', right-click on the 'binancews' project and select "Setup as startup project"
5. Run


### Linux
Coming soon
