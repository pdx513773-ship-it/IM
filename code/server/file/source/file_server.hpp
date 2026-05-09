#include "asr.hpp"
#include "logger.hpp"
#include "etcd.hpp"
#include "file.pb.h"
#include "base.pb.h"
#include <brpc/server.h>
#include "utils.hpp"
//实现文件存储子服务
//1.实现文件rpc服务，提供文件上传和下载接口
//2.实现文件存储接口，提供文件的存储和读取功能
//3.实现文件子服务的构造者模式


namespace IM
{
    class FileServerImpl : public IM::FileService
    {
        public:
            FileServerImpl() {}
            ~FileServerImpl() {}
            void GetSingleFile(google::protobuf::RpcController *controller,
                               const IM::GetSingleFileReq *request,
                               IM::GetSingleFileRsp *response,
                               google::protobuf::Closure *done)                      
            {
                brpc::ClosureGuard rpc_guard(done);
                response->set_request_id(request->request_id());
                //取出请求中的文件ID(文件名)
                std::string file_id = request->file_id();
                //从文件系统中读取文件内容
                std::string file_content;
                bool ret = readFile(file_id,file_content);
                if(ret == false)
                {
                    response->set_success(false);
                    response->set_errmsg("文件读取失败");
                    LOG_ERROR("文件读取失败");
                    return;
                }
                //组织响应
                response->set_success(true);
                response->mutable_file_data()->set_file_id(file_id);
                response->mutable_file_data()->set_file_content(file_content);
                LOG_INFO("文件读取成功，文件ID：{}", file_id);
            }
            void GetMultiFile(google::protobuf::RpcController *controller,
                               const IM::GetMultiFileReq *request,
                               IM::GetMultiFileRsp *response,
                               google::protobuf::Closure *done)                      
            {
                brpc::ClosureGuard rpc_guard(done);
                response->set_request_id(request->request_id());
                //循环取出请求中的文件ID 读取并组织响应
                for(int i = 0;i < request->file_id_list_size();i++)
                {
                    std::string file_id = request->file_id_list(i);
                    std::string file_content;
                    bool ret = readFile(file_id,file_content);
                     if(ret == false)
                    {
                        response->set_success(false);
                        response->set_errmsg("文件读取失败");
                        LOG_ERROR("文件读取失败，文件ID：{}", file_id);
                        continue;
                    }
                    FileDownloadData data;
                    data.set_file_id(file_id);
                    data.set_file_content(file_content);
                    response->mutable_file_data()->insert({file_id, data});
                }
                response->set_success(true);
                LOG_INFO("文件批量读取成功，文件数量：{}", request->file_id_list_size());
            }
            
            void PutSingleFile(google::protobuf::RpcController *controller,
                               const IM::PutSingleFileReq    *request,
                               IM::PutSingleFileRsp *response,
                               google::protobuf::Closure *done)                      
            {
                brpc::ClosureGuard rpc_guard(done);
                //为文件生成一个uuid作为文件名
                //取出请求中的文件数据 进行数据写入
                //组织响应
                response->set_request_id(request->request_id());
                std::string file_id = uuid();
                bool ret = writeFile(file_id,request->file_data().file_content());
                if(ret == false)
                {
                    response->set_success(false);
                    response->set_errmsg("文件写入失败");
                    LOG_ERROR("文件写入失败");
                    return;
                }
                response->mutable_file_info()->set_file_id(file_id);
                response->mutable_file_info()->set_file_size(request->file_data().file_content().size());
                response->mutable_file_info()->set_file_name(request->file_data().file_name());
                response->set_success(true);
                LOG_INFO("文件写入成功，文件ID：{}", file_id);
                
            }
            void PutMultiFile(google::protobuf::RpcController *controller,
                               const IM::PutMultiFileReq *request,
                               IM::PutMultiFileRsp *response,
                               google::protobuf::Closure *done)                      
            {
                brpc::ClosureGuard rpc_guard(done);
                response->set_request_id(request->request_id());
                //取出请求中的文件数据
                for(int i = 0;i < request->file_data_size();i++)
                {
                    std::string file_id = uuid();
                    bool ret = writeFile(file_id,request->file_data(i).file_content());
                    if(ret == false)
                    {
                        LOG_ERROR("文件写入失败，文件ID：{}", file_id);
                        continue;
                    }
                    FileMessageInfo* info = response->add_file_info();
                    info->set_file_id(file_id);
                    info->set_file_size(request->file_data(i).file_size());
                    info->set_file_name(request->file_data(i).file_name());
                }
                response->set_success(true);
                LOG_INFO("文件批量写入成功，文件数量：{}", request->file_data_size());
            }

        private:
    };

    class FileServer
    {
    public:
        using ptr = std::shared_ptr<FileServer>;
        FileServer(Registry::ptr reg_client,const std::shared_ptr<brpc::Server> &server) :_reg_client(reg_client), _rpc_server(server) {}
        ~FileServer() {}
        void start()
        {
            _rpc_server->RunUntilAskedToQuit();
        }
    private:
        Registry::ptr _reg_client;
        std::shared_ptr<brpc::Server> _rpc_server;
    };
    class FileServerBuilder
    {
    public:
        
        void make_reg_object(const std::string &reg_host,const std::string &service_name
            ,const std::string &access_host)
        {
            // 注册中心 注册地址和访问地址
            _reg_client = std::make_shared<Registry>(reg_host);
            _reg_client->registry(service_name, access_host);
        }
        void make_rpc_server(uint16_t port, uint32_t timeout = -1, uint8_t num_threads = 1)
        {
            _rpc_server = std::make_shared<brpc::Server>();
            auto file_service = new FileServerImpl();
            int ret = _rpc_server->AddService(file_service, 
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
        FileServer::ptr build()
        {
            if(!_rpc_server)
            {
                LOG_ERROR("RPC server is not initialized");
                abort();
            }
            FileServer::ptr server = std::make_shared<FileServer>(_reg_client, _rpc_server);
            return server;
        }
    private:
        Registry::ptr _reg_client;
        std::shared_ptr<brpc::Server> _rpc_server;
};
}