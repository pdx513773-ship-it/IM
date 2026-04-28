#include "aip-cpp-sdk/speech.h"
#include "logger.hpp"
class ASRClient
{
    public:
        using ptr = std::shared_ptr<ASRClient>;
        ASRClient(const std::string app_id, const std::string &ak, const std::string &sk)
        {
            _client = std::make_unique<aip::Speech>(app_id, ak, sk);
        }
        std::string recognize(const std::string &file_path,std::string &err)
        {
            Json::Value result = _client->recognize(file_path, "pcm", 16000, aip::null);
            if(result["err_no"].asInt() != 0)
            {
                LOG_ERROR("ASR recognition failed: {}", result["err_msg"].asString());
                err = result["err_msg"].asString();
                return std::string();
            }

            return result["result"][0].asString();
        }
    private:
        std::unique_ptr<aip::Speech> _client;
};