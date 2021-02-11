# Binance WebSockets
binancews is a C++ library which receives market data from the Binance crypto currency exchange. 

The project uses Microsoft's cpprestsdk for asynchronous websocket functionality to receive the market data which is then published to Redis using the RedisPlusPlus library.

These dependencies are handled by vcpkg, a cross platform package manager.


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
1. Build vcpkg: open shell in vcpkg_linux and run:  bootstrap-vcpkg.sh
2. Install dependencies: in the same prompt run:
```
.\vcpkg.exe install cpprestsdk[websockets]:x64-linux poco:x64-linux boost-asio:x64-linux redis-plus-plus[cxx17]:x64-linux
```
3. Go up a directory then into 'binancews' directory and run:   ```cmake .```
4. Then run: ```make```
5. The binary is in the 'binancews' sub-dir ('binancews/binancews' from the top level directory) 


## Run
1. Redis IP and port are set via command line args:   ./binancews [ip] [port]  , i.e. ```./binancews 192.168.10.10 6379``` 
2. This expects the Binance exchange websocket URI at wss://stream.binance.com:9443  :  Change in BinanceExchange.hpp
