#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <memory>
#include <bite_scaffold/log.h>
#include "../../../data/userData.h"
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

class UserDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        _userData = std::make_shared<userService::UserData>(MYSQL_CONFIG, REDIS_CONFIG);
    }

    void TearDown() override {
        if (_userData) {
            try {
                // _userData->deleteUserCache("test_user_001", "test_nickname_001", "test_email_001@test.com");
            } catch (const std::exception& e) {
                std::cerr << "Cleanup error: " << e.what() << std::endl;
            }
        }
    }

    std::shared_ptr<userService::UserData> _userData;
};

TEST_F(UserDataTest, SaveUserToDb_Success) {
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Offline;

    bool result = _userData->saveUserToDb(userInfo);
    EXPECT_TRUE(result);
}

TEST_F(UserDataTest, GetUserByUserIdFromDb_Success) {
    // _userData->saveUserToDb(
    //     "test_user_001",
    //     "test_nickname_001",
    //     "test_email_001@test.com",
    //     "encrypted_password_001",
    //     0
    // );

    auto userOpt = _userData->getUserByUserIdFromDb("test_user_001");
    ASSERT_TRUE(userOpt.has_value());

    auto& user = userOpt.value();
    EXPECT_EQ(user._userId, "test_user_001");
    EXPECT_EQ(user._nickname, "test_nickname_001");
    EXPECT_EQ(user._email, "test_email_001@test.com");
    EXPECT_EQ(user._password, "encrypted_password_001");
    EXPECT_EQ(user._status, userService::UserStatus::Offline);
}

TEST_F(UserDataTest, GetUserByNicknameFromDb_Success) {
    // _userData->saveUserToDb(
    //     "test_user_001",
    //     "test_nickname_001",
    //     "test_email_001@test.com",
    //     "encrypted_password_001",
    //     1
    // );

    auto userOpt = _userData->getUserByNicknameFromDb("test_nickname_001");
    ASSERT_TRUE(userOpt.has_value());

    auto& user = userOpt.value();
    EXPECT_EQ(user._userId, "test_user_001");
    EXPECT_EQ(user._nickname, "test_nickname_001");
    EXPECT_EQ(user._status, userService::UserStatus::Offline);
}

TEST_F(UserDataTest, GetUserByEmailFromDb_Success) {
    // _userData->saveUserToDb(
    //     "test_user_001",
    //     "test_nickname_001",
    //     "test_email_001@test.com",
    //     "encrypted_password_001",
    //     1
    // );

    auto userOpt = _userData->getUserByEmailFromDb("test_email_001@test.com");
    ASSERT_TRUE(userOpt.has_value());

    auto& user = userOpt.value();
    EXPECT_EQ(user._userId, "test_user_001");
    EXPECT_EQ(user._email, "test_email_001@test.com");
}

TEST_F(UserDataTest, GetUserByUserIdFromDb_NotFound) {
    auto userOpt = _userData->getUserByUserIdFromDb("non_existent_user");
    EXPECT_FALSE(userOpt.has_value());
}

TEST_F(UserDataTest, GetUserByNicknameFromDb_NotFound) {
    auto userOpt = _userData->getUserByNicknameFromDb("non_existent_nickname");
    EXPECT_FALSE(userOpt.has_value());
}

TEST_F(UserDataTest, GetUserByEmailFromDb_NotFound) {
    auto userOpt = _userData->getUserByEmailFromDb("non_existent@email.com");
    EXPECT_FALSE(userOpt.has_value());
}

TEST_F(UserDataTest, ExistNicknameInDb_True) {
    // _userData->saveUserToDb(
    //     "test_user_001",
    //     "test_nickname_001",
    //     "test_email_001@test.com",
    //     "encrypted_password_001",
    //     0
    // );

    EXPECT_TRUE(_userData->existNicknameInDb("test_nickname_001"));
}

TEST_F(UserDataTest, ExistNicknameInDb_False) {
    EXPECT_FALSE(_userData->existNicknameInDb("non_existent_nickname"));
}

TEST_F(UserDataTest, ExistEmailInDb_True) {
    // _userData->saveUserToDb(
    //     "test_user_001",
    //     "test_nickname_001",
    //     "test_email_001@test.com",
    //     "encrypted_password_001",
    //     0
    // );

    EXPECT_TRUE(_userData->existEmailInDb("test_email_001@test.com"));
}

TEST_F(UserDataTest, ExistEmailInDb_False) {
    EXPECT_FALSE(_userData->existEmailInDb("non_existent@email.com"));
}

TEST_F(UserDataTest, SaveUserToCache_Success) {
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Offline;

    bool result = _userData->saveUserToCache(userInfo);
    EXPECT_TRUE(result);

    auto userOpt = _userData->getUserFromCacheByUserId("test_user_001");
    ASSERT_TRUE(userOpt.has_value());
    EXPECT_EQ(userOpt.value()._nickname, "test_nickname_001");
}

TEST_F(UserDataTest, GetUserFromCacheByUserId_Success) {
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Offline;

    _userData->saveUserToCache(userInfo);

    auto userOpt = _userData->getUserFromCacheByUserId("test_user_001");
    ASSERT_TRUE(userOpt.has_value());
    EXPECT_EQ(userOpt.value()._userId, "test_user_001");
    EXPECT_EQ(userOpt.value()._nickname, "test_nickname_001");
}

TEST_F(UserDataTest, GetUserFromCacheByNickname_Success) {
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Online;

    _userData->saveUserToCache(userInfo);

    auto userOpt = _userData->getUserFromCacheByNickname("test_nickname_001");
    ASSERT_TRUE(userOpt.has_value());
    EXPECT_EQ(userOpt.value()._nickname, "test_nickname_001");
    EXPECT_EQ(userOpt.value()._status, userService::UserStatus::Online);
}

TEST_F(UserDataTest, GetUserFromCacheByEmail_Success) {
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Online;

    _userData->saveUserToCache(userInfo);

    auto userOpt = _userData->getUserFromCacheByEmail("test_email_001@test.com");
    ASSERT_TRUE(userOpt.has_value());
    EXPECT_EQ(userOpt.value()._email, "test_email_001@test.com");
}

TEST_F(UserDataTest, GetUserFromCacheByUserId_NotFound) {
    auto userOpt = _userData->getUserFromCacheByUserId("non_existent_user");
    EXPECT_FALSE(userOpt.has_value());
}

TEST_F(UserDataTest, DeleteUserCache_Success) {
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Offline;

    _userData->saveUserToCache(userInfo);

    _userData->deleteUserCache("test_user_001", "test_nickname_001", "test_email_001@test.com");

    auto userOpt = _userData->getUserFromCacheByUserId("test_user_001");
    EXPECT_FALSE(userOpt.has_value());
}

} // namespace


void saveUserInfoToCache() {
    auto _userData = std::make_shared<userService::UserData>(MYSQL_CONFIG, REDIS_CONFIG);
    userService::UserInfo userInfo;
    userInfo._userId = "test_user_001";
    userInfo._nickname = "test_nickname_001";
    userInfo._email = "test_email_001@test.com";
    userInfo._password = "encrypted_password_001";
    userInfo._status = userService::UserStatus::Offline;

    _userData->saveUserToCache(userInfo);

    auto userOpt = _userData->getUserFromCacheByUserId("test_user_001");
    if(userOpt.has_value()){
        INF("userId = {} nickname = {}", userOpt.value()._userId, userOpt.value()._nickname);
    }
}

int main(int argc, char** argv) {
    // 初始化日志库
    bitelog::bitelog_init();
    // 初始化gtest测试框架
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();

    return 0;
}