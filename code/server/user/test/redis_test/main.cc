#include "../../../common/data_redis.hpp"

#include <gflags/gflags.h>

using namespace IM;

DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");

DEFINE_string(redis_host, "127.0.0.1", "Redis server host");
DEFINE_int32(redis_port, 6379, "Redis server port");
DEFINE_int32(db,0,"库的编号");
DEFINE_bool(keep_alive,true,"是否进行长连接保活");

void session_test(const std::shared_ptr<sw::redis::Redis>& redis_client)
{
    IM::Session ss(redis_client);
    ss.append("ssid1","user1");
    ss.append("ssid2","user2");
    ss.append("ssid3","user3");
    ss.append("ssid4","user4");
    ss.append("ssid5","user5");
    
    ss.remove("ssid3");

    auto uid = ss.uid("ssid2");
    if(uid)
    {
        LOG_INFO("ssid2对应的用户id是: {}", *uid);
    }
    else
    {
        LOG_INFO("ssid2没有对应的用户id");
    }
    uid = ss.uid("ssid3");
    if(uid)    {
        LOG_INFO("ssid3对应的用户id是: {}", *uid);
    }
    else
    {
        LOG_INFO("ssid3没有对应的用户id");
    }
    
}
void status_test(const std::shared_ptr<sw::redis::Redis>& redis_client)
{
    IM::Status st(redis_client);
    st.append("user1");
    st.append("user2");
    st.append("user3");
    st.append("user4");
    st.append("user5");
    
    st.remove("user3");

    auto res = st.exists("user2");
    if(res)
    {
        LOG_INFO("user2在线");
    }
    else
    {
        LOG_INFO("user2不在线");
    }
    res = st.exists("user3");
    if(res)    
    {
        LOG_INFO("user3在线");
    }
    else
    {
        LOG_INFO("user3不在线");
    }
    
}
void code_test(const std::shared_ptr<sw::redis::Redis>& redis_client)
{
    IM::Codes cd(redis_client);
    cd.append("cid1","code1",std::chrono::seconds(60));
    cd.append("cid2","code2",std::chrono::seconds(60));
    cd.append("cid3","code3",std::chrono::seconds(60));
    cd.append("cid4","code4",std::chrono::seconds(60));
    cd.append("cid5","code5",std::chrono::seconds(60));
    
    cd.remove("cid3");

    auto res = cd.code("cid2");
    if(res)
    {
        LOG_INFO("cid2对应的验证码是: {}", *res);
    }
    else
    {
        LOG_INFO("cid2没有对应的验证码");
    }
    res = cd.code("cid3");
    if(res)    
    {
        LOG_INFO("cid3对应的验证码是: {}", *res);
    }
    else
    {
        LOG_INFO("cid3没有对应的验证码");
    }
}
int main(int argc, char *argv[])
{
   
    google::ParseCommandLineFlags(&argc, &argv, true);

    IM::init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);
    auto redis_client = IM::RedisClientFactory::create(FLAGS_redis_host, FLAGS_redis_port, FLAGS_db, FLAGS_keep_alive);
    session_test(redis_client);
    status_test(redis_client);
    code_test(redis_client);
    return 0;
}