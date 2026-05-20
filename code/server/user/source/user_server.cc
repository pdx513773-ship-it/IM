#include "user_server.hpp"

DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");



DEFINE_string(registry_host,"http://127.0.0.1:2379","etcd地址");
DEFINE_string(basedir,"/service","服务注册根目录");
DEFINE_string(instance_name,"/user_service/instance","实例名称");
DEFINE_string(access_host,"127.0.0.1:10003","外部访问地址");
DEFINE_int32(listen_port,10001,"Rpc服务器监听端口");
DEFINE_int32(idle_timeout,-1,"Rpc调用超时时间");
DEFINE_int32(rpc_threads,1,"Rpc调用线程数");

DEFINE_string(base_service,"/service","服务监控根目录");
DEFINE_string(file_service,"/service/file","文件管理子服务名称");

DEFINE_string(es_host,"127.0.0.1:9200","es地址");

DEFINE_string(mysql_user,"root","mysql用户名");
DEFINE_string(mysql_pswd,"tjmnb666","mysql密码");
DEFINE_string(mysql_host,"http://127.0.0.1:3306","mysql地址");
DEFINE_string(mysql_db,"IM","mysql数据库");
DEFINE_string(mysql_cset,"utf8","mysql字符集");
DEFINE_int32(mysql_port,3306,"mysql端口");
DEFINE_int32(mysql_conn_pool_count,3,"mysql连接池数量");

DEFINE_string(redis_host,"127.0.0.1","redis地址");
DEFINE_int32(redis_port,6379,"redis端口");
DEFINE_int32(redis_db,0,"redis数据库");
DEFINE_bool(redis_keep_alive,true,"redis连接是否保持活跃");



using namespace IM;

int main(int argc, char* argv[])
{
    // 解析命令行参数
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    // 初始化日志系统   
    IM::init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);

    IM::UserServerBuilder builder;
    builder.make_es_object({FLAGS_es_host});
    
    builder.make_dms_client(FLAGS_dms_access_key_id, FLAGS_dms_access_key_secret);

    builder.make_mysql_object(FLAGS_mysql_user, 
        FLAGS_mysql_pswd, FLAGS_mysql_host, FLAGS_mysql_db, 
        FLAGS_mysql_cset, FLAGS_mysql_port, FLAGS_mysql_conn_pool_count);

    builder.make_redis_object(FLAGS_redis_host, 
        FLAGS_redis_port, FLAGS_redis_db, FLAGS_redis_keep_alive);

    builder.make_discovery_object(FLAGS_registry_host, 
        FLAGS_basedir, FLAGS_instance_name);

    builder.make_reg_object(FLAGS_registry_host, 
        FLAGS_instance_name, FLAGS_access_host);
        
    builder.make_rpc_server(FLAGS_listen_port, FLAGS_idle_timeout, FLAGS_rpc_threads);
    
    auto server = builder.build();
    server->start();

    return 0;
}
