#include "etcd.hpp"
#include <gflags/gflags.h>
#include "channel.hpp"
#include "file.pb.h"
#include "base.pb.h"
#include "utils.hpp"  // 添加 readFile/writeFile
#include <iostream>
#include <gtest/gtest.h>

using namespace IM;

DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");
DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd地址");
DEFINE_string(basedir, "/service", "服务注册根目录");
DEFINE_string(file_service, "/service/file_service", "file服务");

ChannelPtr channel;

TEST(FileTest, put_and_get_single_file) {
    // 1. 上传文件
    std::string file_content;
    ASSERT_TRUE(readFile("./Makefile", file_content));
    
    FileService_Stub stub(channel.get());
    PutSingleFileReq put_req;
    put_req.set_request_id("111");
    put_req.mutable_file_data()->set_file_name("Makefile");
    put_req.mutable_file_data()->set_file_size(file_content.size());
    put_req.mutable_file_data()->set_file_content(file_content);
    
    brpc::Controller put_cntl;
    put_cntl.set_timeout_ms(10000);
    PutSingleFileRsp put_rsp;
    stub.PutSingleFile(&put_cntl, &put_req, &put_rsp, nullptr);
    
    ASSERT_FALSE(put_cntl.Failed()) << "RPC调用失败: " << put_cntl.ErrorText();
    ASSERT_TRUE(put_rsp.success()) << "文件上传失败: " << put_rsp.errmsg();
    ASSERT_EQ(put_rsp.file_info().file_size(), file_content.size());
    ASSERT_EQ(put_rsp.file_info().file_name(), "Makefile");
    
    std::string file_id = put_rsp.file_info().file_id();
    LOG_DEBUG("文件ID：{}", file_id);
    
    // 2. 下载文件
    GetSingleFileReq get_req;
    get_req.set_request_id("222");
    get_req.set_file_id(file_id);
    
    brpc::Controller get_cntl;
    get_cntl.set_timeout_ms(10000);
    GetSingleFileRsp get_rsp;
    stub.GetSingleFile(&get_cntl, &get_req, &get_rsp, nullptr);
    
    ASSERT_FALSE(get_cntl.Failed()) << "RPC调用失败: " << get_cntl.ErrorText();
    ASSERT_TRUE(get_rsp.success()) << "文件下载失败: " << get_rsp.errmsg();
    ASSERT_EQ(file_id, get_rsp.file_data().file_id());
    ASSERT_EQ(file_content, get_rsp.file_data().file_content());
    
    // 3. 保存到本地
    ASSERT_TRUE(writeFile("single_file_download.txt", get_rsp.file_data().file_content()));
}

int main(int argc, char *argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);
    
    // 初始化 RPC 客户端
    auto sm = std::make_shared<ServiceManager>();
    sm->declared(FLAGS_file_service);
    auto put_cb = std::bind(&ServiceManager::onServiceOnline,
                            sm, std::placeholders::_1, std::placeholders::_2);
    auto del_cb = std::bind(&ServiceManager::onServiceOffline,
                            sm, std::placeholders::_1, std::placeholders::_2);
    
    Discovery::ptr dclient = std::make_shared<Discovery>(FLAGS_etcd_host,
                                                         FLAGS_basedir, put_cb, del_cb);
    
    channel = sm->choose(FLAGS_file_service);
    if (!channel) {
        LOG_ERROR("没有提供节点的主机");
        return -1;
    }
    
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}