#include <elasticlient/client.h>
#include <iostream>
#include <cpr/cpr.h>
#include <memory>
#include <string>
#include <json/json.h>
#include "logger.hpp"

bool Serialize(const Json::Value &root, std::string &output)
{
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["emitUTF8Bom"] = true;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    std::stringstream ss;
    if (jsonWriter->write(root, &ss) != 0)
    {
        LOG_ERROR("Failed to serialize JSON.");
        return false;
    }
    output = ss.str();
    return true;
}
bool UnSerialize(const std::string &input, Json::Value &root)
{
    Json::CharReaderBuilder readerBuilder;
    readerBuilder["emitUTF8Bom"] = true;

    std::unique_ptr<Json::CharReader> jsonReader(readerBuilder.newCharReader());
    std::string errs;
    if (!jsonReader->parse(input.c_str(), input.c_str() + input.length(), &root, &errs))
    {
        LOG_ERROR("Failed to unserialize JSON: " + errs);
        return false;
    }
    return true;
}

class ESIndex
{
public:
    ESIndex(const std::string &name, const std::string &type, std::shared_ptr<elasticlient::Client> client)
        : _name(name), _type(type), _client(client)
    {
        Json::Value analysis;
        Json::Value analyzer;
        Json::Value ik;
        Json::Value tokenizer;
        tokenizer["tokenizer"] = "ik_max_word";
        ik["ik"] = tokenizer;
        analyzer["analyzer"] = ik;
        analysis["analysis"] = analyzer;
        _index["settings"] = analysis;
    }
    ESIndex append(const std::string &key, const std::string &type = "text",
                   const std::string &analyzer = "ik_max_word", bool enabled = true)
    {
        Json::Value fields;
        fields["type"] = type;
        if (type == "text")
            fields["analyzer"] = analyzer;
        if (!enabled)
            fields["enabled"] = enabled;
        _properties[key] = fields;
        return *this;
    }
    bool create( )
    {
        Json::Value mappings;
        mappings["dynamic"] = true;
        mappings["properties"] = _properties;
        _index["mappings"] = mappings;

        std::string body;
        bool ret = Serialize(_index, body);
        if (!ret)
        {
            LOG_ERROR("Failed to serialize index definition.");
            return false;
        }
        LOG_INFO("请求正文[{}]", body);

        try
        {
            auto response = cpr::Put(
                cpr::Url{"http://127.0.0.1:9200/" + _name},
                cpr::Body{body},
                cpr::Header{{"Content-Type", "application/json"}});
            if (response.status_code < 200 || response.status_code >= 300)
            {
                LOG_ERROR("Failed to create index:{} status_code: {}, body: {}",
                          _name, response.status_code, response.text);
                return false;
            }
        }
        catch (const std::exception &e)
        {

            LOG_ERROR("Failed to create index:{} ", std::string(e.what()));
            return false;
        }
        return true;
    }

private:
    std::string _name;
    std::string _type;
    Json::Value _index;
    Json::Value _properties;
    std::shared_ptr<elasticlient::Client> _client;
};

class ESInsert
{
public:
    ESInsert(const std::string &name, const std::string &type, std::shared_ptr<elasticlient::Client> client)
        : _name(name), _type(type), _client(client)
    {
    }
    ESInsert append(const std::string &key, const std::string &val)
    {
        _item[key] = val;
        return *this;
    }
    bool insert(std::string id = "")
    {
        std::string body;
        bool ret = Serialize(_item, body);
        if (!ret)
        {
            LOG_ERROR("Failed to serialize item definition.");
            return false;
        }
        LOG_INFO("请求正文[{}]", body);

        try
        {
            std::string url = "http://127.0.0.1:9200/" + _name + "/_doc";
            if (!id.empty())
                url += "/" + id;

            auto response = cpr::Post(
                cpr::Url{url},
                cpr::Body{body},
                cpr::Header{{"Content-Type", "application/json"}});
            if (response.status_code < 200 || response.status_code >= 300)
            {
                LOG_ERROR("Failed to insert index:{} status_code: {}, body: {}",
                          _name, response.status_code, response.text);
                return false;
            }
        }
        catch (const std::exception &e)
        {

            LOG_ERROR("Failed to insert index:{} ", std::string(e.what()));
            return false;
        }
        return true;
    }

private:
    std::string _name;
    std::string _type;
    std::shared_ptr<elasticlient::Client> _client;
    Json::Value _item;
};

class ESRemove
{
public:
    ESRemove(const std::string &name, const std::string &type, std::shared_ptr<elasticlient::Client> client)
        : _name(name), _type(type), _client(client)
    {
    }

    bool remove(const std::string &id)
    {
        try
        {
            auto response = cpr::Delete(
                cpr::Url{"http://127.0.0.1:9200/" + _name + "/_doc/" + id});
            if (response.status_code < 200 || response.status_code >= 300)
            {
                LOG_ERROR("Failed to remove index:{} status_code: {}, body: {}",
                          _name, response.status_code, response.text);
                return false;
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Failed to remove index:{} ", std::string(e.what()));
            return false;
        }
        return true;
    }

private:
    std::string _name;
    std::string _type;
    std::shared_ptr<elasticlient::Client> _client;
};

class ESSearch
{
public:
    ESSearch(const std::string &name, const std::string &type, std::shared_ptr<elasticlient::Client> client)
        : _name(name), _type(type), _client(client)
    {
    }
    ESSearch append_must_not_terms(const std::string &key, const std::vector<std::string> &values)
    {
        Json::Value fields;
        for (const auto &value : values)
        {
            fields[key].append(value);
        }
        Json::Value terms;
        terms["terms"] = fields;
        _must_not.append(terms);
        return *this;
    }
    ESSearch append_should_match(const std::string &key, const std::string &value)
    {
        Json::Value field;
        field[key] = value;
        Json::Value match;
        match["match"] = field;
        _should.append(match);
        return *this;
    }
    ESSearch &append_must_term(const std::string &key, const std::string &val)
    {
        Json::Value field;
        field[key] = val;
        Json::Value term;
        term["term"] = field;
        _must.append(term);
        return *this;
    }
    
    Json::Value search()
    {
        Json::Value cond;
        if (!_must_not.empty())
            cond["must_not"] = _must_not;
        if (!_should.empty())
            cond["should"] = _should;
        if(!_must.empty())
            cond["must"] = _must;

        Json::Value bool_cond;
        bool_cond["bool"] = cond;
        Json::Value query;
        query["query"] = bool_cond;
        std::string body;
        bool ret = Serialize(query, body);
        if (!ret)
        {
            LOG_ERROR("Failed to serialize item definition.");
            return false;
        }
        LOG_INFO("请求正文[{}]", body);
        cpr::Response response;
        try
        {
            response = cpr::Post(
                cpr::Url{"http://127.0.0.1:9200/" + _name + "/_search"},
                cpr::Body{body},
                cpr::Header{{"Content-Type", "application/json"}});
            if (response.status_code < 200 || response.status_code >= 300)
            {
                LOG_ERROR("Failed to search index:{} status_code: {}, body: {}",
                          _name, response.status_code, response.text);
                return Json::Value();
            }
        }
        catch (const std::exception &e)
        {

            LOG_ERROR("Failed to search index:{} ", std::string(e.what()));
            return Json::Value();
        }

        LOG_DEBUG("检索响应正文: [{}]", response.text);
        Json::Value json_res;
        ret = UnSerialize(response.text, json_res);
        if (ret == false)
        {
            LOG_ERROR("检索数据 {} 结果反序列化失败", response.text);
            return Json::Value();
        }
        return json_res["hits"]["hits"];
    }

private:
    std::string _name;
    std::string _type;
    Json::Value _must_not = Json::Value(Json::arrayValue);
    Json::Value _must = Json::Value(Json::arrayValue);
    Json::Value _should = Json::Value(Json::arrayValue);
    std::shared_ptr<elasticlient::Client> _client;
};