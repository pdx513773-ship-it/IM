#pragma once

#include <brpc/channel.h>
#include <mutex>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include "./logger.hpp"

using ChannelPtr = std::shared_ptr<brpc::Channel>; 
class ServiceChannel
{
    public:
        using ptr = std::shared_ptr<ServiceChannel>;
        ServiceChannel(const std::string &service_name)
            :_service_name(service_name),_index(0)
        {

        }
        ~ServiceChannel(){}    
        //服务上线节点 新增信道  
        void append(const std::string &host)
        {
            auto channel = std::make_shared<brpc::Channel>();
            brpc::ChannelOptions options;
            options.connect_timeout_ms = -1; //连接不超时
            options.timeout_ms = -1; //rpc请求不超时
            options.max_retry = 3; //失败重试3次
            options.protocol = "baidu_std"; //使用baidu_std协议
            int ret = channel->Init(host.c_str(), &options);
            if(ret == -1)
            {
                LOG_ERROR("Failed to init channel");
                return ;
            }
            std::lock_guard<std::mutex> lock(_mutex);
            _channels.push_back(channel);
            _hosts[host] = channel;
        }
        //服务下线节点 释放信道
        void remove(const std::string &host)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _hosts.find(host);
            if(it == _hosts.end())
            {
                LOG_WARN("{}-{}节点删除信道时，没有该信道信息",_service_name,host);
                return;
            }
            for(auto vit = _channels.begin();vit!=_channels.end();++vit)
            {
                if(*vit == it->second)
                {
                    _channels.erase(vit);
                    break;
                }
            }
            _hosts.erase(it);
        }
        //轮转选择信道
        ChannelPtr choose() 
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_channels.empty()) return nullptr;
            size_t idx = static_cast<size_t>(_index++) % _channels.size();
            return _channels[idx];
        }
    private:
        std::mutex _mutex;
        int32_t _index;//轮转的计数器
        std::string _service_name;//服务名称
        std::vector<ChannelPtr> _channels;//当前服务对应的信道集合
        std::unordered_map<std::string,ChannelPtr> _hosts;//host到信道的映射
};


class ServiceManager
{
    public:
        using ptr = std::shared_ptr<ServiceManager>;
        ServiceManager(){}
        ~ServiceManager(){}

        ChannelPtr choose(const std::string &service_name)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto sit = _services.find(service_name);
            if(sit == _services.end())
            {
                LOG_ERROR("选择服务信道失败，没有找到对应的服务: {}", service_name);
                return nullptr;
            }
            return sit->second->choose();
        }
        //声明哪些是我关心的服务
        void declared(const std::string &service_name)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _follow_services.insert(service_name);
        }
   
        //服务上线节点 新增信道  
        void onServiceOnline(const std::string &service_instance,const std::string &host)
        {
            std::string service_name = getServiceName(service_instance);
            LOG_DEBUG("onServiceOnline: instance='{}' -> service='{}'", service_instance, service_name);
            ServiceChannel::ptr service;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto fit = _follow_services.find(service_name);
                if(fit == _follow_services.end())
                {
                    LOG_DEBUG("服务上线了该节点，不关心该服务: {}", service_name);
                    return;
                }
                auto sit = _services.find(service_name);
                if(sit == _services.end())  
                {
                    service = std::make_shared<ServiceChannel>(service_name);
                    _services[service_name] = service;
                }
                else
                {
                    service = sit->second;
                }
            }
            if(!service)
            {
                LOG_ERROR("服务上线节点，创建信道失败: {}", service_name);
                return;
            }
            service->append(host);
            

        }
        //服务下线节点 释放信道
        void onServiceOffline(const std::string &service_instance,const std::string &host)
        {            
            std::string service_name = getServiceName(service_instance);
            ServiceChannel::ptr service;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto fit = _follow_services.find(service_name);
                if(fit == _follow_services.end())
                {
                    LOG_DEBUG("服务下线了该节点，不关心该服务: {}", service_name);
                    return;
                }
                auto sit = _services.find(service_name);
                if(sit == _services.end())  
                {
                    LOG_WARN("删除{}服务节点时,没有找到管理对象",service_name);
                    return;
                }
                service = sit->second;
            }
            service->remove(host);

        }
    std::string getServiceName(const std::string &service_instance)
    {
        auto pos = service_instance.find_last_of("/");
        if(pos == std::string::npos)
            return service_instance;
        return service_instance.substr(0,pos);
    }
    private:
        std::mutex _mutex;
        std::unordered_set<std::string> _follow_services;
        std::unordered_map<std::string, ServiceChannel::ptr> _services;
};
