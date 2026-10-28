#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <memory>
#include <bite_scaffold/log.h>
#include "../../../data/sessionData.h"
#include "../../../svc_userService/common.h"

namespace {

const userService::MysqlConfig MYSQL_CONFIG = {
    ._host = "127.0.0.1",
    ._user = "root",
    ._passwd = "CHANGE_ME",
    ._db = "chat2Data",
    ._port = 3306,
    ._cset = "utf8mb4",
    ._connectionPoolSize = 3
};

const userService::RedisConfig REDIS_CONFIG = {
    ._host = "127.0.0.1",
    ._port = 6379,
    ._passwd = "CHANGE_ME",
    ._db = 0,
    ._connectionPoolSize = 3
};

class SessionDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        _sessionData = std::make_shared<userService::SessionData>(MYSQL_CONFIG, REDIS_CONFIG);
    }

    void TearDown() override {
        if (_sessionData) {
            try {
                _sessionData->deleteSessionFromCache("test_session_001");
            } catch (const std::exception& e) {
                std::cerr << "Cleanup error: " << e.what() << std::endl;
            }
        }
    }

    std::shared_ptr<userService::SessionData> _sessionData;
};

TEST_F(SessionDataTest, SaveSessionToDb_Success) {
    userService::SessionInfo sessionInfo;
    sessionInfo._sessionId = "test_session_001";
    sessionInfo._userId = "test_user_001";

    bool result = _sessionData->saveSessionToDb(sessionInfo);
    EXPECT_TRUE(result);
}

TEST_F(SessionDataTest, GetSessionBySessionIdFromDb_Success) {
    //_sessionData->saveSessionToDb("test_session_001", "test_user_001");

    auto sessionOpt = _sessionData->getSessionBySessionIdFromDb("test_session_001");
    ASSERT_TRUE(sessionOpt.has_value());

    auto& session = sessionOpt.value();
    EXPECT_EQ(session._sessionId, "test_session_001");
    EXPECT_EQ(session._userId, "test_user_001");
}

TEST_F(SessionDataTest, GetSessionBySessionIdFromDb_NotFound) {
    auto sessionOpt = _sessionData->getSessionBySessionIdFromDb("non_existent_session");
    EXPECT_FALSE(sessionOpt.has_value());
}

TEST_F(SessionDataTest, DeleteSessionFromDb_Success) {
    //_sessionData->saveSessionToDb("test_session_001", "test_user_001");

    bool result = _sessionData->deleteSessionFromDb("test_session_001");
    EXPECT_TRUE(result);

    auto sessionOpt = _sessionData->getSessionBySessionIdFromDb("test_session_001");
    EXPECT_FALSE(sessionOpt.has_value());
}

TEST_F(SessionDataTest, SaveSessionToCache_Success) {
    userService::SessionInfo sessionInfo;
    sessionInfo._sessionId = "test_session_001";
    sessionInfo._userId = "test_user_001";

    bool result = _sessionData->saveSessionToCache(sessionInfo);
    EXPECT_TRUE(result);

    auto sessionOpt = _sessionData->getSessionFromCache("test_session_001");
    ASSERT_TRUE(sessionOpt.has_value());
    EXPECT_EQ(sessionOpt.value()._userId, "test_user_001");
}

TEST_F(SessionDataTest, GetSessionFromCache_Success) {
    userService::SessionInfo sessionInfo;
    sessionInfo._sessionId = "test_session_001";
    sessionInfo._userId = "test_user_001";

    _sessionData->saveSessionToCache(sessionInfo);

    auto sessionOpt = _sessionData->getSessionFromCache("test_session_001");
    ASSERT_TRUE(sessionOpt.has_value());
    EXPECT_EQ(sessionOpt.value()._sessionId, "test_session_001");
    EXPECT_EQ(sessionOpt.value()._userId, "test_user_001");
}

TEST_F(SessionDataTest, GetSessionFromCache_NotFound) {
    auto sessionOpt = _sessionData->getSessionFromCache("non_existent_session");
    EXPECT_FALSE(sessionOpt.has_value());
}

TEST_F(SessionDataTest, DeleteSessionFromCache_Success) {
    userService::SessionInfo sessionInfo;
    sessionInfo._sessionId = "test_session_001";
    sessionInfo._userId = "test_user_001";

    _sessionData->saveSessionToCache(sessionInfo);

    _sessionData->deleteSessionFromCache("test_session_001");

    auto sessionOpt = _sessionData->getSessionFromCache("test_session_001");
    EXPECT_FALSE(sessionOpt.has_value());
}

} // namespace

int main(int argc, char** argv) {
    // 初始化日志库
    bitelog::bitelog_init();
    // 初始化gtest测试框架
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}