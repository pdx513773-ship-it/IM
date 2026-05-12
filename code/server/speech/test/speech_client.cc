#include "etcd.hpp"
#include <gflags/gflags.h>
#include "channel.hpp"
#include "speech.pb.h"
#include <thread>
#include "aip-cpp-sdk/speech.h"
// 进行服务发现 发现speech_server的服务器节点信息 并实例化信道通信
// 读取语音文件数据
// 调用ASRClient进行语音识别

using namespace IM;
DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "etcd地址");
DEFINE_string(basedir, "/service", "服务注册根目录");
DEFINE_string(speech_service, "/service/speech_service", "speech服务");

int main(int argc, char *argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    IM::init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);

    // 构造rpc信道  构造服务发现对象  通过信道获取echo服务的信道 调用echorpc的调用
    auto sm = std::make_shared<IM::ServiceManager>();
    sm->declared(FLAGS_speech_service);
    auto put_cb = std::bind(&IM::ServiceManager::onServiceOnline,
                            sm, std::placeholders::_1, std::placeholders::_2);
    auto del_cb = std::bind(&IM::ServiceManager::onServiceOffline,
                            sm, std::placeholders::_1, std::placeholders::_2);

    IM::Discovery::ptr dclient = std::make_shared<IM::Discovery>(FLAGS_etcd_host,
                                                         FLAGS_basedir, put_cb, del_cb);

    auto channel = sm->choose(FLAGS_speech_service);
    if (!channel)
    {
        LOG_ERROR("没有提供节点的主机");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return -1;
    }
    IM::SpeechService_Stub stub(channel.get());
    //读取文件数据
    std::string speech_content;
    aip::get_file_content("16k.pcm", &speech_content);
    // 进行rpc调用
    IM::SpeechRecognitionReq request;
    request.set_speech_content(speech_content);
    request.set_request_id("111111");

    brpc::Controller *cntl = new brpc::Controller();
    cntl->set_timeout_ms(10000); // 设置rpc请求超时时间为10s
    IM::SpeechRecognitionRsp *response = new IM::SpeechRecognitionRsp();
    stub.SpeechRecognition(cntl, &request, response, nullptr);
    if (cntl->Failed())
    {
        LOG_ERROR("RPC调用失败: {}", cntl->ErrorText());
        delete cntl;
        delete response;
        return -1;
    }
    if (!response->success())
    {
        LOG_ERROR("语音识别失败: {}", response->errmsg());
        LOG_ERROR("error code: {}", response->errmsg());
    }
    else
    {
        LOG_INFO("语音识别成功: {}", response->recognition_result());
        LOG_INFO("请求ID: {}", response->request_id());
        LOG_INFO("请求结果：{}", response->recognition_result());
    }

    return 0;
}