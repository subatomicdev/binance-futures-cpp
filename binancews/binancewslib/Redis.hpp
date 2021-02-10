#pragma once

#include <sw/redis++/redis.h>
#include <functional>
#include "Logger.hpp"


namespace binancews
{
    using std::shared_ptr;
    using std::string;


    class Redis
    {
    public:
        Redis()
        {

        }


        void init(const std::string& ip, const int port)
        {
            sw::redis::ConnectionPoolOptions poolOptions;
            poolOptions.size = std::thread::hardware_concurrency();

            sw::redis::ConnectionOptions connOptions;
            connOptions.port = port;
            connOptions.host = ip;
            connOptions.type = sw::redis::ConnectionType::TCP;

            m_redis = std::make_shared<sw::redis::Redis>(connOptions, poolOptions);            
        }


        void set(const std::string& k, const std::string& v)
        {
            try
            {
                m_redis->set(k, v);
            }
            catch (std::exception ex)
            {
                logg(ex.what());
            }            
        }

        void publish(const string& channel, const string& msg)
        {
            m_redis->publish(channel, msg); // TODO return value useful?
        }

        void publish(const string& channel, string&& msg)
        {
            m_redis->publish(channel, msg); // TODO return value useful?
        }

    private:
        shared_ptr<sw::redis::Redis> m_redis;
    };
}
