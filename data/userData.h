#pragma once

#include <memory>
#include <string>
#include <optional>
#include <mutex>
#include <odb/mysql/database.hxx>
#include <sw/redis++/redis.h>
#include "../svc_userService/common.h"

namespace userService {

class UserData {
public:
    /**
     * @brief  构造函数，根据配置初始化MySQL和Redis连接
     * @param  db  MySQL数据库连接
     * @param  redis  Redis数据库连接
     */
    UserData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief  保存用户信息到MySQL，如果用户存在则更新，不存在则插入
     * @param  userInfo  用户信息结构体
     * @return 成功返回true，失败返回false
     */
    bool saveUserToDb(const UserInfo& userInfo);

    /**
     * @brief  从MySQL通过用户ID获取用户信息
     * @param  userId  用户ID
     * @return 存在返回用户信息，不存在返回std::nullopt
     */
    std::optional<UserInfo> getUserByUserIdFromDb(const std::string& userId);

    /**
     * @brief  从MySQL通过用户昵称获取用户信息
     * @param  nickname  用户昵称
     * @return 存在返回用户信息，不存在返回std::nullopt
     */
    std::optional<UserInfo> getUserByNicknameFromDb(const std::string& nickname);

    /**
     * @brief  从MySQL通过用户邮箱获取用户信息
     * @param  email  用户邮箱
     * @return 存在返回用户信息，不存在返回std::nullopt
     */
    std::optional<UserInfo> getUserByEmailFromDb(const std::string& email);

    /**
     * @brief  从MySQL检测昵称是否存在
     * @param  nickname  用户昵称
     * @return 存在返回true，不存在返回false
     */
    bool existNicknameInDb(const std::string& nickname);

    /**
     * @brief  从MySQL检测邮箱是否存在
     * @param  email  用户邮箱
     * @return 存在返回true，不存在返回false
     */
    bool existEmailInDb(const std::string& email);

    /**
     * @brief  保存用户信息到Redis缓存
     * @param  userInfo  用户信息结构体
     * @return 成功返回true，失败返回false
     */
    bool saveUserToCache(const UserInfo& userInfo);

    /**
     * @brief  从Redis缓存通过用户ID获取用户信息
     * @param  userId  用户ID
     * @return 存在返回用户信息，不存在返回std::nullopt
     */
    std::optional<UserInfo> getUserFromCacheByUserId(const std::string& userId);

    /**
     * @brief  从Redis缓存通过用户昵称获取用户信息
     * @param  nickname  用户昵称
     * @return 存在返回用户信息，不存在返回std::nullopt
     */
    std::optional<UserInfo> getUserFromCacheByNickname(const std::string& nickname);

    /**
     * @brief  从Redis缓存通过用户邮箱获取用户信息
     * @param  email  用户邮箱
     * @return 存在返回用户信息，不存在返回std::nullopt
     */
    std::optional<UserInfo> getUserFromCacheByEmail(const std::string& email);

    /**
     * @brief  从Redis缓存删除用户信息
     * @param  userId    用户ID
     * @param  nickname  用户昵称
     * @param  email     用户邮箱
     */
    void deleteUserCache(const std::string& userId, const std::string& nickname,
                         const std::string& email);

private:
    std::shared_ptr<odb::database> _db;    // MySQL数据库连接
    std::shared_ptr<sw::redis::Redis> _redis;      // Redis数据库连接
    std::mutex _redisMutex;                // Redis操作互斥锁
    const static int USER_CACHE_TTL_SECONDS = 60 * 60;  // 1h = 60min * 60s
};

} // namespace userService