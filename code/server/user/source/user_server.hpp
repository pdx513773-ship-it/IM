// 实现语音识别子服务

#include "data_es.hpp"
#include "data_mysql.hpp"
#include "data_redis.hpp"

#include "logger.hpp"
#include "etcd.hpp"
#include "user.pb.h"
#include "base.pb.h"
#include "file.pb.h"
#include "channel.hpp"
#include "user-odb.hxx"
#include "dms.hpp"
#include "user.hxx"
#include "../../../common/utils.hpp"
#include <brpc/server.h>

namespace IM
{
    class UserServerImpl : public IM::UserService
    {
    public:
        UserServerImpl(const std::shared_ptr<elasticlient::Client> &es_client,
                       const std::shared_ptr<odb::core::database> &mysql_client,
                       const std::shared_ptr<sw::redis::Redis> &redis_client,
                       const ServiceManager::ptr &channel_manager,
                       const std::string &file_service_name,
                       const DMSClient::ptr &dms_client)
            : _es_user(std::make_shared<ESUser>(es_client)),
              _mysql_user(std::make_shared<UserTable>(mysql_client)),
              _redis_session(std::make_shared<Session>(redis_client)),
              _redis_login_status(std::make_shared<Status>(redis_client)),
              _redis_codes(std::make_shared<Codes>(redis_client)),
              _channel_manager(channel_manager),
              _file_service_name(file_service_name),
              _dms_client(dms_client)
        {
        }

        ~UserServerImpl() {}

        bool check_nickname(const std::string &nickname)
        {
            if (nickname.length() < 1 || nickname.length() > 22)
            {
                return false;
            }
            for (char c : nickname)
            {
                if (!std::isalnum(c) && c != '~' && c != '_')
                {
                    return false;
                }
            }
            return true;
        }

        bool check_password(const std::string &password)
        {
            if (password.length() < 6 || password.length() > 15)
            {
                LOG_ERROR("密码长度不合法");
                return false;
            }
            for (char c : password)
            {
                if (!std::isalnum(c))
                {
                    LOG_ERROR("密码包含非法字符");
                    return false;
                }
            }
            return true;
        }

        virtual void UserRegister(::google::protobuf::RpcController *controller,
                                  const ::IM::UserRegisterReq *request,
                                  ::IM::UserRegisterRsp *response,
                                  ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
            // 定义错误处理函数 每一个接口响应结构不同 所以我们在接口内部封装错误处理函数
            auto err_handler = [this, response](const std::string &rid,
                                                const std::string &errmsg)
            {
                response->set_request_id(rid);
                response->set_errmsg(errmsg);
                response->set_success(false);
            };
            // 用户注册逻辑
            // 1.从用户请求中获取注册信息 取出昵称和密码
            std::string nickname = request->nickname();
            std::string password = request->password();
            // 2.检查昵称是否合法（只能包含字母数字 ~ _ 长度限制3~15个字符）
            if (!check_nickname(nickname))
            {
                LOG_ERROR("昵称不合法: - {}", request->request_id());
                err_handler(request->request_id(), "昵称不合法");
                return;
            }
            // 3.检查密码是否合法（只能包含字母数字 长度限制6~15个字符）
            if (!check_password(password))
            {
                LOG_ERROR("密码不合法: - {}", request->request_id());
                err_handler(request->request_id(), "密码不合法");
                return;
            }
            // 4.检查用户是否已经注册过
            auto user = _mysql_user->select_by_nickname(nickname);
            if (user != nullptr)
            {
                LOG_ERROR("用户已存在: - {}", request->request_id());
                err_handler(request->request_id(), "用户已存在");
                return;
            }
            // 5.如果没有注册过 则将用户信息存储到数据库中
            std::string user_id = uuid();
            user = std::make_shared<User>(user_id, nickname, password);
            if (!_mysql_user->insert(user))
            {
                LOG_ERROR("用户注册失败: - {}", request->request_id());
                err_handler(request->request_id(), "用户注册失败");
                return;
            }
            // 6. 向es中添加用户信息 以便后续搜索使用
            if (!_es_user->appendData(user_id, "", nickname, "", ""))
            {
                LOG_ERROR("用户信息添加到ES失败: - {}", request->request_id());
                err_handler(request->request_id(), "用户信息添加到ES失败");
                return;
            }
            // 7.注册成功 返回成功响应
            response->set_request_id(request->request_id());
            response->set_success(true);
        }
        virtual void UserLogin(::google::protobuf::RpcController *controller,
                               const ::IM::UserLoginReq *request,
                               ::IM::UserLoginRsp *response,
                               ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
            auto err_handler = [this, response](const std::string &rid,
                                                const std::string &errmsg)
            {
                response->set_request_id(rid);
                response->set_errmsg(errmsg);
                response->set_success(false);
            };
            // 用户登录逻辑
            // 1. 从用户请求中获取登录信息 取出昵称和密码
            std::string nickname = request->nickname();
            std::string password = request->password();
            // 2.检查昵称是否存在
            auto user = _mysql_user->select_by_nickname(nickname);
            if (user == nullptr)
            {
                LOG_ERROR("用户不存在: - {}", request->request_id());
                err_handler(request->request_id(), "用户不存在");
                return;
            }
            // 3.如果昵称存在 则检查密码是否正确
            if (user->password() != password)
            {
                LOG_ERROR("密码错误: - {}", request->request_id());
                err_handler(request->request_id(), "密码错误");
                return;
            }
            // 4.在redis中检查用户登录状态 如果已经登录 则返回错误提示 如果没有登录 则继续登录流程
            if (_redis_login_status->exists(user->user_id()))
            {
                LOG_ERROR("用户已登录: - {}", request->request_id());
                err_handler(request->request_id(), "用户已登录");
                return;
            }
            std::string session_id = uuid();
            _redis_session->append(session_id, user->user_id());
            // 5.登录成功 返回成功响应
            response->set_request_id(request->request_id());
            response->set_success(true);
            response->set_login_session_id(session_id);
        }
        bool check_phone_number(const std::string &phone_number)
        {
            if (phone_number.length() != 11)
            {
                LOG_ERROR("手机号长度不合法");
                return false;
            }
            if (phone_number[0] != '1')
            {
                LOG_ERROR("手机号开头不合法");
                return false;
            }
            for (char c : phone_number)
            {
                if (!std::isdigit(c))
                {
                    LOG_ERROR("手机号包含非法字符");
                    return false;
                }
            }
            return true;
        }
        virtual void GetPhoneVerifyCode(::google::protobuf::RpcController *controller,
                                        const ::IM::PhoneVerifyCodeReq *request,
                                        ::IM::PhoneVerifyCodeRsp *response,
                                        ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
            auto err_handler = [this, response](const std::string &rid,
                                                const std::string &errmsg)
            {
                response->set_request_id(rid);
                response->set_errmsg(errmsg);
                response->set_success(false);
            };
            // 1.从请求中取出手机号
            std::string phone_number = request->phone_number();
            // 2.检查手机号是否合法
            if (!check_phone_number(phone_number))
            {
                LOG_ERROR("手机号不合法: - {}", request->request_id());
                err_handler(request->request_id(), "手机号不合法");
                return;
            }
            // 3.生成验证码
            std::string code_id = uuid();
            std::string code = vcode();

            // 4.发送验证码到用户手机
            if (!_dms_client->send(phone_number, code))
            {
                LOG_ERROR("验证码发送失败: - {}", request->request_id());
                err_handler(request->request_id(), "验证码发送失败");
                return;
            }
            // 5.将验证码存储到redis中 设置过期时间为1分钟
            if (!_redis_codes->append(code_id, code))
            {
                LOG_ERROR("验证码存储失败: - {}", request->request_id());
                err_handler(request->request_id(), "验证码存储失败");
                return;
            }

            // 6.返回响应给用户 响应中包含验证码id 用户后续验证验证码时需要提供验证码id和验证码内容
            response->set_request_id(request->request_id());
            response->set_success(true);
            response->set_verify_code_id(code_id);
        }
        virtual void PhoneRegister(::google::protobuf::RpcController *controller,
                                   const ::IM::PhoneRegisterReq *request,
                                   ::IM::PhoneRegisterRsp *response,
                                   ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
            auto err_handler = [this, response](const std::string &rid,
                                                const std::string &errmsg)
            {
                response->set_request_id(rid);
                response->set_errmsg(errmsg);
                response->set_success(false);
            };
            // 1.从请求中取出手机号 验证码id 验证码内容 昵称 密码
            std::string phone_number = request->phone_number();
            std::string code_id = request->verify_code_id();
            std::string code = request->verify_code();
            // 2.检查手机号是否合法
            if (!check_phone_number(phone_number))
            {
                LOG_ERROR("手机号不合法: - {}", request->request_id());
                err_handler(request->request_id(), "手机号不合法");
                return;
            }
            // 3.检查验证码是否正确
            auto vcode = _redis_codes->code(code_id);
            if (!vcode.has_value() || vcode.value() != code)
            {
                LOG_ERROR("验证码错误: - {}", request->request_id());
                err_handler(request->request_id(), "验证码错误");
                return;
            }
            // 4.通过数据库查看该手机号是否注册过 如果没有注册过 需要新增用户信息
            auto user = _mysql_user->select_by_phone(phone_number);
            if (user != nullptr)
            {
                LOG_ERROR("手机号已注册: - {}", request->request_id());
                err_handler(request->request_id(), "手机号已注册");
                return;
            }
            std::string user_id = uuid();
            user = std::make_shared<User>(user_id, phone_number);

            if (!_mysql_user->insert(user))
            {
                LOG_ERROR("用户注册失败: - {}", request->request_id());
                err_handler(request->request_id(), "用户注册失败");
                return;
            }

            // 5.向es中添加用户信息 以便后续搜索使用
            if (!_es_user->appendData(user_id, phone_number, user_id, "", ""))
            {
                LOG_ERROR("用户信息添加到ES失败: - {}", request->request_id());
                err_handler(request->request_id(), "用户信息添加到ES失败");
                return;
            }
            // 6.注册成功 返回成功响应
            response->set_request_id(request->request_id());
            response->set_success(true);
        }

        virtual void PhoneLogin(::google::protobuf::RpcController *controller,
                                const ::IM::PhoneLoginReq *request,
                                ::IM::PhoneLoginRsp *response,
                                ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
            auto err_handler = [this, response](const std::string &rid,
                                                const std::string &errmsg)
            {
                response->set_request_id(rid);
                response->set_errmsg(errmsg);
                response->set_success(false);
            };
            // 1.从请求中取出手机号 验证码id 验证码内容
            
        }
        virtual void GetUserInfo(::google::protobuf::RpcController *controller,
                                 const ::IM::GetUserInfoReq *request,
                                 ::IM::GetUserInfoRsp *response,
                                 ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
        }
        virtual void GetMultiUserInfo(::google::protobuf::RpcController *controller,
                                      const ::IM::GetMultiUserInfoReq *request,
                                      ::IM::GetMultiUserInfoRsp *response,
                                      ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
        }
        virtual void SetUserAvatar(::google::protobuf::RpcController *controller,
                                   const ::IM::SetUserAvatarReq *request,
                                   ::IM::SetUserAvatarRsp *response,
                                   ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
        }
        virtual void SetUserNickname(::google::protobuf::RpcController *controller,
                                     const ::IM::SetUserNicknameReq *request,
                                     ::IM::SetUserNicknameRsp *response,
                                     ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
        }
        virtual void SetUserDescription(::google::protobuf::RpcController *controller,
                                        const ::IM::SetUserDescriptionReq *request,
                                        ::IM::SetUserDescriptionRsp *response,
                                        ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
        }
        virtual void SetUserPhoneNumber(::google::protobuf::RpcController *controller,
                                        const ::IM::SetUserPhoneNumberReq *request,
                                        ::IM::SetUserPhoneNumberRsp *response,
                                        ::google::protobuf::Closure *done)
        {
            brpc::ClosureGuard rpc_guard(done);
        }

    private:
        ESUser::ptr _es_user;
        UserTable::ptr _mysql_user;
        Session::ptr _redis_session;
        Status::ptr _redis_login_status;
        Codes::ptr _redis_codes;

        // rpc客户端调用相关对象
        ServiceManager::ptr _channel_manager;
        std::string _file_service_name;

        DMSClient::ptr _dms_client;
    };

    class UserServer
    {
    public:
        using ptr = std::shared_ptr<UserServer>;
        UserServer(const std::shared_ptr<elasticlient::Client> &es_client,
                   const std::shared_ptr<odb::core::database> &mysql_client,
                   const std::shared_ptr<sw::redis::Redis> &redis_client,
                   Discovery::ptr discovery, Registry::ptr reg_client,
                   const std::shared_ptr<brpc::Server> &server,
                   const std::shared_ptr<DMSClient> &dms_client)
            : _es_client(es_client),
              _mysql_client(mysql_client),
              _redis_client(redis_client),
              _discovery(discovery),
              _reg_client(reg_client),
              _rpc_server(server),
              _dms_client(dms_client)
        {
        }
        ~UserServer()
        {
        }
        void start()
        {
            _rpc_server->RunUntilAskedToQuit();
        }

    private:
        Discovery::ptr _discovery;
        Registry::ptr _reg_client;
        std::shared_ptr<brpc::Server> _rpc_server;
        std::shared_ptr<elasticlient::Client> _es_client;
        std::shared_ptr<odb::core::database> _mysql_client;
        std::shared_ptr<sw::redis::Redis> _redis_client;
        std::shared_ptr<DMSClient> _dms_client;
    };
    class UserServerBuilder
    {
    public:
        // 构造es客户端对象
        // 构造mysql客户端对象
        // 构造redis客户端对象
        void make_es_object(const std::vector<std::string> &es_host)
        {
            _es_client = ESClientFactory::create(es_host);
        }

        void make_mysql_object(const std::string &user,
                               const std::string &password,
                               const std::string &host,
                               const std::string &db,
                               const std::string &cset,
                               int port,
                               int conn_pool_count)
        {
            _mysql_client = ODBFactory::create(user, password,
                                               host, db, cset, port, conn_pool_count);
        }

        void make_redis_object(const std::string &host,
                               int port, int db, bool keep_alive)
        {
            _redis_client = RedisClientFactory::create(host, port, db, keep_alive);
        }
        void make_discovery_object(const std::string &etcd_host,
                                   const std::string &base_service_name, const std::string &file_service_name)
        {
            _file_service_name = file_service_name;
            _channel_manager = std::make_shared<ServiceManager>();
            _channel_manager->declared(file_service_name);
            auto put_cb = std::bind(&ServiceManager::onServiceOnline, _channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
            auto del_cb = std::bind(&ServiceManager::onServiceOffline, _channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
            _discovery = std::make_shared<Discovery>(etcd_host, base_service_name, put_cb, del_cb);
        }
        void make_dms_client(const std::string &access_key_id,
                             const std::string &access_key_secret)
        {
            _dms_client = std::make_shared<DMSClient>(access_key_id, access_key_secret);
        }
        void make_reg_object(const std::string &reg_host,
                             const std::string &service_name, const std::string &access_host)
        {
            // 注册中心 注册地址和访问地址
            _reg_client = std::make_shared<Registry>(reg_host);
            _reg_client->registry(service_name, access_host);
        }

        // 搭建RPC服务器，并启动
        void make_rpc_server(uint16_t port, uint32_t timeout = -1, uint8_t num_threads = 1)
        {
            if (!_es_client)
            {
                LOG_ERROR("ES client is not initialized");
                abort();
            }
            if (!_mysql_client)
            {
                LOG_ERROR("MySQL client is not initialized");
                abort();
            }
            if (!_redis_client)
            {
                LOG_ERROR("Redis client is not initialized");
                abort();
            }
            if (!_channel_manager)
            {
                LOG_ERROR("Channel manager is not initialized");
                abort();
            }
            if (!_discovery)
            {
                LOG_ERROR("Discovery is not initialized");
                abort();
            }
            if (!_reg_client)
            {
                LOG_ERROR("Registry client is not initialized");
                abort();
            }
            if (!_dms_client)
            {
                LOG_ERROR("DMS client is not initialized");
                abort();
            }
            _rpc_server = std::make_shared<brpc::Server>();
            auto User_service = new UserServerImpl(_es_client, _mysql_client,
                                                   _redis_client, _channel_manager, _file_service_name, _dms_client);
            int ret = _rpc_server->AddService(User_service,
                                              brpc::SERVER_OWNS_SERVICE); // SERVER_OWNS_SERVICE表示服务器会负责删除这个服务对象
            if (ret == -1)
            {
                LOG_ERROR("Failed to add service");
                abort();
            }
            brpc::ServerOptions options;
            options.idle_timeout_sec = 0;          // 连接不超时
            options.max_concurrency = num_threads; // 最大并发数
            ret = _rpc_server->Start(port, &options);
            if (ret != 0)
            {
                LOG_ERROR("Failed to start RPC server");
                abort();
            }
            LOG_INFO("RPC server started on port {}", port);
        }
        UserServer::ptr build()
        {
            if (!_es_client)
            {
                LOG_ERROR("ES client is not initialized");
                abort();
            }
            if (!_mysql_client)
            {
                LOG_ERROR("MySQL client is not initialized");
                abort();
            }
            if (!_redis_client)
            {
                LOG_ERROR("Redis client is not initialized");
                abort();
            }
            if (!_channel_manager)
            {
                LOG_ERROR("Channel manager is not initialized");
                abort();
            }
            if (!_discovery)
            {
                LOG_ERROR("Discovery is not initialized");
                abort();
            }
            if (!_reg_client)
            {
                LOG_ERROR("Registry client is not initialized");
                abort();
            }
            if (!_rpc_server)
            {
                LOG_ERROR("RPC server is not initialized");
                abort();
            }
            if (!_dms_client)
            {
                LOG_ERROR("DMS client is not initialized");
                abort();
            }
            UserServer::ptr server = std::make_shared<UserServer>(_es_client,
                                                                  _mysql_client, _redis_client, _discovery, _reg_client, _rpc_server, _dms_client);
            return server;
        }

    private:
        std::shared_ptr<elasticlient::Client> _es_client;

        std::shared_ptr<odb::core::database> _mysql_client;

        std::shared_ptr<sw::redis::Redis> _redis_client;

        std::shared_ptr<DMSClient> _dms_client;

        std::string _file_service_name;
        ServiceManager::ptr _channel_manager;
        Discovery::ptr _discovery;
        Registry::ptr _reg_client;
        std::shared_ptr<brpc::Server> _rpc_server;
    };
}
