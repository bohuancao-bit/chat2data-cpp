#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <bite_scaffold/log.h>
#include "databaseSchema.h"

using namespace databaseService;

class DatabaseSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

static std::string getTempDbPath() {
    return "/tmp/test_db_driver_" + std::to_string(getpid()) + ".db";
}

static bool createTempDbFile(const std::string& path) {
    std::ofstream file(path);
    return file.is_open();
}

static bool removeTempDbFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

TEST_F(DatabaseSchemaTest, T001_ValidConfig_AllFieldsCorrect) {
    // MySQLConfig所有必填字段正确时配置有效
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "testdb";
    EXPECT_TRUE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T003_InvalidConfig_HostEmpty) {
    // MySQLConfig的host为空时配置无效
    MySQLConfig config;
    config.host = "";
    config.port = 3306;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "testdb";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T004_InvalidConfig_PortZero) {
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "testdb";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T005_InvalidConfig_PortNegative) {
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = -1;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "testdb";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T006_InvalidConfig_PortExceedsMax) {
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 65536;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "testdb";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T007_InvalidConfig_UsernameEmpty) {
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.username = "";
    config.password = "CHANGE_ME";
    config.database = "testdb";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T008_InvalidConfig_DatabaseEmpty) {
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T010_GetType_MySQLConfig) {
    MySQLConfig config;
    EXPECT_EQ(config.getType(), DBType::MYSQL);
}

TEST_F(DatabaseSchemaTest, T011_ValidConfig_SQLiteDbPathExists) {
    std::string dbPath = getTempDbPath();
    ASSERT_TRUE(createTempDbFile(dbPath));

    SQLiteConfig config;
    config.dbPath = dbPath;
    EXPECT_TRUE(config.validConfig());

    removeTempDbFile(dbPath);
}

TEST_F(DatabaseSchemaTest, T012_InvalidConfig_SQLiteDbPathEmpty) {
    SQLiteConfig config;
    config.dbPath = "";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T013_InvalidConfig_SQLiteDbPathNotDbSuffix) {
    SQLiteConfig config;
    config.dbPath = "/path/test.sqlite";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T014_InvalidConfig_SQLiteDbPathNotExists) {
    SQLiteConfig config;
    config.dbPath = "/path/nonexistent.db";
    EXPECT_FALSE(config.validConfig());
}

TEST_F(DatabaseSchemaTest, T015_GetType_SQLiteConfig) {
    SQLiteConfig config;
    EXPECT_EQ(config.getType(), DBType::SQLITE);
}

TEST_F(DatabaseSchemaTest, T016_Constructor_EmptyParam) {
    PreparedParam param;
    EXPECT_EQ(param.getType(), ParamType::Null);
}

TEST_F(DatabaseSchemaTest, T017_Constructor_Nullptr) {
    PreparedParam param(nullptr);
    EXPECT_EQ(param.getType(), ParamType::Null);
}

TEST_F(DatabaseSchemaTest, T018_Constructor_IntValue) {
    PreparedParam param(100LL);
    EXPECT_EQ(param.getType(), ParamType::Int);
    EXPECT_EQ(param.getIntValue(), 100);
}

TEST_F(DatabaseSchemaTest, T019_Constructor_DoubleValue) {
    // 通过浮点值构造PreparedParam，类型为ParamType::Double
    PreparedParam param(3.14);
    EXPECT_EQ(param.getType(), ParamType::Double);
    EXPECT_DOUBLE_EQ(param.getDoubleValue(), 3.14);
}

TEST_F(DatabaseSchemaTest, T020_Constructor_StringValue) {
    // 通过字符串构造PreparedParam，类型为ParamType::String
    PreparedParam param(std::string("hello"));
    EXPECT_EQ(param.getType(), ParamType::String);
    EXPECT_EQ(param.getStringValue(), "hello");
}

TEST_F(DatabaseSchemaTest, T021_Constructor_BoolValue_True) {
    // 通过true构造PreparedParam，类型为ParamType::Bool
    PreparedParam param(true);
    EXPECT_EQ(param.getType(), ParamType::Bool);
    EXPECT_TRUE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T022_Constructor_BoolValue_False) {
    // 通过false构造PreparedParam，类型为ParamType::Bool
    PreparedParam param(false);
    EXPECT_EQ(param.getType(), ParamType::Bool);
    EXPECT_FALSE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T023_GetType_IntParam) {
    PreparedParam param(100LL);
    EXPECT_EQ(param.getType(), ParamType::Int);
}

TEST_F(DatabaseSchemaTest, T024_GetType_StringParam) {
    PreparedParam param(std::string("test"));
    EXPECT_EQ(param.getType(), ParamType::String);
}

TEST_F(DatabaseSchemaTest, T025_GetType_NullParam) {
    PreparedParam param(nullptr);
    EXPECT_EQ(param.getType(), ParamType::Null);
}

TEST_F(DatabaseSchemaTest, T026_IsNull_True) {
    // 空构造的PreparedParam的isNull返回true
    PreparedParam param;
    EXPECT_TRUE(param.isNull());
}

TEST_F(DatabaseSchemaTest, T027_IsNull_IntZero) {
    PreparedParam param(0LL);
    EXPECT_FALSE(param.isNull());
}

TEST_F(DatabaseSchemaTest, T028_IsNull_EmptyString) {
    // 空字符串的PreparedParam的isNull返回true
    PreparedParam param(std::string(""));
    EXPECT_TRUE(param.isNull());
}

TEST_F(DatabaseSchemaTest, T029_GetIntValue_IntType) {
    PreparedParam param(42LL);
    EXPECT_EQ(param.getIntValue(), 42);
}

TEST_F(DatabaseSchemaTest, T030_GetIntValue_DoubleType) {
    PreparedParam param(3.14);
    EXPECT_EQ(param.getIntValue(), 3);
}

TEST_F(DatabaseSchemaTest, T031_GetIntValue_ValidString) {
    PreparedParam param(std::string("123"));
    EXPECT_EQ(param.getIntValue(), 123);
}

TEST_F(DatabaseSchemaTest, T032_GetIntValue_InvalidString) {
    PreparedParam param(std::string("abc"));
    EXPECT_EQ(param.getIntValue(), 0);
}

TEST_F(DatabaseSchemaTest, T033_GetIntValue_BoolTrue) {
    PreparedParam param(true);
    EXPECT_EQ(param.getIntValue(), 1);
}

TEST_F(DatabaseSchemaTest, T034_GetIntValue_BoolFalse) {
    PreparedParam param(false);
    EXPECT_EQ(param.getIntValue(), 0);
}

TEST_F(DatabaseSchemaTest, T035_GetIntValue_NullType) {
    PreparedParam param(nullptr);
    EXPECT_EQ(param.getIntValue(), 0);
}

TEST_F(DatabaseSchemaTest, T036_GetDoubleValue_DoubleType) {
    PreparedParam param(3.14);
    EXPECT_DOUBLE_EQ(param.getDoubleValue(), 3.14);
}

TEST_F(DatabaseSchemaTest, T037_GetDoubleValue_IntType) {
    PreparedParam param(100LL);
    EXPECT_DOUBLE_EQ(param.getDoubleValue(), 100.0);
}

TEST_F(DatabaseSchemaTest, T038_GetDoubleValue_ValidString) {
    PreparedParam param(std::string("2.718"));
    EXPECT_DOUBLE_EQ(param.getDoubleValue(), 2.718);
}

TEST_F(DatabaseSchemaTest, T039_GetDoubleValue_InvalidString) {
    PreparedParam param(std::string("abc"));
    EXPECT_DOUBLE_EQ(param.getDoubleValue(), 0.0);
}

TEST_F(DatabaseSchemaTest, T040_GetDoubleValue_BoolTrue) {
    PreparedParam param(true);
    EXPECT_DOUBLE_EQ(param.getDoubleValue(), 1.0);
}

TEST_F(DatabaseSchemaTest, T041_GetStringValue_StringType) {
    PreparedParam param(std::string("hello"));
    EXPECT_EQ(param.getStringValue(), "hello");
}

TEST_F(DatabaseSchemaTest, T042_GetStringValue_IntType) {
    PreparedParam param(123LL);
    EXPECT_EQ(param.getStringValue(), "123");
}

TEST_F(DatabaseSchemaTest, T043_GetStringValue_DoubleType) {
    PreparedParam param(3.14);
    EXPECT_EQ(param.getStringValue(), "3.14");
}

TEST_F(DatabaseSchemaTest, T044_GetStringValue_BoolTrue) {
    // Bool类型true获取字符串值返回"1"
    PreparedParam param(true);
    EXPECT_EQ(param.getStringValue(), "1");
}

TEST_F(DatabaseSchemaTest, T045_GetStringValue_NullType) {
    PreparedParam param(nullptr);
    EXPECT_EQ(param.getStringValue(), "");
}

TEST_F(DatabaseSchemaTest, T046_GetBoolValue_BoolTrue) {
    PreparedParam param(true);
    EXPECT_TRUE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T047_GetBoolValue_BoolFalse) {
    PreparedParam param(false);
    EXPECT_FALSE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T048_GetBoolValue_IntOne) {
    PreparedParam param(1LL);
    EXPECT_TRUE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T049_GetBoolValue_IntZero) {
    PreparedParam param(0LL);
    EXPECT_FALSE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T050_GetBoolValue_StringTRUE) {
    PreparedParam param(std::string("TRUE"));
    EXPECT_TRUE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T051_GetBoolValue_StringYES) {
    PreparedParam param(std::string("YES"));
    EXPECT_TRUE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T052_GetBoolValue_StringFalse) {
    PreparedParam param(std::string("false"));
    EXPECT_FALSE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T053_GetBoolValue_StringAbc) {
    PreparedParam param(std::string("abc"));
    EXPECT_FALSE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T054_GetBoolValue_NullType) {
    PreparedParam param(nullptr);
    EXPECT_FALSE(param.getBoolValue());
}

TEST_F(DatabaseSchemaTest, T055_Success_Default) {
    QueryResult result;
    EXPECT_TRUE(result.success());
}

TEST_F(DatabaseSchemaTest, T056_Success_False) {
    // 设置success为false
    QueryResult result;
    result._success = false;
    EXPECT_FALSE(result.success());
}

TEST_F(DatabaseSchemaTest, T057_ErrorMsg_Empty) {
    QueryResult result;
    EXPECT_EQ(result.errorMsg(), "");
}

TEST_F(DatabaseSchemaTest, T058_ErrorMsg_WithError) {
    // 设置errorMsg
    QueryResult result;
    result._errorMsg = "test error";
    EXPECT_EQ(result.errorMsg(), "test error");
}

TEST_F(DatabaseSchemaTest, T059_ColumnCount_Zero) {
    QueryResult result;
    EXPECT_EQ(result.columnCount(), 0);
}

TEST_F(DatabaseSchemaTest, T060_ColumnCount_WithColumns) {
    // 设置columnCount
    QueryResult result;
    result._columns = {"a", "b", "c"};
    EXPECT_EQ(result.columnCount(), 3);
}

TEST_F(DatabaseSchemaTest, T061_RowCount_Zero) {
    // 默认构造QueryResult的rowCount为0
    QueryResult result;
    EXPECT_EQ(result.rowCount(), 0);
}

TEST_F(DatabaseSchemaTest, T062_RowCount_WithRows) {
    // 设置rowCount
    QueryResult result;
    result._rows = {{"1", "2"}, {"3", "4"}, {"5", "6"}};
    EXPECT_EQ(result.rowCount(), 3);
}

TEST_F(DatabaseSchemaTest, T063_GetRow_ValidIndex) {
    // 获取有效索引的行
    QueryResult result;
    result._rows = {{"1", "2"}, {"3", "4"}};
    auto row = result.getRow(1);
    EXPECT_EQ(row.size(), 2);
    EXPECT_EQ(row[0], "3");
    EXPECT_EQ(row[1], "4");
}

TEST_F(DatabaseSchemaTest, T064_GetRow_NegativeIndex) {
    QueryResult result;
    result._rows = {{"1", "2"}};
    auto row = result.getRow(-1);
    EXPECT_TRUE(row.empty());
}

TEST_F(DatabaseSchemaTest, T065_GetRow_OutOfRange) {
    QueryResult result;
    result._rows = {{"1", "2"}};
    auto row = result.getRow(5);
    EXPECT_TRUE(row.empty());
}

int main(int argc, char** argv) {
    bitelog::bitelog_init();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
