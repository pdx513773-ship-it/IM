#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include<spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <iostream>




std::shared_ptr<spdlog::logger> g_default_logger;
//mode 运行模式 true - 发布模式 false - 调试模式
void init_log(bool mode,const std::string& log_file_path,int level)
{
    if(spdlog::get("default-logger"))
    {
        return;
    }
    //调试模式 输出等级调为最低
    if(!mode)
    {
        g_default_logger = spdlog::stdout_color_mt("default-logger");//创建同步的日志记录器
        g_default_logger->set_level(spdlog::level::trace);//设置日志输出等级为trace
        g_default_logger->flush_on(spdlog::level::trace);//遇到trace以上立即刷新
    }
    //发布模式
    else
    {   
        g_default_logger = spdlog::basic_logger_mt<spdlog::async_factory>("default-logger", log_file_path);//创建异步的日志记录器
        g_default_logger->set_level(spdlog::level::level_enum(level));//设置日志输出等级
        g_default_logger->flush_on(spdlog::level::level_enum(level));//遇到level以上立即刷新
    }
    g_default_logger->set_pattern("[%n][%H:%M:%S][%t][%-8l] %v ");//设置日志输出格式
}
#define LOG_TRACE(format, ...) g_default_logger->trace("[{}:{}] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) g_default_logger->debug("[{}:{}] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(format, ...) g_default_logger->info("[{}:{}] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(format, ...) g_default_logger->warn("[{}:{}] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) g_default_logger->error("[{}:{}] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_CRITICAL(format, ...) g_default_logger->critical("[{}:{}] " format, __FILE__, __LINE__, ##__VA_ARGS__)  