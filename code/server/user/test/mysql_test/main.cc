#include "../../../common/data_mysql.hpp"
#include "../../../odb/user.hxx"
#include "user-odb.hxx"
#include <gflags/gflags.h>

using namespace IM;
void insert_user(IM::UserTable &user)
{
    auto user1 = std::make_shared<IM::User>("uid1", "user1_nickname", "user1_password");
    user.insert(user1);

    auto user2 = std::make_shared<IM::User>("uid2", "111111");
    user.insert(user2);
}
void update_by_id(IM::UserTable &user)
{
    auto res = user.select_by_user_id("uid1");
    if (res)
    {
        res->description("This is user1's description.");
        user.update(res);
        LOG_INFO("User updated successfully: {}", res->user_id());
    }
    else
    {
        LOG_ERROR("User not found for update by user_id");
    }
}
void update_by_phone(IM::UserTable &user)
{
    auto res = user.select_by_phone("111111");
    res->password("new_password_for_user2");
    user.update(res);
}
void select_by_id(IM::UserTable &user)
{
    auto res = user.select_by_user_id("uid1");
    if (res)
    {
        LOG_INFO("User found: {} {} {}", res->user_id(), res->nickname(), res->phone());
    }
    else
    {
        LOG_ERROR("User not found by user_id");
    }
}
void select_by_phone(IM::UserTable &user)
{
    auto res = user.select_by_phone("111111");
    if (res)
    {
        LOG_INFO("User found: {} {} {}", res->user_id(), res->nickname(), res->phone());
    }
    else
    {
        LOG_ERROR("User not found by phone");
    }
}
void select_by_nickname(IM::UserTable &user)
{
    auto res = user.select_by_nickname("user1_nickname");
    if (res)    
    {
        LOG_INFO("User found: {} {} {}", res->user_id(), res->nickname(), res->phone());
    }
    else
    {
        LOG_ERROR("User not found by nickname");
    }
}
void select_users(IM::UserTable &user)
{
    std::vector<std::string> id_lists = {"uid1", "uid2"};
    auto res = user.select_multi_users(id_lists);
    for (auto &u : res)
    {
        std::cout << u.user_id() << " " << u.nickname() << " " << u.phone() << std::endl;
    }
}


DEFINE_bool(debug_enable, false, "是否启用调试模式");
DEFINE_string(log_file_path, "Log/log.txt", "日志文件路径");
DEFINE_int32(log_level, 0, "日志输出等级");
int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);

    IM::init_log(FLAGS_debug_enable, FLAGS_log_file_path, FLAGS_log_level);

   auto db = IM::ODBFactory::create(
    "root",           // user
    "tjmnb666",       // password
    "127.0.0.1",      // host
    "IM",             // db (数据库名)
    "utf8",           // cset (字符集)
    3306,
    1           // port
);
    IM::UserTable user(db);

    //insert_user(user);
    // update_by_id(user);
    // select_by_id(user);
    // select_by_phone(user);
    // select_by_nickname(user);
    //update_by_phone(user);
    select_users(user);
    return 0;
}