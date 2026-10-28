#include "sessionData.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/mysql/database.hxx>
#include <odb/transaction.hxx>
#include <bite_scaffold/redis.h>
#include <bite_scaffold/odb.h>
#include "sessionEntity.h"
#include "odb/sessionEntity-odb.hxx"

namespace userService {

SessionData::SessionData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis)
    : _db(db)
    , _redis(redis) {
}

bool SessionData::saveSessionToDb(const SessionInfo& sessionInfo) {
    try {
        SessionEntity session(sessionInfo._sessionId, sessionInfo._userId);
        // 1. 开启事务
        odb::transaction trans(_db->begin());
        // 2. 插入会话信息：INSERT INTO tbl_session (sessionId, userId) VALUES (...)
        _db->persist(session);
        // 3. 提交事务
        trans.commit();

        INF("Session saved to DB: sessionId={}, userId={}", sessionInfo._sessionId, sessionInfo._userId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save session to DB: sessionId={}, error={}", sessionInfo._sessionId, e.what());
        return false;
    }
}

std::optional<SessionInfo> SessionData::getSessionBySessionIdFromDb(const std::string& sessionId) {
    try {
        // 1. 开启事务
        odb::transaction trans(_db->begin());
        // 2. 查询会话信息：SELECT * FROM tbl_session WHERE sessionId = ?
        auto result = _db->query_one<SessionEntity>(odb::query<SessionEntity>::sessionId == sessionId);
        // 3. 提交事务
        trans.commit();
        // 4. 返回结果
        if (result) {
            SessionInfo sessionInfo;
            sessionInfo._sessionId = result->sessionId();
            sessionInfo._userId = result->userId();

            INF("Session found in DB: sessionId={}", sessionId);
            return sessionInfo;
        }

        WRN("Session not found in DB: sessionId={}", sessionId);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get session from DB: sessionId={}, error={}", sessionId, e.what());
        return std::nullopt;
    }
}

bool SessionData::deleteSessionFromDb(const std::string& sessionId) {
    try {
        // 1. 开启事务
        odb::transaction trans(_db->begin());
        // 2. 删除会话信息：DELETE FROM tbl_session WHERE sessionId = ?
        _db->erase_query<SessionEntity>(odb::query<SessionEntity>::sessionId == sessionId);
        // 3. 提交事务
        trans.commit();

        INF("Session deleted from DB: sessionId={}", sessionId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to delete session from DB: sessionId={}, error={}", sessionId, e.what());
        return false;
    }
}

bool SessionData::saveSessionToCache(const SessionInfo& sessionInfo) {
    try {
        std::string key = "session:" + sessionInfo._sessionId;
        Json::Value sessionJson;
        sessionJson["sessionId"] = sessionInfo._sessionId;
        sessionJson["userId"] = sessionInfo._userId;
        auto jsonStr = biteutil::JSON::serialize(sessionJson);
        if (!jsonStr.has_value()) {
            WRN("Failed to serialize session info for cache");
            return false;
        }

        std::lock_guard<std::mutex> lock(_redisMutex);
        _redis->setex(key, SESSION_CACHE_TTL_SECONDS, jsonStr.value());

        INF("Session saved to cache: sessionId={}", sessionInfo._sessionId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save session to cache: sessionId={}, error={}", sessionInfo._sessionId, e.what());
        return false;
    }
}

std::optional<SessionInfo> SessionData::getSessionFromCache(const std::string& sessionId) {
    try {
        std::string key = "session:" + sessionId;
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("Session not found in cache: sessionId={}", sessionId);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize session from cache: sessionId={}", sessionId);
            return std::nullopt;
        }

        SessionInfo sessionInfo;
        sessionInfo._sessionId = jsonOpt.value()["sessionId"].asString();
        sessionInfo._userId = jsonOpt.value()["userId"].asString();

        INF("Session found in cache: sessionId={}", sessionId);
        return sessionInfo;
    } catch (const std::exception& e) {
        ERR("Failed to get session from cache: sessionId={}, error={}", sessionId, e.what());
        return std::nullopt;
    }
}

bool SessionData::deleteSessionFromCache(const std::string& sessionId) {
    try {
        std::string key = "session:" + sessionId;
        std::lock_guard<std::mutex> lock(_redisMutex);
        auto res = _redis->del(key);
        if (res == 0) {
            INF("Session not found in cache: sessionId={}", sessionId);
            return true;
        }

        INF("Session deleted from cache: sessionId={}", sessionId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to delete session from cache: sessionId={}, error={}", sessionId, e.what());
        return false;
    }
}

} // namespace userService