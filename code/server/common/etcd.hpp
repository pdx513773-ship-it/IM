#pragma once

#include "etcd/Client.hpp"
#include "etcd/Response.hpp"
#include "etcd/KeepAlive.hpp"
#include "etcd/Watcher.hpp"
#include "etcd/Value.hpp"
#include "../common/logger.hpp"
#include <thread>
#include <iostream>
#include <functional>

// 服务注册客服端类
class Registry
{
public:
    using ptr = std::shared_ptr<Registry>;
    Registry(const std::string &etcd_host) : _client(std::make_shared<etcd::Client>(etcd_host)),
                                             _keep_alive(_client->leasekeepalive(3).get()),
                                             _lease_id(_keep_alive->Lease())
    {
    }
    bool registry(const std::string &key, const std::string &val)
    {
        auto resp1 = _client->put(key, val, _lease_id).get();
        if (!resp1.is_ok())
        {
            LOG_ERROR("注册失败:{}", resp1.error_message());
            return false;
        }
        LOG_DEBUG("注册成功: key={}, val={}", key, val);
        return true;
    }

    ~Registry()
    {
        _keep_alive->Cancel();
    }

private:
    std::shared_ptr<etcd::Client> _client;
    std::shared_ptr<etcd::KeepAlive> _keep_alive;
    uint64_t _lease_id;
};

// 服务发现客户端
class Discovery
{
public:
    using ptr = std::shared_ptr<Discovery>;
    using NotifyCallback = std::function<void(std::string const &, std::string const &)>;
    Discovery(const std::string &etcd_host, const std::string &basedir,
    const NotifyCallback &put_cb, const NotifyCallback &del_cb) :
    _client(std::make_shared<etcd::Client>(etcd_host)),
    _put_cb(put_cb),
    _del_cb(del_cb)
    {
        //先进行服务发现，获取到已有的数据
        auto resp = _client->ls(basedir).get();
        if(!resp.is_ok())
        {
            LOG_ERROR("服务发现失败:{}", resp.error_message());
            return;
        }
        int sz = resp.keys().size();
        for(int i=0; i<sz; ++i)
        {
            if(_put_cb)
                _put_cb(resp.keys()[i], resp.values()[i].as_string());
        }
        //监听服务变化 
        _watcher = std::make_shared<etcd::Watcher>(*_client.get(), basedir,
            std::bind(&Discovery::callback, this, std::placeholders::_1),true);
        LOG_DEBUG("服务发现初始化完成");
    }
private:
    void callback(const etcd::Response& resp)
    {
        if(!resp.is_ok())
        {
            LOG_ERROR("监听服务变化失败:{}", resp.error_message());
            return;
        }
        for(auto const & event : resp.events())
        {
            if(event.event_type() == etcd::Event::EventType::PUT)
            {
                LOG_DEBUG("服务信息发生了改变: key={}, val={}", event.kv().key(), event.kv().as_string());
                if(_put_cb)
                    _put_cb(event.kv().key(), event.kv().as_string());
            }
            else if(event.event_type() == etcd::Event::EventType::DELETE_)
            {
                LOG_DEBUG("服务信息被删除: key={}, val={}", event.prev_kv().key(), event.prev_kv().as_string());
                if(_del_cb)
                    _del_cb(event.prev_kv().key(), event.prev_kv().as_string());
            }
        }
    }
private:
    NotifyCallback _put_cb;
    NotifyCallback _del_cb;
    std::shared_ptr<etcd::Client> _client;
    std::shared_ptr<etcd::Watcher> _watcher;
};

