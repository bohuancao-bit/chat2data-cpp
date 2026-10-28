#include "chatSessionData.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/transaction.hxx>
#include "odb/chatSessionEntity-odb.hxx"

namespace aiService {

ChatSessionData::ChatSessionData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis)
    : _db(db)
    , _redis(redis) {
    INF("ChatSessionData initialized");
}

// 保存或更新聊天会话到MySQL数据库
bool ChatSessionData::saveChatSessionToDb(const ChatSessionEntity& chatSession) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询chatSessionId对应的会话是否存在
        //    等价的SQL语句：SELECT * FROM tbl_chatSession WHERE chat_session_id = ? 
        auto result = _db->query_one<ChatSessionEntity>(
            odb::query<ChatSessionEntity>::chatSessionId == chatSession.chatSessionId());

        // 3. 检查会话是否存在
        if (result) {
            // 3.1 如果存在，则更新会话信息
            result->setUserId(chatSession.userId());
            result->setTitle(chatSession.title());
            result->setCreateTime(chatSession.createTime());
            result->setUpdateTime(chatSession.updateTime());
            result->setMessageCount(chatSession.messageCount());
            result->setModelName(chatSession.modelName());
            result->setFileId(chatSession.fileId());
            result->setSessionType(chatSession.sessionType());
            result->setDbConnectionInfo(chatSession.dbConnectionInfo());
            // 3.2 更新数据库中的会话信息
            _db->update(*result);
            INF("ChatSession updated in DB: chatSessionId={}", chatSession.chatSessionId());
        } else {
            // 3.3 如果不存在，则插入新的会话信息
            _db->persist(const_cast<ChatSessionEntity&>(chatSession));
            INF("ChatSession saved to DB: chatSessionId={}", chatSession.chatSessionId());
        }
        // 4. 提交事务
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save ChatSession to DB: chatSessionId={}, error={}", chatSession.chatSessionId(), e.what());
        return false;
    }
}

// 通过会话Id从MySQL数据库中查询聊天会话
std::optional<ChatSessionEntity> ChatSessionData::getChatSessionBySessionIdFromDb(const std::string& chatSessionId) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询chatSessionId对应的会话是否存在
        //    等价的SQL语句：SELECT * FROM tbl_chatSession WHERE chat_session_id = ? 
        auto result = _db->query_one<ChatSessionEntity>(
            odb::query<ChatSessionEntity>::chatSessionId == chatSessionId);
        // 3. 提交事务
        trans.commit();
        // 4. 检查查询结果是否存在，存在则返回会话信息
        if (result) {
            INF("ChatSession found in DB by chatSessionId: chatSessionId={}", chatSessionId);
            return *result;
        }
        WRN("ChatSession not found in DB by chatSessionId: chatSessionId={}", chatSessionId);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get ChatSession from DB by chatSessionId: chatSessionId={}, error={}",
            chatSessionId, e.what());
        return std::nullopt;
    }
}

// 通过用户Id从MySQL数据库中查询该用户所有聊天会话
std::vector<ChatSessionEntity> ChatSessionData::getChatSessionByUserIdFromDb(const std::string& userId) {
    std::vector<ChatSessionEntity> result;
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询userId对应的所有会话
        //    等价的SQL语句：SELECT * FROM tbl_chatSession WHERE user_id = ? 
        odb::result<ChatSessionEntity> queryResult = _db->query<ChatSessionEntity>(
            odb::query<ChatSessionEntity>::userId == userId);
        // 3. 遍历查询结果，将每个会话添加到结果向量中
        for (const auto& session : queryResult) {
            result.push_back(session);
        }
        // 4. 提交事务
        trans.commit();
        INF("ChatSessions found in DB by userId: userId={}, count={}", userId, result.size());
        return result;
    } catch (const std::exception& e) {
        ERR("Failed to get ChatSessions from DB by userId: userId={}, error={}", userId, e.what());
        return result;
    }
}

// 通过会话Id从MySQL数据库中删除聊天会话
bool ChatSessionData::deleteChatSessionBySessionIdFromDb(const std::string& chatSessionId) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 删除chatSessionId对应的会话信息
        //    等价的SQL语句：DELETE FROM tbl_chatSession WHERE chat_session_id = ? 
        _db->erase_query<ChatSessionEntity>(odb::query<ChatSessionEntity>::chatSessionId == chatSessionId);
        // 3. 提交事务
        trans.commit();
        INF("ChatSession deleted from DB: chatSessionId={}", chatSessionId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to delete ChatSession from DB: chatSessionId={}, error={}", chatSessionId, e.what());
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 保存聊天会话到Redis缓存
bool ChatSessionData::saveChatSessionToCache(const ChatSessionEntity& chatSession) {
    try {
        // 1. 构建缓存键，格式为"chat_session:chatSessionId"
        std::string key = "chat_session:" + chatSession.chatSessionId();

        // 2. 构建缓存值，缓存值以Json字符串方式保存
        Json::Value sessionJson;
        sessionJson["chatSessionId"] = chatSession.chatSessionId();
        sessionJson["userId"] = chatSession.userId();
        sessionJson["title"] = chatSession.title();
        sessionJson["createTime"] = static_cast<Json::Int64>(chatSession.createTime());
        sessionJson["updateTime"] = static_cast<Json::Int64>(chatSession.updateTime());
        sessionJson["messageCount"] = chatSession.messageCount();
        sessionJson["modelName"] = chatSession.modelName();
        sessionJson["fileId"] = chatSession.fileId();
        sessionJson["sessionType"] = chatSession.sessionType();
        sessionJson["dbConnectionInfo"] = chatSession.dbConnectionInfo();

        // 3. 序列化Json对象为字符串
        auto jsonStr = biteutil::JSON::serialize(sessionJson);
        if (!jsonStr.has_value()) {
            WRN("Failed to serialize ChatSession for cache: chatSessionId={}", chatSession.chatSessionId());
            return false;
        }

        // 4. 将缓存值保存到Redis缓存中，过期时间为CHAT_SESSION_CACHE_TTL_SECONDS秒
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            _redis->setex(key, CHAT_SESSION_CACHE_TTL_SECONDS, jsonStr.value());
        }
        // 5. 返回结果
        INF("ChatSession saved to cache: chatSessionId={}", chatSession.chatSessionId());
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save ChatSession to cache: chatSessionId={}, error={}",
            chatSession.chatSessionId(), e.what());
        return false;
    }
}

// 通过会话Id从Redis缓存中获取聊天会话
std::optional<ChatSessionEntity> ChatSessionData::getChatSessionFromCacheBySessionId(const std::string& chatSessionId) {
    try {
        // 1. 构建缓存键，格式为"chat_session:chatSessionId"
        std::string key = "chat_session:" + chatSessionId;
        // 2. 从Redis缓存中获取缓存值
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("ChatSession not found in cache by chatSessionId: chatSessionId={}", chatSessionId);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        // 3. 反序列化Json字符串为Json对象
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize ChatSession from cache: chatSessionId={}", chatSessionId);
            return std::nullopt;
        }

        // 4. 解析Json对象并构建ChatSessionEntity
        ChatSessionEntity chatSession;
        chatSession.setChatSessionId(jsonOpt.value()["chatSessionId"].asString());
        chatSession.setUserId(jsonOpt.value()["userId"].asString());
        chatSession.setTitle(jsonOpt.value()["title"].asString());
        chatSession.setCreateTime(jsonOpt.value()["createTime"].asInt64());
        chatSession.setUpdateTime(jsonOpt.value()["updateTime"].asInt64());
        chatSession.setMessageCount(jsonOpt.value()["messageCount"].asInt());
        chatSession.setModelName(jsonOpt.value()["modelName"].asString());
        chatSession.setFileId(jsonOpt.value()["fileId"].asString());
        chatSession.setSessionType(jsonOpt.value()["sessionType"].asString());
        chatSession.setDbConnectionInfo(jsonOpt.value()["dbConnectionInfo"].asString());

        INF("ChatSession found in cache by chatSessionId: chatSessionId={}", chatSessionId);
        return chatSession;
    } catch (const std::exception& e) {
        ERR("Failed to get ChatSession from cache by chatSessionId: chatSessionId={}, error={}",
            chatSessionId, e.what());
        return std::nullopt;
    }
}

// 通过会话Id从Redis缓存中删除聊天会话
void ChatSessionData::deleteChatSessionFromCache(const std::string& chatSessionId) {
    try {
        // 1. 构建缓存键，格式为"chat_session:chatSessionId"
        std::string key = "chat_session:" + chatSessionId;
        // 2. 从Redis缓存中删除会话信息
        std::lock_guard<std::mutex> lock(_redisMutex);
        auto res = _redis->del(key);
        if (res == 0) {
            INF("ChatSession not found in cache: chatSessionId={}", chatSessionId);
        } else {
            INF("ChatSession deleted from cache: chatSessionId={}", chatSessionId);
        }
    } catch (const std::exception& e) {
        ERR("Failed to delete ChatSession from cache: chatSessionId={}, error={}",
            chatSessionId, e.what());
    }
}

} // namespace aiService
