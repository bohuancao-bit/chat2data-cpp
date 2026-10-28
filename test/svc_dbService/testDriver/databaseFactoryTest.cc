#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include <sqlite3.h>
#include "databaseFactory.h"
#include "databaseSchema.h"
#include "sqliteDatabase.h"

using namespace databaseService;

class DatabaseFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(DatabaseFactoryTest, T161_GetInstance_FirstCall) {
    // getInstance首次调用返回非空单例
    auto instance = DatabaseFactory::getInstance();
    EXPECT_NE(instance, nullptr);
}

TEST_F(DatabaseFactoryTest, T162_GetInstance_MultipleCalls) {
    // getInstance多次调用返回相同实例
    auto instance1 = DatabaseFactory::getInstance();
    auto instance2 = DatabaseFactory::getInstance();
    EXPECT_EQ(instance1, instance2);
}

TEST_F(DatabaseFactoryTest, T163_RegisterDatabase_MySQL) {
    // 注册MySQL数据库类型成功
    auto factory = DatabaseFactory::getInstance();
    EXPECT_TRUE(factory->isSupported(DBType::MYSQL));
}

TEST_F(DatabaseFactoryTest, T164_RegisterDatabase_SQLite) {
    // 注册SQLite数据库类型成功
    auto factory = DatabaseFactory::getInstance();
    EXPECT_TRUE(factory->isSupported(DBType::SQLITE));
}

TEST_F(DatabaseFactoryTest, T166_CreateDatabase_MySQL_ValidConfig) {
    // createDatabase能创建MySQL数据库实例
    auto factory = DatabaseFactory::getInstance();

    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.username = "root";
    config.password = "CHANGE_ME";
    config.database = "testdb";

    auto db = factory->createDatabase(&config);
    EXPECT_NE(db, nullptr);
}

TEST_F(DatabaseFactoryTest, T167_CreateDatabase_SQLite_ValidConfig) {
    // createDatabase能创建SQLite数据库实例
    auto factory = DatabaseFactory::getInstance();

    std::string dbPath = "/tmp/test_factory_valid.db";
    std::remove(dbPath.c_str());

    {
        sqlite3* db = nullptr;
        sqlite3_open(dbPath.c_str(), &db);
        if (db) sqlite3_close(db);
    }

    SQLiteConfig config;
    config.dbPath = dbPath;

    auto db = factory->createDatabase(&config);
    EXPECT_NE(db, nullptr);

    std::remove(dbPath.c_str());
}

TEST_F(DatabaseFactoryTest, T168_CreateDatabase_NullptrConfig) {
    // createDatabase传入空配置返回nullptr
    auto factory = DatabaseFactory::getInstance();
    auto db = factory->createDatabase(nullptr);
    EXPECT_EQ(db, nullptr);
}

TEST_F(DatabaseFactoryTest, T169_CreateDatabase_InvalidMySQLConfig) {
    // createDatabase使用无效MySQL配置返回nullptr
    auto factory = DatabaseFactory::getInstance();

    MySQLConfig config;
    config.host = "";
    config.port = 0;
    config.username = "";
    config.password = "";
    config.database = "";

    auto db = factory->createDatabase(&config);
    EXPECT_EQ(db, nullptr);
}

TEST_F(DatabaseFactoryTest, T170_CreateDatabase_InvalidSQLiteConfig) {
    // createDatabase使用无效SQLite配置返回nullptr
    auto factory = DatabaseFactory::getInstance();

    SQLiteConfig config;
    config.dbPath = "";

    auto db = factory->createDatabase(&config);
    EXPECT_EQ(db, nullptr);
}

TEST_F(DatabaseFactoryTest, T172_GetSupportedTypes) {
    // getSupportedTypes返回支持的数据库类型列表
    auto factory = DatabaseFactory::getInstance();
    auto types = factory->getSupportedTypes();
    EXPECT_GE(types.size(), 2);
}

TEST_F(DatabaseFactoryTest, T173_IsSupported_MySQL) {
    // isSupported对MySQL返回true
    auto factory = DatabaseFactory::getInstance();
    EXPECT_TRUE(factory->isSupported(DBType::MYSQL));
}

TEST_F(DatabaseFactoryTest, T174_IsSupported_SQLite) {
    // isSupported对SQLite返回true
    auto factory = DatabaseFactory::getInstance();
    EXPECT_TRUE(factory->isSupported(DBType::SQLITE));
}
