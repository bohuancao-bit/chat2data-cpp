#pragma once

#include <memory>
#include <string>
#include <optional>
#include <mutex>
#include <odb/mysql/database.hxx>
#include <sw/redis++/redis.h>
#include "../svc_userService/common.h"

namespace userService {

class SessionData {
public:
    /**
     * @brief  构造函数，根据配置初始化MySQL和Redis连接
     * @param  db  MySQL数据库连接
     * @param  redis  Redis数据库连接
     */
    SessionData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief  保存会话信息到MySQL
     * @param  sessionInfo  会话信息结构体
     * @return 成功返回true，失败返回false
     */
    bool saveSessionToDb(const SessionInfo& sessionInfo);

    /**
     * @brief  从MySQL通过会话ID获取会话信息
     * @param  sessionId  会话ID
     * @return 存在返回会话信息，不存在返回std::nullopt
     */
    std::optional<SessionInfo> getSessionBySessionIdFromDb(const std::string& sessionId);

    /**
     * @brief  从MySQL删除指定会话信息
     * @param  sessionId  会话ID
     * @return 成功返回true，失败返回false
     */
    bool deleteSessionFromDb(const std::string& sessionId);

    /**
     * @brief  保存会话信息到Redis缓存
     * @param  sessionInfo  会话信息结构体
     * @return 成功返回true，失败返回false
     */
    bool saveSessionToCache(const SessionInfo& sessionInfo);

    /**
     * @brief  从Redis缓存通过会话ID获取会话信息
     * @param  sessionId  会话ID
     * @return 存在返回会话信息，不存在返回std::nullopt
     */
    std::optional<SessionInfo> getSessionFromCache(const std::string& sessionId);

    /**
     * @brief  从Redis缓存删除指定会话信息
     * @param  sessionId  会话ID
     * @return 成功返回true，失败返回false
     */
    bool deleteSessionFromCache(const std::string& sessionId);

private:
    std::shared_ptr<odb::database> _db;   // MySQL数据库连接
    std::shared_ptr<sw::redis::Redis> _redis;    // Redis数据库连接
    std::mutex _redisMutex;              // Redis操作互斥锁
    const static int SESSION_CACHE_TTL_SECONDS = 3 * 24 * 60 * 60;  // 会话缓存过期时间为3天
};

} // namespace userService