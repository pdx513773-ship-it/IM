//实现项目中公共的工具类接口
//1.生成唯一ID的接口
//2.文件的读写操作接口
#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <atomic>
#include <random>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "logger.hpp"
namespace IM
{
    std::string uuid()
    {
        //生成一个16位随机字符串组成的唯一ID
        //1.生成6个 0到255之间的随机数（1字节-转换为16进制字符）
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        std::stringstream ss;
        for(int i = 0; i < 6; i++)
        {
            if(i == 2) ss<<"-";
            ss<<std::setw(2)<<std::setfill('0')<<std::hex<<dis(gen);
        }
        ss<<"-";
        //2.用静态变量生成2字节编号数字--生成4位16进制数字字符
        static std::atomic<int> id(0);
        short tmp = id.fetch_add(1);
        ss<<std::setw(4)<<std::setfill('0')<<std::hex<<tmp;
        return ss.str();
    }
    std::string vcode()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 9);

        std::stringstream ss;
        for(int i = 0; i < 4; i++)
        {
            ss<<dis(gen);
        }
        return ss.str();
    }
    bool readFile(const std::string& filename, std::string& content)
    {
        std::ifstream file(filename,std::ios::binary | std::ios::in);
        if(!file.is_open())
        {
            LOG_ERROR("Failed to open file: %s", filename.c_str());
            file.close();
            return false;
        }
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        content.resize(fileSize);
        file.read(&content[0], fileSize);
        if(file.good() == false)
        {
            LOG_ERROR("Failed to read file: %s", filename.c_str());
            file.close();
            return false;
        }        
        file.close();
        return true;
    }
    bool writeFile(const std::string& filename, const std::string& content)
    {
        std::ofstream file(filename,std::ios::binary | std::ios::trunc);
        if(!file.is_open())
        {
            LOG_ERROR("Failed to open file: %s", filename.c_str());
            file.close();
            return false;
        }
        file.write(content.c_str(), content.size());
        if(file.good() == false)
        {
            LOG_ERROR("Failed to write file: %s", filename.c_str());
            file.close();
            return false;
        }
        file.close();
        return true;
    }
}