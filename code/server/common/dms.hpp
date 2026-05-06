#include <cstdlib>
#include <iostream>
#include <memory>
#include <alibabacloud/core/AlibabaCloud.h>
#include <alibabacloud/core/CommonRequest.h>
#include <alibabacloud/core/CommonClient.h>
#include <alibabacloud/core/CommonResponse.h>
#include "logger.hpp"


namespace IM
{
class DMSClient
{
public:
    using ptr = std::shared_ptr<DMSClient>;
    DMSClient(const std::string &access_key_id, const std::string &access_key_secret)
    {
        AlibabaCloud::InitializeSdk();
        AlibabaCloud::ClientConfiguration configuration("cn-hangzhou");
        configuration.setConnectTimeout(1500);
        configuration.setReadTimeout(4000);

        AlibabaCloud::Credentials credential(access_key_id, access_key_secret);
        _client = std::make_unique<AlibabaCloud::CommonClient>(credential, configuration);
    }

    ~DMSClient()
    {
        AlibabaCloud::ShutdownSdk();
    }

    void send(const std::string &phone_number, const std::string &code)
    {
        AlibabaCloud::CommonRequest request(AlibabaCloud::CommonRequest::RequestPattern::RpcPattern);

        request.setHttpMethod(AlibabaCloud::HttpRequest::Method::Post);
        request.setDomain("dypnsapi.aliyuncs.com");
        request.setVersion("2017-05-25");
        request.setQueryParameter("Action", "SendSmsVerifyCode");
        request.setQueryParameter("SignName", "速通互联验证码");
        request.setQueryParameter("TemplateCode", "100003");
        request.setQueryParameter("PhoneNumber", phone_number);
        std::string param_code = "{\"code\":\"" + code + "\",\"min\":\"5\"}";
        request.setQueryParameter("TemplateParam", param_code);

        auto response = _client->commonResponse(request);
        if (response.isSuccess())
        {
            LOG_INFO("send sms code success, phone={}, payload={}", phone_number, response.result().payload().c_str());
        }   
        else
        {
            LOG_ERROR("send sms code failed: {}", response.error().errorMessage().c_str());
            LOG_ERROR("error code: {}", response.error().errorCode().c_str());
            LOG_ERROR("request id: {}", response.error().requestId().c_str());
            LOG_ERROR("phone={}", phone_number);
        }
    }

private:
    std::unique_ptr<AlibabaCloud::CommonClient> _client;
};
}