#include <bite_scaffold/log.h>
#include "sessionManager.h"
#include "../common/utils.h"
#include "../common/errorHandler.h"
#include "common.h"


namespace userService {

SessionManager::SessionManager(SessionData* sessionData)
    : _sessionData(sessionData) {
}

std::string SessionManager::createSession(const std::string& userId) {
    // 1. 生成会话id
    std::string sessionId = chat2Data::Utils::generateUuid();
    // 2. 创建会话信息
    SessionInfo sessionInfo;
    sessionInfo._sessionId = sessionId;
    sessionInfo._userId = userId;
    // 3. 保存会话到数据库
    bool saveResult = _sessionData->saveSessionToDb(sessionInfo);
    if (!saveResult) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_CREATE_ERROR);
    }

    INF("Session created: sessionId={}, userId={}", sessionId, userId);
    return sessionId;
}

bool SessionManager::deleteSession(const std::string& sessionId) {
    // 1. 从数据库中删除会话
    bool deleteFromDb = _sessionData->deleteSessionFromDb(sessionId);
    if (!deleteFromDb) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_DELETE_ERROR);
    }

    // 2. 从缓存中删除会话
    bool deleteFromCache = _sessionData->deleteSessionFromCache(sessionId);
    if (!deleteFromCache) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_DELETE_ERROR);
    }

    INF("Session deleted: sessionId={}", sessionId);
    return true;
}

std::string SessionManager::getUserIdBySessionId(const std::string& sessionId) {
    // 1. 从缓存中获取会话信息
    auto sessionCache = _sessionData->getSessionFromCache(sessionId);
    if (sessionCache.has_value()) {
        INF("UserId found (from cache): sessionId={}, userId={}",
            sessionId, sessionCache.value()._userId);
        return sessionCache.value()._userId;
    }

    // 2. 从数据库中获取会话信息, 并添加到缓存
    auto sessionDb = _sessionData->getSessionBySessionIdFromDb(sessionId);
    if (sessionDb.has_value()) {
        bool cacheResult = _sessionData->saveSessionToCache(sessionDb.value());
        if (!cacheResult) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_CACHE_ERROR);
        }
        INF("UserId found (from db, cached): sessionId={}, userId={}",
            sessionId, sessionDb.value()._userId);
        return sessionDb.value()._userId;
    }

    throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
}

} // namespace userService