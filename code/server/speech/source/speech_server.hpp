// 实现语音识别子服务

#include "asr.hpp"
#include "logger.hpp"
#include "etcd.hpp"
#include "speech.pb.h"
#include <brpc/server.h>

namespace IM
{
    class SpeechServerImpl : public IM::SpeechService
    {
    public:
        SpeechServerImpl(const ASRClient::ptr &asr_client) : _asr_client(asr_client){}
        ~SpeechServerImpl() {}
        void SpeechRecognition(google::protobuf::RpcController *controller,
                               const IM:: SpeechRecognitionReq *request,
                               IM::SpeechRecognitionRsp *response,
                               google::protobuf::Closure *done)                      
        {
            brpc::ClosureGuard rpc_guard(done);
            //取出语音内容，调用ASRClient进行识别，得到响应
            std::string err;
            std::string res = _asr_client->recognize(request->speech_content(), err);
            if (res.empty())
            {
                response->set_success(false);
                response->set_errmsg(err + "语音识别失败");
                response->set_request_id(request->request_id());
                return;
            }
            //组织响应
            response->set_success(true);
            response->set_request_id(request->request_id());
            response->set_recognition_result(res);
        }

    private:
        ASRClient::ptr _asr_client;
    };

    class SpeechServer
    {
    public:
        using ptr = std::shared_ptr<SpeechServer>;
        SpeechServer(ASRClient::ptr asr_client, Registry::ptr reg_client,
                     const std::shared_ptr<brpc::Server> &server) :
            _asr_client(asr_client), _reg_client(reg_client), _rpc_server(server)
        {
        }
        ~SpeechServer()
        {
        }
        void start()
        {
            _rpc_server->RunUntilAskedToQuit();
        }
    private:
        ASRClient::ptr _asr_client;
        Registry::ptr _reg_client;
        std::shared_ptr<brpc::Server> _rpc_server;
    };
    class SpeechServerBuilder
    {
    public:
        void make_asr_object(const std::string &app_id,
                             const std::string &api_k, const std::string &secret_key)
        {
            _asr_client = std::make_shared<ASRClient>(app_id, api_k, secret_key);
        }

        void make_reg_object(const std::string &reg_host,const std::string &service_name
            ,const std::string &access_host)
        {
            // 注册中心 注册地址和访问地址
            _reg_client = std::make_shared<Registry>(reg_host);
            _reg_client->registry(service_name, access_host);
        }
        // 搭建RPC服务器，并启动
        void make_rpc_server(uint16_t port, uint32_t timeout = -1, uint8_t num_threads = 1)
        {
            if(_asr_client == nullptr)
            {
                LOG_ERROR("ASR client is not initialized");
                abort();
            }
            _rpc_server = std::make_shared<brpc::Server>();
            auto speech_service = new SpeechServerImpl(_asr_client);
            int ret = _rpc_server->AddService(speech_service, 
                brpc::SERVER_OWNS_SERVICE);//SERVER_OWNS_SERVICE表示服务器会负责删除这个服务对象
            if(ret == -1)
            {
                LOG_ERROR("Failed to add service");
                abort();
            }
            brpc::ServerOptions options;
            options.idle_timeout_sec = 0; //连接不超时
            options.max_concurrency = num_threads; //最大并发数
            ret = _rpc_server->Start(port, &options);
            if (ret != 0)
            {
                LOG_ERROR("Failed to start RPC server");
                abort();
            }
            LOG_INFO("RPC server started on port {}", port);
        }
        SpeechServer::ptr build()
        {
            if(!_asr_client)
            {
                LOG_ERROR("ASR client is not initialized");
                abort();
            }
            if(!_reg_client)
            {
                LOG_ERROR("Registry client is not initialized");
                abort();
            }
            if(!_rpc_server)
            {
                LOG_ERROR("RPC server is not initialized");
                abort();
            }
            SpeechServer::ptr server = std::make_shared<SpeechServer>
            (_asr_client, _reg_client, _rpc_server);
            return server;
        }
    private:
        ASRClient::ptr _asr_client;
        Registry::ptr _reg_client;
        std::shared_ptr<brpc::Server> _rpc_server;
    };
}
