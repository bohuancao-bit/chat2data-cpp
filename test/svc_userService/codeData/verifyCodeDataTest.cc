#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <memory>
#include <sw/redis++/redis.h>
#include <bite_scaffold/redis.h>
#include <bite_scaffold/log.h>
#include "../../../data/verifyCodeData.h"

namespace {

const std::string REDIS_HOST = "127.0.0.1";
const int REDIS_PORT = 6379;
const std::string REDIS_PASSWD = "CHANGE_ME";
const int REDIS_DB = 0;


class VerifyCodeDataTest : public ::testing::Test {
protected:
    void SetUp() override {

        userService::RedisConfig settings;
        settings._host = REDIS_HOST;
        settings._port = REDIS_PORT;
        settings._passwd = REDIS_PASSWD;
        settings._db = REDIS_DB;
        settings._connectionPoolSize = 3;
        _verifyCodeData = std::make_shared<userService::VerifyCodeData>(settings);
    }

    void TearDown() override {

            try {
                
            } catch (const std::exception& e) {
                std::cerr << "Cleanup error: " << e.what() << std::endl;
            }
    }

    std::shared_ptr<userService::VerifyCodeData> _verifyCodeData;
};

TEST_F(VerifyCodeDataTest, SaveVerifyCode_Success) {
    userService::VerifyCodeInfo verifyCodeInfo;
    verifyCodeInfo._codeId = "test_code_001";
    verifyCodeInfo._verifyCode = "123456";
    verifyCodeInfo._email = "test_email@test.com";
    verifyCodeInfo._createTime = "2024-01-01 00:00:00";

    bool result = _verifyCodeData->saveVerifyCodeToCache(verifyCodeInfo);
    EXPECT_TRUE(result);
}

TEST_F(VerifyCodeDataTest, GetVerifyCode_Success) {
    userService::VerifyCodeInfo verifyCodeInfo;
    verifyCodeInfo._codeId = "test_code_001";
    verifyCodeInfo._verifyCode = "123456";
    verifyCodeInfo._email = "test_email@test.com";
    verifyCodeInfo._createTime = "2024-01-01 00:00:00";

    _verifyCodeData->saveVerifyCodeToCache(verifyCodeInfo);

    auto codeOpt = _verifyCodeData->getVerifyCodeFromCache("test_code_001");
    ASSERT_TRUE(codeOpt.has_value());

    auto& code = codeOpt.value();
    EXPECT_EQ(code._codeId, "test_code_001");
    EXPECT_EQ(code._verifyCode, "123456");
    EXPECT_EQ(code._email, "test_email@test.com");
    EXPECT_EQ(code._createTime, "2024-01-01 00:00:00");
}

TEST_F(VerifyCodeDataTest, GetVerifyCode_NotFound) {
    auto codeOpt = _verifyCodeData->getVerifyCodeFromCache("non_existent_code");
    EXPECT_FALSE(codeOpt.has_value());
}

TEST_F(VerifyCodeDataTest, DeleteVerifyCode_Success) {
    userService::VerifyCodeInfo verifyCodeInfo;
    verifyCodeInfo._codeId = "test_code_001";
    verifyCodeInfo._verifyCode = "123456";
    verifyCodeInfo._email = "test_email@test.com";
    verifyCodeInfo._createTime = "2024-01-01 00:00:00";

    _verifyCodeData->saveVerifyCodeToCache(verifyCodeInfo);

    bool deleteResult = _verifyCodeData->deleteVerifyCodeFromCache("test_code_001");
    EXPECT_TRUE(deleteResult);

    auto codeOpt = _verifyCodeData->getVerifyCodeFromCache("test_code_001");
    EXPECT_FALSE(codeOpt.has_value());
}

TEST_F(VerifyCodeDataTest, DeleteVerifyCode_NotFound) {
    bool deleteResult = _verifyCodeData->deleteVerifyCodeFromCache("non_existent_code");
    INF("Delete result: {}", deleteResult);
    EXPECT_FALSE(deleteResult);
}

TEST_F(VerifyCodeDataTest, VerifyCodeFlow) {
    userService::VerifyCodeInfo verifyCodeInfo;
    verifyCodeInfo._codeId = "test_code_001";
    verifyCodeInfo._verifyCode = "123456";
    verifyCodeInfo._email = "test_email@test.com";
    verifyCodeInfo._createTime = "2024-01-01 00:00:00";

    bool saveResult = _verifyCodeData->saveVerifyCodeToCache(verifyCodeInfo);
    EXPECT_TRUE(saveResult);

    auto code1 = _verifyCodeData->getVerifyCodeFromCache("test_code_001");
    ASSERT_TRUE(code1.has_value());
    EXPECT_EQ(code1.value()._verifyCode, "123456");

    bool deleteResult = _verifyCodeData->deleteVerifyCodeFromCache("test_code_001");
    EXPECT_TRUE(deleteResult);

    auto code3 = _verifyCodeData->getVerifyCodeFromCache("test_code_001");
    EXPECT_FALSE(code3.has_value());
}

TEST_F(VerifyCodeDataTest, MultipleCodesForSameEmail) {
    userService::VerifyCodeInfo verifyCodeInfo1;
    verifyCodeInfo1._codeId = "test_code_001";
    verifyCodeInfo1._verifyCode = "111111";
    verifyCodeInfo1._email = "test_email@test.com";
    verifyCodeInfo1._createTime = "2024-01-01 00:00:00";

    userService::VerifyCodeInfo verifyCodeInfo2;
    verifyCodeInfo2._codeId = "test_code_002";
    verifyCodeInfo2._verifyCode = "222222";
    verifyCodeInfo2._email = "test_email@test.com";
    verifyCodeInfo2._createTime = "2024-01-01 00:01:00";

    _verifyCodeData->saveVerifyCodeToCache(verifyCodeInfo1);
    _verifyCodeData->saveVerifyCodeToCache(verifyCodeInfo2);

    auto code1 = _verifyCodeData->getVerifyCodeFromCache("test_code_001");
    auto code2 = _verifyCodeData->getVerifyCodeFromCache("test_code_002");

    ASSERT_TRUE(code1.has_value());
    ASSERT_TRUE(code2.has_value());
    EXPECT_EQ(code1.value()._verifyCode, "111111");
    EXPECT_EQ(code2.value()._verifyCode, "222222");
    EXPECT_EQ(code1.value()._email, code2.value()._email);
    EXPECT_NE(code1.value()._codeId, code2.value()._codeId);
}

} // namespace

int main(int argc, char** argv) {
     // 初始化日志库
    bitelog::bitelog_init();
    // 初始化gtest测试框架
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}