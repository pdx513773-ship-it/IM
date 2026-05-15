#include "../../../common/data_es.hpp"
#include <gflags/gflags.h>  



DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");

int main(int argc, char *argv[])  
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    IM::init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);
    std::vector<std::string> hosts = {"http://127.0.0.1:9200"};
    auto es = IM::ESClientFactory::create(hosts);
    auto es_user = std::make_shared<IM::ESUser>(es);
    es_user->createIndex();
    es_user->appendData("001", "12345678901", "张三", "这是张三的个人简介", "avatar001");
    es_user->appendData("002", "12345678902", "李四", "这是李四的个人简介", "avatar002");
    es_user->appendData("003", "12345678903", "王五", "这是王五的个人简介", "avatar003");
    es_user->appendData("004", "12345678904", "赵六", "这是赵六的个人简介", "avatar004");
    es_user->appendData("005", "12345678905", "钱七", "这是钱七的个人简介", "avatar005");
    
    auto res = es_user->search("12345678902", {"001"});
    for(auto& item : res)
    {
        IM::LOG_INFO("ES搜索结果: user_id={}, phone={}, nickname={}, description={}, avatar_id={}",
            item.user_id(), item.phone(), item.nickname(), item.description(), item.avatar_id());
    }


    return 0;
}