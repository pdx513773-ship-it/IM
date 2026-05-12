#include "speech_server.hpp"

DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");


DEFINE_string(app_id, "122673271", "百度云 APP ID");
DEFINE_string(api_key,"NQhkdwm8IRNw5wRtPBIvEW6p","百度云 API Key");
DEFINE_string(secret_key,"L4bsXTNC84CK3Wt8Nsd0h7lsU5FkRzA4","百度云 Secret Key");

DEFINE_string(registry_host,"http://127.0.0.1:2379","etcd地址");
DEFINE_string(basedir,"/service","服务注册根目录");
DEFINE_string(instance_name,"/speech_service/instance","实例名称");
DEFINE_string(access_host,"127.0.0.1:10001","外部访问地址");
DEFINE_int32(listen_port,10001,"Rpc服务器监听端口");
DEFINE_int32(idle_timeout,-1,"Rpc调用超时时间");
DEFINE_int32(rpc_threads,1,"Rpc调用线程数");

using namespace IM;

int main(int argc, char* argv[])
{
    // 解析命令行参数
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    // 初始化日志系统   
    IM::init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);

    IM::SpeechServerBuilder builder;
    builder.make_asr_object(FLAGS_app_id, FLAGS_api_key, FLAGS_secret_key);
    builder.make_rpc_server(FLAGS_listen_port, FLAGS_idle_timeout, FLAGS_rpc_threads);
    builder.make_reg_object(FLAGS_registry_host, 
        FLAGS_basedir + FLAGS_instance_name, FLAGS_access_host);
    auto server = builder.build();
    server->start();

    return 0;
}
