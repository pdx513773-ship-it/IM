#pragma once
#include <string>
#include <iostream>
#include <memory>  // std::shared_ptr
#include <cstdlib> // std::exit
#include <odb/core.hxx>
#include <odb/nullable.hxx>
#include <cstddef>

namespace IM
{
#pragma db object table("user")
    class User
    {
    public:
        User() {}
        // 手机号注册：只需要 user_id 和 phone
        User(const std::string &user_id, const std::string &phone)
            : _user_id(user_id), _phone(phone) {}

        // 普通注册：需要 user_id, nickname, password
        User(const std::string &user_id, const std::string &nickname, const std::string &password)
            : _user_id(user_id), _nickname(nickname), _password(password) {}
        std::string user_id() const { return _user_id; }
        void user_id(const std::string &user_id) { _user_id = user_id;}
        std::string nickname() const { return _nickname ? *_nickname : ""; }
        void nickname(const std::string &nickname) { _nickname = nickname; }

        void description(const std::string &description) { _description = description; }
        std::string description() const { return _description ? *_description : ""; }

        void password(const std::string &password) { _password = password; }
        std::string password() const { return _password ? *_password : ""; }

        void avatar_id(const std::string &avatar_id) { _avatar_id = avatar_id; }
        std::string avatar_id() const { return _avatar_id ? *_avatar_id : ""; }

        void phone(const std::string &phone) { _phone = phone; }
        std::string phone() const { return _phone ? *_phone : ""; }

    private:
        friend class odb::access;
#pragma db id auto
        unsigned long _id;

#pragma db type("varchar(64)") index unique
        std::string _user_id;

#pragma db type("varchar(64)") index unique
        odb::nullable<std::string> _phone;

#pragma db type("varchar(64)") index unique
        odb::nullable<std::string> _nickname;    // 手机号注册默认是没有昵称的 该字段可以为空
        odb::nullable<std::string> _description; // 个人简介
#pragma db type("varchar(64)")
        odb::nullable<std::string> _password;
#pragma db type("varchar(64)")
        odb::nullable<std::string> _avatar_id;
    };
}