# Binance WebSockets
binancews is a C++ library which receives market data from the Binance crypto currency exchange. 

The project uses Microsoft's cpprestsdk for asynchronous websocket functionality to receive the market data which is then published to Redis using the RedisPlusPlus library.

These dependencies are handled by vcpkg, a cross platform package manager.
