#include <sw/redis++/redis++.h>
#include <gflags/gflags.h>
#include <iostream>
#include <memory>
#include <thread>
#include "logger.hpp"
namespace IM
{
    class RedisClientFactory
    {
        public:
            static std::shared_ptr<sw::redis::Redis> create(const std::string &host, 
                int port, int db, bool keep_alive)
            {
                sw::redis::ConnectionOptions opts;
                opts.host = host;
                opts.port = port;
                opts.db = db;
                opts.keep_alive = keep_alive;

                auto client = std::make_shared<sw::redis::Redis>(opts);
                return client;
            }
    };
    class Session
    {
        public:
            Session(const std::shared_ptr<sw::redis::Redis> &redis_client):
                _redis_client(redis_client){}
                
            void append(const std::string &ssid,const std::string &user_id)
            {
                _redis_client->set(ssid,user_id);
            }
            void remove(const std::string &ssid)
            {
                _redis_client->del(ssid);
            }
            sw::redis::Optional<std::string> uid(const std::string &ssid)
            {
                return _redis_client->get(ssid);
            }



        private:
            std::shared_ptr<sw::redis::Redis> _redis_client;

    };
    class Status
    {
        public:
            Status(const std::shared_ptr<sw::redis::Redis> &redis_client):
                _redis_client(redis_client){}
            void append(const std::string &user_id)
            {
                _redis_client->set(user_id,"");
            }
            void remove(const std::string &user_id)
            {
                _redis_client->del(user_id);
            }

            bool exists(const std::string &user_id)
            {
                auto result = _redis_client->get(user_id);
                if(result)
                {
                    return true;
                }
                return false;
            }

        private:
            std::shared_ptr<sw::redis::Redis> _redis_client;
    };

    class Codes
    {
        public:
            Codes(const std::shared_ptr<sw::redis::Redis> &redis_client):
                _redis_client(redis_client){}
            void append(const std::string &cid,const std::string &code,
                const std::chrono::milliseconds &expire_time = std::chrono::milliseconds(60000))
            {
                _redis_client->set(cid,code,expire_time);
            }
            void remove(const std::string &cid)
            {
                _redis_client->del(cid);
            }
            sw::redis::Optional<std::string> code(const std::string &cid)
            {
                return _redis_client->get(cid);
            }
        private:
            std::shared_ptr<sw::redis::Redis> _redis_client;
    };
}