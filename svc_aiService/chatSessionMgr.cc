#include <bite_scaffold/log.h>
#include "chatSessionMgr.h"


namespace aiService {

ChatSessionMgr::ChatSessionMgr(std::shared_ptr<ChatSessionData> chatSessionData)
    : _chatSessionData(chatSessionData) {
    INF("ChatSessionMgr initialized");
}

bool ChatSessionMgr::isSessionOwnedByUser(const std::string& chatSessionId, const std::string& userId) {
    // 先从缓存获取
    auto cachedSession = _chatSessionData->getChatSessionFromCacheBySessionId(chatSessionId);
    if (cachedSession.has_value()) {
        bool owned = (cachedSession.value().userId() == userId);
        INF("Session ownership check from cache: chatSessionId={}, userId={}, owned={}",
            chatSessionId, userId, owned);
        return owned;
    }

    // 缓存未命中，从MySQL获取
    auto dbSession = _chatSessionData->getChatSessionBySessionIdFromDb(chatSessionId);
    if (!dbSession.has_value()) {
        WRN("Session not found: chatSessionId={}", chatSessionId);
        return false;
    }

    // 检查用户是否属于会话所有者
    bool owned = (dbSession.value().userId() == userId);
    INF("Session ownership check from DB: chatSessionId={}, userId={}, owned={}", chatSessionId, userId, owned);
    return owned;
}

bool ChatSessionMgr::saveChatSession(const ChatSessionInfo& chatSessionInfo) {
    // 1. 构建ChatSessionEntity
    ChatSessionEntity entity;
    entity.setChatSessionId(chatSessionInfo._chatSessionId);
    entity.setUserId(chatSessionInfo._userId);
    entity.setTitle(chatSessionInfo._title);
    entity.setCreateTime(chatSessionInfo._createTime);
    entity.setUpdateTime(chatSessionInfo._updateTime);
    entity.setMessageCount(chatSessionInfo._messageCount);
    entity.setModelName(chatSessionInfo._modelName);
    entity.setFileId(chatSessionInfo._fileId);
    entity.setSessionType(chatSessionInfo._sessionType);
    entity.setDbConnectionInfo(chatSessionInfo._dbConnectionInfo);

    // 2. 保存或更新到MySQL
    bool success = _chatSessionData->saveChatSessionToDb(entity);
    if (!success) {
        ERR("Failed to save ChatSession to DB: chatSessionId={}", chatSessionInfo._chatSessionId);
        return false;
    }

    // 3. 删除Redis缓存（写策略：写DB成功后删除缓存）
    _chatSessionData->deleteChatSessionFromCache(chatSessionInfo._chatSessionId);

    INF("ChatSession saved: chatSessionId={}", chatSessionInfo._chatSessionId);
    return true;
}

std::vector<ChatSessionInfo> ChatSessionMgr::getChatSessionsByUserId(const std::string& userId) {
    std::vector<ChatSessionInfo> result;

    // 从MySQL获取用户所有会话, 因为缓存可能不完整
    auto chatSessions = _chatSessionData->getChatSessionByUserIdFromDb(userId);

    for (const auto& chatSession : chatSessions) {
        ChatSessionInfo info;
        info._chatSessionId = chatSession.chatSessionId();
        info._userId = chatSession.userId();
        info._title = chatSession.title();
        info._createTime = chatSession.createTime();
        info._updateTime = chatSession.updateTime();
        info._messageCount = chatSession.messageCount();
        info._modelName = chatSession.modelName();
        info._fileId = chatSession.fileId();
        info._sessionType = chatSession.sessionType();
        info._dbConnectionInfo = chatSession.dbConnectionInfo();
        result.push_back(info);
    }

    INF("Get ChatSessions by userId: userId={}, count={}", userId, result.size());
    return result;
}

std::optional<ChatSessionInfo> ChatSessionMgr::getChatSessionBySessionId(const std::string& sessionId) {
    // 先从缓存获取
    auto cachedSession = _chatSessionData->getChatSessionFromCacheBySessionId(sessionId);
    if (cachedSession.has_value()) {
        const auto& chatSession = cachedSession.value();
        ChatSessionInfo info;
        info._chatSessionId = chatSession.chatSessionId();
        info._userId = chatSession.userId();
        info._title = chatSession.title();
        info._createTime = chatSession.createTime();
        info._updateTime = chatSession.updateTime();
        info._messageCount = chatSession.messageCount();
        info._modelName = chatSession.modelName();
        info._fileId = chatSession.fileId();
        info._sessionType = chatSession.sessionType();
        info._dbConnectionInfo = chatSession.dbConnectionInfo();
        INF("ChatSession found in cache: sessionId={}", sessionId);
        return info;
    }

    // 缓存未命中，从MySQL获取
    auto dbSession = _chatSessionData->getChatSessionBySessionIdFromDb(sessionId);
    if (!dbSession.has_value()) {
        WRN("ChatSession not found: sessionId={}", sessionId);
        return std::nullopt;
    }

    const auto& chatSession = dbSession.value();
    ChatSessionInfo info;
    info._chatSessionId = chatSession.chatSessionId();
    info._userId = chatSession.userId();
    info._title = chatSession.title();
    info._createTime = chatSession.createTime();
    info._updateTime = chatSession.updateTime();
    info._messageCount = chatSession.messageCount();
    info._modelName = chatSession.modelName();
    info._fileId = chatSession.fileId();
    info._sessionType = chatSession.sessionType();
    info._dbConnectionInfo = chatSession.dbConnectionInfo();

    // 回填缓存
    _chatSessionData->saveChatSessionToCache(chatSession);

    INF("ChatSession found in DB: sessionId={}", sessionId);
    return info;
}

bool ChatSessionMgr::deleteChatSession(const std::string& chatSessionId, const std::string& userId) {
    // 1. 校验用户权限
    if (!isSessionOwnedByUser(chatSessionId, userId)) {
        WRN("User does not own the session: chatSessionId={}, userId={}", chatSessionId, userId);
        return false;
    }

    // 2. 从MySQL删除会话
    bool success = _chatSessionData->deleteChatSessionBySessionIdFromDb(chatSessionId);
    if (!success) {
        ERR("Failed to delete ChatSession from DB: chatSessionId={}", chatSessionId);
        return false;
    }

    // 3. 删除Redis缓存
    _chatSessionData->deleteChatSessionFromCache(chatSessionId);

    INF("ChatSession deleted: sessionId={}", chatSessionId);
    return true;
}

bool ChatSessionMgr::associateFileId(const std::string& chatSessionId, const std::string& fileId, const std::string& userId) {
    // 1. 校验用户权限
    if (!isSessionOwnedByUser(chatSessionId, userId)) {
        WRN("User does not own the session: chatSessionId={}, userId={}", chatSessionId, userId);
        return false;
    }

    // 2. 获取当前会话信息（先从缓存获取，缓存未命中再从数据库获取）
    auto sessionOpt = getChatSessionBySessionId(chatSessionId);
    if (!sessionOpt.has_value()) {
        WRN("ChatSession not found: chatSessionId={}", chatSessionId);
        return false;
    }

    // 3. 更新会话信息
    auto chatSession = sessionOpt.value();
    chatSession._fileId = fileId;
    bool success = saveChatSession(chatSession);
    if (!success) {
        ERR("Failed to update ChatSession fileId: chatSessionId={}, fileId={}", chatSessionId, fileId);
        return false;
    }

    // 4. 删除缓存
    _chatSessionData->deleteChatSessionFromCache(chatSessionId);

    INF("ChatSession fileId associated: chatSessionId={}, fileId={}", chatSessionId, fileId);
    return true;
}

} // namespace aiService
