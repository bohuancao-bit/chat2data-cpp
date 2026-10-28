#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "sqliteDatabase.h"
#include "databaseSchema.h"
#include <sys/stat.h>

using namespace databaseService;

class SQLiteDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDbPath = "/tmp/test_sqlite.db";
        
        removeTempDbFile();
        createTempDbFile();
        config.dbPath = tempDbPath;
        db = nullptr;
    }

    void TearDown() override {
        if (db && db->ping()) {
            db->disconnect();
        }
        removeTempDbFile();
    }

    std::string tempDbPath;
    SQLiteConfig config;
    std::shared_ptr<SQLiteDatabase> db;

    bool removeTempDbFile() {
        if (access(tempDbPath.c_str(), F_OK) == 0) {
            return remove(tempDbPath.c_str()) == 0;
        }
        return true;
    }

    bool createTempDbFile() {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(tempDbPath.c_str(), &db);
        if (rc == SQLITE_OK && db != nullptr) {
            sqlite3_close(db);
            return true;
        }
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return false;
    }
};

TEST_F(SQLiteDatabaseTest, T215_Connect_Success) {
    // SQLiteDatabase使用有效配置能成功连接
    db = std::make_shared<SQLiteDatabase>(config);
    bool result = db->connect();
    EXPECT_TRUE(result);
    EXPECT_TRUE(db->ping());
}

TEST_F(SQLiteDatabaseTest, T216_Connect_InvalidPath) {
    // SQLiteDatabase使用无效路径连接失败
    SQLiteConfig invalidConfig;
    invalidConfig.dbPath = "/invalid/path/to/db.sqlite";
    db = std::make_shared<SQLiteDatabase>(invalidConfig);
    bool result = db->connect();
    EXPECT_FALSE(result);
}

TEST_F(SQLiteDatabaseTest, T217_Connect_EmptyPath) {
    // SQLiteDatabase使用空路径连接失败
    SQLiteConfig invalidConfig;
    invalidConfig.dbPath = "";
    db = std::make_shared<SQLiteDatabase>(invalidConfig);
    bool result = db->connect();
    EXPECT_FALSE(result);
}

TEST_F(SQLiteDatabaseTest, T220_Connect_DuplicateConnect) {
    // SQLiteDatabase重复连接不会出错
    db = std::make_shared<SQLiteDatabase>(config);
    bool result1 = db->connect();
    EXPECT_TRUE(result1);
    bool result2 = db->connect();
    EXPECT_TRUE(result2);
}

TEST_F(SQLiteDatabaseTest, T221_Disconnect_Connected) {
    // SQLiteDatabase断开已连接的数据库
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();
    db->disconnect();
    EXPECT_FALSE(db->ping());
}

TEST_F(SQLiteDatabaseTest, T222_Disconnect_NotConnected) {
    // SQLiteDatabase断开未连接的数据库不会出错
    db = std::make_shared<SQLiteDatabase>(config);
    db->disconnect();
    EXPECT_FALSE(db->ping());
}

TEST_F(SQLiteDatabaseTest, T223_Ping_Success) {
    // SQLiteDatabase的ping返回连接状态
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();
    EXPECT_TRUE(db->ping());
}

TEST_F(SQLiteDatabaseTest, T224_Ping_NotConnected) {
    // SQLiteDatabase未连接时ping返回false
    db = std::make_shared<SQLiteDatabase>(config);
    EXPECT_FALSE(db->ping());
}

TEST_F(SQLiteDatabaseTest, T225_ExecuteQuery_Success) {
    // executeQuery能执行SELECT查询
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    auto result = db->executeQuery("SELECT 1 AS col");
    EXPECT_TRUE(result->success());
    EXPECT_GT(result->rowCount(), 0);
}

TEST_F(SQLiteDatabaseTest, T226_ExecuteQuery_EmptyResult) {
    // executeQuery能处理空结果集
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_table(id INTEGER)");
    auto result = db->executeQuery("SELECT * FROM test_table WHERE 1=0");
    EXPECT_TRUE(result->success());
    EXPECT_EQ(result->rowCount(), 0);
}

TEST_F(SQLiteDatabaseTest, T227_ExecuteQuery_NotConnected) {
    // executeQuery未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    auto result = db->executeQuery("SELECT 1");
    EXPECT_FALSE(result->success());
}

TEST_F(SQLiteDatabaseTest, T228_ExecuteQuery_SyntaxError) {
    // executeQuery能处理语法错误
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    auto result = db->executeQuery("SELEC * FROM users");
    EXPECT_FALSE(result->success());
}

TEST_F(SQLiteDatabaseTest, T230_ExecuteModify_InsertSuccess) {
    // executeModify能执行INSERT语句
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT)");
    auto result = db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    EXPECT_TRUE(result->success());
    EXPECT_GE(result->_affectedRows, 1);
}

TEST_F(SQLiteDatabaseTest, T231_ExecuteModify_UpdateSuccess) {
    // executeModify能执行UPDATE语句
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT)");
    db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    auto result = db->executeModify("UPDATE test_users SET name='new' WHERE id=1");
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T232_ExecuteModify_DeleteSuccess) {
    // executeModify能执行DELETE语句
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT)");
    db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    auto result = db->executeModify("DELETE FROM test_users WHERE id=1");
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T233_ExecuteModify_NotConnected) {
    // executeModify未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    auto result = db->executeModify("INSERT INTO users VALUES(1)");
    EXPECT_FALSE(result->success());
}

TEST_F(SQLiteDatabaseTest, T234_ExecuteModify_SyntaxError) {
    // executeModify能处理语法错误
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    auto result = db->executeModify("INSER INTO users VALUES(1)");
    EXPECT_FALSE(result->success());
}

TEST_F(SQLiteDatabaseTest, T236_ExecutePreparedQuery_IntParam) {
    // executePreparedQuery能绑定整数参数
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::vector<PreparedParam> params;
    params.push_back(100LL);

    auto result = db->executePreparedQuery("SELECT ? AS num", params);
    EXPECT_TRUE(result->success());
    EXPECT_GT(result->rowCount(), 0);
}

TEST_F(SQLiteDatabaseTest, T237_ExecutePreparedQuery_StringParam) {
    // executePreparedQuery能绑定字符串参数
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::vector<PreparedParam> params;
    params.push_back(std::string("test"));

    auto result = db->executePreparedQuery("SELECT ? AS name", params);
    EXPECT_TRUE(result->success());
    EXPECT_GT(result->rowCount(), 0);
}

TEST_F(SQLiteDatabaseTest, T238_ExecutePreparedQuery_MultipleParams) {
    // executePreparedQuery能绑定多个参数
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::vector<PreparedParam> params;
    params.push_back(1LL);
    params.push_back(std::string("test"));

    auto result = db->executePreparedQuery("SELECT ?, ?", params);
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T239_ExecutePreparedQuery_NotConnected) {
    // executePreparedQuery未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    std::vector<PreparedParam> params;
    params.push_back(1LL);

    auto result = db->executePreparedQuery("SELECT ?", params);
    EXPECT_FALSE(result->success());
}

TEST_F(SQLiteDatabaseTest, T241_ExecutePreparedModify_InsertSuccess) {
    // executePreparedModify能执行预处理INSERT
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT)");

    std::vector<PreparedParam> params;
    params.push_back(std::string("test"));

    auto result = db->executePreparedModify("INSERT INTO test_users(name) VALUES(?)", params);
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T242_ExecutePreparedModify_NotConnected) {
    // executePreparedModify未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    std::vector<PreparedParam> params;
    params.push_back(std::string("test"));

    auto result = db->executePreparedModify("INSERT INTO users(name) VALUES(?)", params);
    EXPECT_FALSE(result->success());
}

TEST_F(SQLiteDatabaseTest, T243_BeginTransaction_Success) {
    // beginTransaction能开启事务
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    bool result = db->beginTransaction();
    EXPECT_TRUE(result);
    db->rollback();
}

TEST_F(SQLiteDatabaseTest, T244_BeginTransaction_NotConnected) {
    // beginTransaction未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    bool result = db->beginTransaction();
    EXPECT_FALSE(result);
}

TEST_F(SQLiteDatabaseTest, T245_Commit_Success) {
    // commit能提交事务
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->beginTransaction();
    bool result = db->commit();
    EXPECT_TRUE(result);
}

TEST_F(SQLiteDatabaseTest, T246_Commit_NotConnected) {
    // commit未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    bool result = db->commit();
    EXPECT_FALSE(result);
}

TEST_F(SQLiteDatabaseTest, T247_Rollback_Success) {
    // rollback能回滚事务
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->beginTransaction();
    bool result = db->rollback();
    EXPECT_TRUE(result);
}

TEST_F(SQLiteDatabaseTest, T248_Rollback_NotConnected) {
    // rollback未连接时返回失败
    db = std::make_shared<SQLiteDatabase>(config);
    bool result = db->rollback();
    EXPECT_FALSE(result);
}

TEST_F(SQLiteDatabaseTest, T249_Transaction_RollbackData) {
    // 事务回滚能撤销数据修改
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT)");
    db->beginTransaction();
    db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    db->rollback();

    auto result = db->executeQuery("SELECT * FROM test_users");
    EXPECT_EQ(result->rowCount(), 0);
}

TEST_F(SQLiteDatabaseTest, T250_Transaction_CommitData) {
    // 事务提交能保存数据修改
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT)");
    db->beginTransaction();
    db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    db->commit();

    auto result = db->executeQuery("SELECT * FROM test_users");
    EXPECT_EQ(result->rowCount(), 1);
}

TEST_F(SQLiteDatabaseTest, T251_QuoteIdentifier_Normal) {
    // quoteIdentifier能处理普通标识符
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::string result = db->quoteIdentifier("users");
    EXPECT_EQ(result, "\"users\"");
}

TEST_F(SQLiteDatabaseTest, T252_QuoteIdentifier_AlreadyQuoted) {
    // quoteIdentifier能处理已加引号的标识符
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::string result = db->quoteIdentifier("\"users\"");
    EXPECT_EQ(result, "\"users\"");
}

TEST_F(SQLiteDatabaseTest, T253_QuoteIdentifier_ReservedWord) {
    // quoteIdentifier能处理保留字
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::string result = db->quoteIdentifier("group");
    EXPECT_EQ(result, "\"group\"");
}

TEST_F(SQLiteDatabaseTest, T254_ParamBinder_EmptyParams) {
    // executePreparedQuery能处理空参数列表
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::vector<PreparedParam> params;
    auto result = db->executePreparedQuery("SELECT 1", params);
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T255_ParamBinder_AllTypes) {
    // executePreparedModify能处理多种类型参数
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE test_users(id INTEGER, name TEXT, balance REAL, active INTEGER)");

    std::vector<PreparedParam> params;
    params.push_back(nullptr);
    params.push_back(1LL);
    params.push_back(3.14);
    params.push_back(std::string("str"));
    params.push_back(true);

    auto result = db->executePreparedModify(
        "INSERT INTO test_users(id, balance, name, active) VALUES(?, ?, ?, ?)",
        params);
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T256_CreateTable_Success) {
    // executeModify能创建表
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    auto result = db->executeModify("CREATE TABLE new_table(id INTEGER PRIMARY KEY, name TEXT)");
    EXPECT_TRUE(result->success());

    auto checkResult = db->executeQuery("SELECT name FROM sqlite_master WHERE type='table' AND name='new_table'");
    EXPECT_EQ(checkResult->rowCount(), 1);
}

TEST_F(SQLiteDatabaseTest, T257_DropTable_Success) {
    // executeModify能删除表
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE drop_test(id INTEGER)");
    auto result = db->executeModify("DROP TABLE drop_test");
    EXPECT_TRUE(result->success());

    auto checkResult = db->executeQuery("SELECT name FROM sqlite_master WHERE type='table' AND name='drop_test'");
    EXPECT_EQ(checkResult->rowCount(), 0);
}

TEST_F(SQLiteDatabaseTest, T258_NullParam_InQuery) {
    // executePreparedQuery能处理NULL参数
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    std::vector<PreparedParam> params;
    params.push_back(nullptr);

    auto result = db->executePreparedQuery("SELECT NULL AS val", params);
    EXPECT_TRUE(result->success());
}

TEST_F(SQLiteDatabaseTest, T259_BeginTransaction_Explicit) {
    // beginTransaction能显式开启事务
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    bool result = db->beginTransaction();
    EXPECT_TRUE(result);
    EXPECT_TRUE(db->commit());
}

TEST_F(SQLiteDatabaseTest, T260_AutoCommit_Default) {
    // SQLite默认开启自动提交
    db = std::make_shared<SQLiteDatabase>(config);
    db->connect();

    db->executeModify("CREATE TABLE auto_commit_test(id INTEGER)");
    auto result = db->executeModify("INSERT INTO auto_commit_test(id) VALUES(1)");
    EXPECT_TRUE(result->success());

    auto queryResult = db->executeQuery("SELECT * FROM auto_commit_test");
    EXPECT_EQ(queryResult->rowCount(), 1);
}
