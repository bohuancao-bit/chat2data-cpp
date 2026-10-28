#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <odb/mysql/database.hxx>
#include <sw/redis++/redis.h>
#include "chatSessionEntity.h"

namespace aiService {

class ChatSessionData {
public:
    ChatSessionData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis);

    // MySQL数据库操作接口
    // 保存或更新聊天会话到MySQL
    bool saveChatSessionToDb(const ChatSessionEntity& chatSession);
    // 通过会话Id从MySQL获取聊天会话
    std::optional<ChatSessionEntity> getChatSessionBySessionIdFromDb(const std::string& chatSessionId);
    // 通过用户Id从MySQL获取该用户所有聊天会话
    std::vector<ChatSessionEntity> getChatSessionByUserIdFromDb(const std::string& userId);
    // 通过会话Id从MySQL删除聊天会话
    bool deleteChatSessionBySessionIdFromDb(const std::string& chatSessionId);

    // Redis缓存操作接口
    // 保存聊天会话到Redis缓存
    bool saveChatSessionToCache(const ChatSessionEntity& chatSession);
    // 通过会话Id从Redis缓存获取聊天会话
    std::optional<ChatSessionEntity> getChatSessionFromCacheBySessionId(const std::string& chatSessionId);
    // 通过会话Id从Redis缓存删除聊天会话
    void deleteChatSessionFromCache(const std::string& chatSessionId);

private:
    std::shared_ptr<odb::database> _db;            // 操作MySQL数据库的实例指针
    std::shared_ptr<sw::redis::Redis> _redis;      // 操作Redis数据库的实例指针
    std::mutex _redisMutex;                        // 用于保护Redis数据库操作的互斥锁
    const static int CHAT_SESSION_CACHE_TTL_SECONDS = 3 * 24 * 60 * 60; // 缓存过期时间：3天
};

} // namespace aiService
