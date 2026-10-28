#include "userData.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/mysql/database.hxx>
#include <odb/transaction.hxx>
#include <bite_scaffold/redis.h>
#include <bite_scaffold/odb.h>
#include "userEntity.h"
#include "odb/userEntity-odb.hxx"

namespace userService {

UserData::UserData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis)
    : _db(db)
    , _redis(redis) {
    INF("UserData initialized");
}

bool UserData::saveUserToDb(const UserInfo& userInfo) {
    try {
        // 1. 开启事务
        odb::transaction trans(_db->begin());

        // 2. 查询用户是否已存在
        auto existingUser = _db->query_one<UserEntity>(odb::query<UserEntity>::userId == userInfo._userId);

        if (existingUser) {
            // 3a. 用户已存在，执行更新
            existingUser->setNickname(userInfo._nickname);
            existingUser->setEmail(userInfo._email);
            existingUser->setPassword(userInfo._password);
            existingUser->setStatus(static_cast<unsigned char>(userInfo._status));
            _db->update(*existingUser);
            INF("User updated in DB: userId={}, nickname={}", userInfo._userId, userInfo._nickname);
        } else {
            // 3b. 用户不存在，执行插入
            UserEntity user(userInfo._userId, userInfo._nickname, userInfo._email,
                           userInfo._password, static_cast<unsigned char>(userInfo._status));
            _db->persist(user);
            INF("User saved to DB: userId={}, nickname={}", userInfo._userId, userInfo._nickname);
        }

        // 4. 提交事务
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save user to DB: userId={}, error={}", userInfo._userId, e.what());
        return false;
    }
}

std::optional<UserInfo> UserData::getUserByUserIdFromDb(const std::string& userId) {
    try {
        // 1. 开启事务
        odb::transaction trans(_db->begin());
        // 2. 查询用户信息：SELECT * FROM tbl_user WHERE userId = ?
        auto result = _db->query_one<UserEntity>(odb::query<UserEntity>::userId == userId);
        // 3. 提交事务
        trans.commit();
        // 4. 返回结果
        if (result) {
            UserInfo userInfo;
            userInfo._userId = result->userId();
            userInfo._nickname = result->nickname();
            userInfo._password = result->password();
            userInfo._email = result->email();
            userInfo._status = static_cast<UserStatus>(result->status());

            INF("User found in DB by userId: userId={}", userId);
            return userInfo;
        }

        WRN("User not found in DB by userId: userId={}", userId);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get user from DB by userId: userId={}, error={}", userId, e.what());
        return std::nullopt;
    }
}

std::optional<UserInfo> UserData::getUserByNicknameFromDb(const std::string& nickname) {
    try {
        // 1. 开启事务
        odb::transaction trans(_db->begin());
        // 2. 查询用户信息：SELECT * FROM tbl_user WHERE nickname = ?
        auto result = _db->query_one<UserEntity>(odb::query<UserEntity>::nickname == nickname);
        // 3. 提交事务
        trans.commit();
        // 4. 返回结果
        if (result) {
            UserInfo userInfo;
            userInfo._userId = result->userId();
            userInfo._nickname = result->nickname();
            userInfo._password = result->password();
            userInfo._email = result->email();
            userInfo._status = static_cast<UserStatus>(result->status());

            INF("User found in DB by nickname: nickname={}", nickname);
            return userInfo;
        }

        WRN("User not found in DB by nickname: nickname={}", nickname);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get user from DB by nickname: nickname={}, error={}", nickname, e.what());
        return std::nullopt;
    }
}

std::optional<UserInfo> UserData::getUserByEmailFromDb(const std::string& email) {
    try {
        // 1. 开启事务
        odb::transaction trans(_db->begin());
        // 2. 查询用户信息：SELECT * FROM tbl_user WHERE email = ?
        auto result = _db->query_one<UserEntity>(odb::query<UserEntity>::email == email);
        // 3. 提交事务
        trans.commit();
        // 4. 返回结果
        if (result) {
            UserInfo userInfo;
            userInfo._userId = result->userId();
            userInfo._nickname = result->nickname();
            userInfo._password = result->password();
            userInfo._email = result->email();
            userInfo._status = static_cast<UserStatus>(result->status());

            INF("User found in DB by email: email={}", email);
            return userInfo;
        }

        WRN("User not found in DB by email: email={}", email);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get user from DB by email: email={}, error={}", email, e.what());
        return std::nullopt;
    }
}

bool UserData::existNicknameInDb(const std::string& nickname) {
    auto user = getUserByNicknameFromDb(nickname);
    return user.has_value();
}

bool UserData::existEmailInDb(const std::string& email) {
    auto user = getUserByEmailFromDb(email);
    return user.has_value();
}

bool UserData::saveUserToCache(const UserInfo& userInfo) {
    try {
        // 1. 生成缓存key
        std::string userIdKey = "user:userId:" + userInfo._userId;
        std::string nicknameKey = "user:nickname:" + userInfo._nickname;
        std::string emailKey = "user:email:" + userInfo._email;

        // 2. 序列化用户信息为JSON字符串
        Json::Value userJson;
        userJson["userId"] = userInfo._userId;
        userJson["nickname"] = userInfo._nickname;
        userJson["password"] = userInfo._password;
        userJson["email"] = userInfo._email;
        userJson["status"] = static_cast<int>(userInfo._status);

        // 3. 序列化JSON字符串为Redis缓存值
        auto jsonStr = biteutil::JSON::serialize(userJson);
        if (!jsonStr.has_value()) {
            WRN("Failed to serialize user info for cache");
            return false;
        }

        // 4. 存储缓存值到Redis数据库，并设置过期时间为1h
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            _redis->setex(userIdKey, USER_CACHE_TTL_SECONDS, jsonStr.value());
            _redis->setex(nicknameKey, USER_CACHE_TTL_SECONDS, jsonStr.value());
            _redis->setex(emailKey, USER_CACHE_TTL_SECONDS, jsonStr.value());
        }

        INF("User saved to cache: userId={}", userInfo._userId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save user to cache: userId={}, error={}", userInfo._userId, e.what());
        return false;
    }
}

std::optional<UserInfo> UserData::getUserFromCacheByUserId(const std::string& userId) {
    try {
        std::string key = "user:userId:" + userId;
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("User not found in cache by userId: userId={}", userId);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize user from cache: userId={}", userId);
            return std::nullopt;
        }
        UserInfo userInfo;
        userInfo._userId = jsonOpt.value()["userId"].asString();
        userInfo._nickname = jsonOpt.value()["nickname"].asString();
        userInfo._password = jsonOpt.value()["password"].asString();
        userInfo._email = jsonOpt.value()["email"].asString();
        userInfo._status = static_cast<userService::UserStatus>(jsonOpt.value()["status"].asInt());

        INF("User found in cache by userId: userId={}", userId);
        return userInfo;
    } catch (const std::exception& e) {
        ERR("Failed to get user from cache by userId: userId={}, error={}", userId, e.what());
        return std::nullopt;
    }
}

std::optional<UserInfo> UserData::getUserFromCacheByNickname(const std::string& nickname) {
    try {
        std::string key = "user:nickname:" + nickname;
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("User not found in cache by nickname: nickname={}", nickname);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize user from cache: nickname={}", nickname);
            return std::nullopt;
        }
        UserInfo userInfo;
        userInfo._userId = jsonOpt.value()["userId"].asString();
        userInfo._nickname = jsonOpt.value()["nickname"].asString();
        userInfo._password = jsonOpt.value()["password"].asString();
        userInfo._email = jsonOpt.value()["email"].asString();
        userInfo._status = static_cast<userService::UserStatus>(jsonOpt.value()["status"].asInt());

        INF("User found in cache by nickname: nickname={}", nickname);
        return userInfo;
    } catch (const std::exception& e) {
        ERR("Failed to get user from cache by nickname: nickname={}, error={}", nickname, e.what());
        return std::nullopt;
    }
}

std::optional<UserInfo> UserData::getUserFromCacheByEmail(const std::string& email) {
    try {
        std::string key = "user:email:" + email;
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("User not found in cache by email: email={}", email);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize user from cache: email={}", email);
            return std::nullopt;
        }
        UserInfo userInfo;
        userInfo._userId = jsonOpt.value()["userId"].asString();
        userInfo._nickname = jsonOpt.value()["nickname"].asString();
        userInfo._password = jsonOpt.value()["password"].asString();
        userInfo._email = jsonOpt.value()["email"].asString();
        userInfo._status = static_cast<userService::UserStatus>(jsonOpt.value()["status"].asInt());

        INF("User found in cache by email: email={}", email);
        return userInfo;
    } catch (const std::exception& e) {
        ERR("Failed to get user from cache by email: email={}, error={}", email, e.what());
        return std::nullopt;
    }
}

void UserData::deleteUserCache(const std::string& userId, const std::string& nickname,
                               const std::string& email) {
    try {
        std::string userIdKey = "user:userId:" + userId;
        std::string nicknameKey = "user:nickname:" + nickname;
        std::string emailKey = "user:email:" + email;
        std::lock_guard<std::mutex> lock(_redisMutex);
        _redis->del(userIdKey);
        _redis->del(nicknameKey);
        _redis->del(emailKey);

        INF("User cache deleted: userId={}", userId);
    } catch (const std::exception& e) {
        ERR("Failed to delete user cache: userId={}, error={}", userId, e.what());
    }
}

} // namespace userService