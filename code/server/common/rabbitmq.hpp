#include <ev.h>
#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#include <iostream>
#include <string>
#include "logger.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>

//

class MQClient
{
public:
    MQClient(const std::string &host, const std::string &user,
             const std::string &password)
    {
        _loop = EV_DEFAULT;
        _handler = std::make_shared<AMQP::LibEvHandler>(_loop);
        std::string url = "amqp://" + user + ":" + password + "@" + host + ":5672/";
        AMQP::Address address(url);
        _connection = std::make_shared<AMQP::TcpConnection>(_handler.get(), address);
        _channel = std::make_shared<AMQP::TcpChannel>(_connection.get());

        _loop_thread = std::thread([this]()
        {
            _async_watcher.data = this;
            ev_async_init(&_async_watcher,
                [](struct ev_loop *loop, ev_async *w, int /*revents*/)
                {
                    auto *self = static_cast<MQClient *>(w->data);
                    self->_connection->close();
                    ev_break(loop, EVBREAK_ALL);
                });
            ev_async_start(_loop, &_async_watcher);
            {
                std::lock_guard<std::mutex> lock(_startup_mutex);
                _async_ready = true;
            }
            _startup_cv.notify_one();
            ev_run(_loop, 0);
        });
    }
    void declareComponents(const std::string &exchange, const std::string &queue,
                           const std::string &routingKey, AMQP::ExchangeType exchange_type = AMQP::ExchangeType::direct)
    {
        _channel->declareExchange(exchange,exchange_type)
            .onSuccess([exchange]()
            {
                LOG_INFO("{} 交换机创建成功", exchange);
            })
            .onError([](const char *message)
            {
                LOG_ERROR("Declare exchange failed: {}", message);
                exit(0);
            });
        // 5.声明队列
        _channel->declareQueue(queue)
            .onSuccess([queue]()
            {
                LOG_INFO("{} 队列创建成功", queue);
            })
            .onError([](const char *message)
            {
                LOG_ERROR("Declare queue failed: {}", message);
                exit(0);
            });
        // 6.绑定交换机与队列
        _channel->bindQueue(exchange, queue, routingKey)
            .onSuccess([exchange, queue]()
            {
                LOG_INFO("{} - {} 绑定成功", exchange, queue);
            })
            .onError([exchange, queue](const char *message)
            {
                LOG_ERROR("{} - {} 绑定失败", exchange, queue);
                exit(0);
            });
    }

    bool publish(const std::string &exchange, const std::string &routingKey, const std::string &message)
    {
        bool ret = _channel->publish(exchange, routingKey, message);
        if(!ret)
        {
            LOG_ERROR("消息发布失败: {}", message);
        }
        return ret;
    }
    bool consume(const std::string &queue, const std::string &consumerTag)
    {
        _channel->consume(queue, consumerTag)
            .onReceived([](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
            {
                std::string msg(message.body(), message.bodySize());
                LOG_INFO("Received message: {}", msg);
            })
            .onError([](const char *message)
            {
                LOG_ERROR("Consume failed: {}", message);
                exit(0);
            });
        return true;
    }
~MQClient()
{
    if (_loop_thread.joinable())
    {
        std::unique_lock<std::mutex> lock(_startup_mutex);
        _startup_cv.wait(lock, [this]() { return _async_ready; });
        ev_async_send(_loop, &_async_watcher);
        lock.unlock();
        _loop_thread.join();
    }
    //ev_loop_destroy(_loop);
}
private:
    struct ev_loop *_loop;
    std::shared_ptr<AMQP::LibEvHandler> _handler;
    std::shared_ptr<AMQP::TcpConnection> _connection;
    std::shared_ptr<AMQP::TcpChannel> _channel;
    std::thread _loop_thread;
    ev_async _async_watcher;
    std::mutex _startup_mutex;
    std::condition_variable _startup_cv;
    bool _async_ready = false;
};