#include "../odb/user.hxx"
#include "../user/test/mysql_test/user-odb.hxx"
#include <odb/mysql/database.hxx>
#include <memory>
#include <iostream>
#include <string>
#include <odb/database.hxx>
#include "logger.hpp"

namespace IM
{
    class ODBFactory
    {
    public:
        static std::shared_ptr<odb::database> create(
            const std::string &user,
            const std::string &password,
            const std::string &host,
            const std::string &db,
            const std::string &cset,
            int port,
            int conn_pool_count)
        {
            // 使用 new 而不是 make_shared，避免构造函数重载解析错误
            std::unique_ptr<odb::mysql::connection_pool_factory> cpf(
                new odb::mysql::connection_pool_factory(conn_pool_count,0));
            auto res = std::make_shared<odb::mysql::database>(user,
                                                              password, db, host, port, nullptr, cset, 0,std::move(cpf));
            return res;
        }
    };

    class UserTable
    {
    public:
        UserTable(const std::shared_ptr<odb::database> &db) : _db(db) {}

        bool insert(const std::shared_ptr<User> &user)
        {
            try
            {
                odb::transaction trans(_db->begin());
                _db->persist(*user);
                trans.commit();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to persist user: {}", user->nickname());
                return false;
            }
            return true;
        }
        bool update(const std::shared_ptr<User> &user)
        {
            try
            {
                odb::transaction trans(_db->begin());
                _db->update(*user);
                trans.commit();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to update user: {}", user->nickname());
                return false;
            }
            return true;
        }

        std::shared_ptr<User> select_by_user_id(const std::string &user_id)
        {
            std::shared_ptr<User> res;
            try
            {
                odb::transaction trans(_db->begin());
                typedef odb::query<User> query;
                typedef odb::result<User> result;
                res.reset(_db->query_one<User>(query::user_id == user_id));
                trans.commit();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to select user by user_id: {}", user_id);
                return nullptr;
            }
            return res;
        }
        std::shared_ptr<User> select_by_phone(const std::string &phone)
        {
            std::shared_ptr<User> res;
            try
            {
                odb::transaction trans(_db->begin());
                typedef odb::query<User> query;
                typedef odb::result<User> result;
                res.reset(_db->query_one<User>(query::phone == phone));
                trans.commit();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to select user by phone: {}", phone);
                return nullptr;
            }
            return res;
        }
        std::shared_ptr<User> select_by_nickname(const std::string &nickname)
        {
            std::shared_ptr<User> res;

            try
            {
                odb::transaction trans(_db->begin());
                typedef odb::query<User> query;
                typedef odb::result<User> result;
                res.reset(_db->query_one<User>(query::nickname == nickname));
                trans.commit();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to select user by nickname: {}", nickname);
                return nullptr;
            }
            return res;
        }
        std::vector<User> select_multi_users(const std::vector<std::string> &id_lists)
        {
            std::vector<User> res;
            if (id_lists.empty())
            {
                return res;
            }

            try
            {
                odb::transaction trans(_db->begin());
                typedef odb::query<User> query;

                // 直接使用 ODB 的 IN 语法，安全且高效
                auto result = _db->query<User>(
                    query::user_id.in_range(id_lists.begin(), id_lists.end()));

                for (const auto &user : result)
                {
                    res.push_back(user);
                }

                trans.commit();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to select multi users by id list: {}", e.what());
            }

            return res;
        }

    private:
        std::shared_ptr<odb::database> _db;
    };
}
