#include "icsearch.hpp"
#include "logger.hpp"
#include "../odb/user.hxx"
namespace IM
{
    class ESClientFactory
    {
    public:
        // 接收字符串（单节点）
        static std::shared_ptr<elasticlient::Client> create(const std::string &hostlist)
        {
            std::vector<std::string> hosts = {hostlist};
            auto client = std::make_shared<elasticlient::Client>(hosts);
            return client;
        }

        // 接收字符串向量（多节点/集群）
        static std::shared_ptr<elasticlient::Client> create(const std::vector<std::string> &hostlist)
        {
            auto client = std::make_shared<elasticlient::Client>(hostlist);
            return client;
        }
    };
    class ESUser
    {
    public:
        using ptr = std::shared_ptr<ESUser>;
        ESUser(const std::shared_ptr<elasticlient::Client> &client) : _client(client) {}

        void createIndex()
        {
            ESIndex index("user", "doc", _client);
            index.append("user_id", "keyword", "standard", true)
                .append("phone", "keyword", "standard", true)
                .append("nickname")
                .append("description", "text", "standard", false)
                .append("avatar_id", "keyword", "standard", false);
            if (!index.create())
            {
                LOG_ERROR("创建ES索引失败");
            }
            LOG_INFO("创建ES索引成功");
        }
        bool appendData(const std::string &user_id,
                        const std::string &phone,
                        const std::string &nickname,
                        const std::string &description,
                        const std::string &avatar_id)
        {
            bool ret = ESInsert("user", "doc", _client)
                           .append("user_id", user_id)
                           .append("phone", phone)
                           .append("nickname", nickname)
                           .append("description", description)
                           .append("avatar_id", avatar_id)
                           .insert(user_id);
            if (!ret)
            {
                LOG_ERROR("插入ES数据失败");
            }
            LOG_INFO("插入ES数据成功");
            return ret;
        }
        std::vector<User> search(const std::string &key, const std::vector<std::string> &uid_list)
        {
            std::vector<User> res;
            Json::Value user;
            user = ESSearch("user", "doc", _client)
                       .append_should_match("nickname", {key})
                       .append_should_match("description", {key})
                       .append_should_match("phone", {key})
                       .append_should_match("user_id", {key})
                       .append_must_not_terms("user_id.keyword", uid_list)
                       .search();
            if (user.isArray() == false)
            {
                LOG_ERROR("ES搜索结果格式错误");
                return res;
            }
            int sz = user.size();
            LOG_DEBUG("ES搜索结果数量: {}", sz);
            for (int i = 0; i < sz; ++i)
            {
                auto item = user[i]["_source"];
                User info;
                info.user_id(item["user_id"].asString());
                info.phone(item["phone"].asString());
                info.nickname(item["nickname"].asString());
                info.description(item["description"].asString());
                info.avatar_id(item["avatar_id"].asString());
                LOG_INFO("ES搜索结果: user_id={}, phone={}, nickname={}, description={}, avatar_id={}",
                         info.user_id(), info.phone(), info.nickname(), info.description(), info.avatar_id());
                res.push_back(info);
            }
            return res;
        }

    private:
        std::shared_ptr<elasticlient::Client> _client;
    };
}