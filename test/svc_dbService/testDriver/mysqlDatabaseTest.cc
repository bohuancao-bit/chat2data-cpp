#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "mysqlDatabase.h"
#include "databaseSchema.h"

using namespace databaseService;

class MySQLDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.host = "127.0.0.1";
        config.port = 3306;
        config.username = "root";
        config.password = "CHANGE_ME";
        config.database = "testdb";
    }

    void TearDown() override {
        if (db && db->ping()) {
            db->disconnect();
        }
    }

    MySQLConfig config;
    std::shared_ptr<MySQLDatabase> db;
};

TEST_F(MySQLDatabaseTest, T176_Connect_Success) {
    // MySQLDatabase使用有效配置能成功连接
    db = std::make_shared<MySQLDatabase>(config);
    bool result = db->connect();
    if (result) {
        EXPECT_TRUE(db->ping());
    } else {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }
}

TEST_F(MySQLDatabaseTest, T177_Connect_InvalidConfig) {
    // MySQLDatabase使用空配置连接失败
    MySQLConfig invalidConfig;
    invalidConfig.host = "";
    db = std::make_shared<MySQLDatabase>(invalidConfig);
    bool result = db->connect();
    EXPECT_FALSE(result);
}

TEST_F(MySQLDatabaseTest, T180_Connect_DuplicateConnect) {
    // MySQLDatabase重复连接不会出错
    db = std::make_shared<MySQLDatabase>(config);
    bool result1 = db->connect();
    if (!result1) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }
    bool result2 = db->connect();
    EXPECT_TRUE(result2);
}

TEST_F(MySQLDatabaseTest, T181_Disconnect_Connected) {
    // MySQLDatabase断开已连接的数据库
    db = std::make_shared<MySQLDatabase>(config);
    db->connect();
    db->disconnect();
    EXPECT_FALSE(db->ping());
}

TEST_F(MySQLDatabaseTest, T182_Disconnect_NotConnected) {
    // MySQLDatabase断开未连接的数据库不会出错
    db = std::make_shared<MySQLDatabase>(config);
    db->disconnect();
    EXPECT_FALSE(db->ping());
}

TEST_F(MySQLDatabaseTest, T183_Ping_Success) {
    // MySQLDatabase的ping返回连接状态
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }
    EXPECT_TRUE(db->ping());
}

TEST_F(MySQLDatabaseTest, T184_Ping_NotConnected) {
    // MySQLDatabase未连接时ping返回false
    db = std::make_shared<MySQLDatabase>(config);
    EXPECT_FALSE(db->ping());
}

TEST_F(MySQLDatabaseTest, T185_Ping_ConnectionLost) {
    // MySQLDatabase能检测连接断开
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }
    db->disconnect();
    EXPECT_FALSE(db->ping());
}

TEST_F(MySQLDatabaseTest, T186_ExecuteQuery_Success) {
    // executeQuery能执行SELECT查询
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    auto result = db->executeQuery("SELECT 1 AS col");
    EXPECT_TRUE(result->success());
    EXPECT_GT(result->rowCount(), 0);
}

TEST_F(MySQLDatabaseTest, T187_ExecuteQuery_EmptyResult) {
    // executeQuery能处理空结果集
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->executeModify("DROP TABLE IF EXISTS test_table");
    db->executeModify("CREATE TABLE test_table(id INT)");
    auto result = db->executeQuery("SELECT * FROM test_table WHERE 1=0");
    EXPECT_TRUE(result->success());
    EXPECT_EQ(result->rowCount(), 0);
    db->executeModify("DROP TABLE IF EXISTS test_table");
}

TEST_F(MySQLDatabaseTest, T188_ExecuteQuery_NotConnected) {
    // executeQuery未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    auto result = db->executeQuery("SELECT 1");
    EXPECT_FALSE(result->success());
}

TEST_F(MySQLDatabaseTest, T189_ExecuteQuery_SyntaxError) {
    // executeQuery能处理语法错误
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    auto result = db->executeQuery("SELEC * FROM users");
    EXPECT_FALSE(result->success());
}

TEST_F(MySQLDatabaseTest, T191_ExecuteModify_InsertSuccess) {
    // executeModify能执行INSERT语句
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->executeModify("DROP TABLE IF EXISTS test_users");
    db->executeModify("CREATE TABLE test_users(id INT, name VARCHAR(100))");
    auto result = db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    EXPECT_TRUE(result->success());
    EXPECT_GE(result->_affectedRows, 1);
    db->executeModify("DROP TABLE IF EXISTS test_users");
}

TEST_F(MySQLDatabaseTest, T192_ExecuteModify_UpdateSuccess) {
    // executeModify能执行UPDATE语句
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->executeModify("DROP TABLE IF EXISTS test_users");
    db->executeModify("CREATE TABLE test_users(id INT, name VARCHAR(100))");
    db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    auto result = db->executeModify("UPDATE test_users SET name='new' WHERE id=1");
    EXPECT_TRUE(result->success());
    db->executeModify("DROP TABLE IF EXISTS test_users");
}

TEST_F(MySQLDatabaseTest, T193_ExecuteModify_DeleteSuccess) {
    // executeModify能执行DELETE语句
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->executeModify("DROP TABLE IF EXISTS test_users");
    db->executeModify("CREATE TABLE test_users(id INT, name VARCHAR(100))");
    db->executeModify("INSERT INTO test_users(id, name) VALUES(1, 'test')");
    auto result = db->executeModify("DELETE FROM test_users WHERE id=1");
    EXPECT_TRUE(result->success());
    db->executeModify("DROP TABLE IF EXISTS test_users");
}

TEST_F(MySQLDatabaseTest, T194_ExecuteModify_NotConnected) {
    // executeModify未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    auto result = db->executeModify("INSERT INTO users VALUES(1)");
    EXPECT_FALSE(result->success());
}

TEST_F(MySQLDatabaseTest, T195_ExecuteModify_SyntaxError) {
    // executeModify能处理语法错误
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    auto result = db->executeModify("INSER INTO users VALUES(1)");
    EXPECT_FALSE(result->success());
}

TEST_F(MySQLDatabaseTest, T197_ExecutePreparedQuery_IntParam) {
    // executePreparedQuery能绑定整数参数
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::vector<PreparedParam> params;
    params.push_back(100LL);

    auto result = db->executePreparedQuery("SELECT ? AS num", params);
    EXPECT_TRUE(result->success());
    EXPECT_GT(result->rowCount(), 0);
}

TEST_F(MySQLDatabaseTest, T198_ExecutePreparedQuery_StringParam) {
    // executePreparedQuery能绑定字符串参数
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::vector<PreparedParam> params;
    params.push_back(std::string("test"));

    auto result = db->executePreparedQuery("SELECT ? AS name", params);
    EXPECT_TRUE(result->success());
    EXPECT_GT(result->rowCount(), 0);
}

TEST_F(MySQLDatabaseTest, T199_ExecutePreparedQuery_MultipleParams) {
    // executePreparedQuery能绑定多个参数
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::vector<PreparedParam> params;
    params.push_back(1LL);
    params.push_back(std::string("test"));

    auto result = db->executePreparedQuery("SELECT ?, ?", params);
    EXPECT_TRUE(result->success());
}

TEST_F(MySQLDatabaseTest, T200_ExecutePreparedQuery_NotConnected) {
    // executePreparedQuery未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    std::vector<PreparedParam> params;
    params.push_back(1LL);

    auto result = db->executePreparedQuery("SELECT ?", params);
    EXPECT_FALSE(result->success());
}

TEST_F(MySQLDatabaseTest, T202_ExecutePreparedModify_InsertSuccess) {
    // executePreparedModify能执行预处理INSERT
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->executeModify("DROP TABLE IF EXISTS test_users");
    db->executeModify("CREATE TABLE test_users(id INT, name VARCHAR(100))");

    std::vector<PreparedParam> params;
    params.push_back(std::string("test"));

    auto result = db->executePreparedModify("INSERT INTO test_users(name) VALUES(?)", params);
    EXPECT_TRUE(result->success());

    db->executeModify("DROP TABLE IF EXISTS test_users");
}

TEST_F(MySQLDatabaseTest, T203_ExecutePreparedModify_NotConnected) {
    // executePreparedModify未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    std::vector<PreparedParam> params;
    params.push_back(std::string("test"));

    auto result = db->executePreparedModify("INSERT INTO users(name) VALUES(?)", params);
    EXPECT_FALSE(result->success());
}

TEST_F(MySQLDatabaseTest, T204_BeginTransaction_Success) {
    // beginTransaction能开启事务
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    bool result = db->beginTransaction();
    EXPECT_TRUE(result);
    db->rollback();
}

TEST_F(MySQLDatabaseTest, T205_BeginTransaction_NotConnected) {
    // beginTransaction未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    bool result = db->beginTransaction();
    EXPECT_FALSE(result);
}

TEST_F(MySQLDatabaseTest, T206_Commit_Success) {
    // commit能提交事务
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->beginTransaction();
    bool result = db->commit();
    EXPECT_TRUE(result);
}

TEST_F(MySQLDatabaseTest, T207_Commit_NotConnected) {
    // commit未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    bool result = db->commit();
    EXPECT_FALSE(result);
}

TEST_F(MySQLDatabaseTest, T208_Rollback_Success) {
    // rollback能回滚事务
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->beginTransaction();
    bool result = db->rollback();
    EXPECT_TRUE(result);
}

TEST_F(MySQLDatabaseTest, T209_Rollback_NotConnected) {
    // rollback未连接时返回失败
    db = std::make_shared<MySQLDatabase>(config);
    bool result = db->rollback();
    EXPECT_FALSE(result);
}

TEST_F(MySQLDatabaseTest, T210_QuoteIdentifier_Normal) {
    // quoteIdentifier能处理普通标识符
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::string result = db->quoteIdentifier("users");
    EXPECT_EQ(result, "`users`");
}

TEST_F(MySQLDatabaseTest, T211_QuoteIdentifier_AlreadyQuoted) {
    // quoteIdentifier能处理已加引号的标识符
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::string result = db->quoteIdentifier("`users`");
    EXPECT_EQ(result, "`users`");
}

TEST_F(MySQLDatabaseTest, T212_QuoteIdentifier_ReservedWord) {
    // quoteIdentifier能处理保留字
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::string result = db->quoteIdentifier("group");
    EXPECT_EQ(result, "`group`");
}

TEST_F(MySQLDatabaseTest, T213_ParamBinder_EmptyParams) {
    // executePreparedQuery能处理空参数列表
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    std::vector<PreparedParam> params;
    auto result = db->executePreparedQuery("SELECT 1", params);
    EXPECT_TRUE(result->success());
}

TEST_F(MySQLDatabaseTest, T214_ParamBinder_AllTypes) {
    // executePreparedModify能处理多种类型参数
    db = std::make_shared<MySQLDatabase>(config);
    bool connectResult = db->connect();
    if (!connectResult) {
        GTEST_SKIP() << "Cannot connect to MySQL server, skipping test";
    }

    db->executeModify("DROP TABLE IF EXISTS test_users");
    db->executeModify("CREATE TABLE test_users(id INT, name VARCHAR(100), balance DOUBLE, active TINYINT)");

    std::vector<PreparedParam> params;
    params.push_back(1LL);
    params.push_back(std::string("str"));
    params.push_back(3.14);
    params.push_back(true);

    auto result = db->executePreparedModify(
        "INSERT INTO test_users(id, name, balance, active) VALUES(?, ?, ?, ?)",
        params);
    EXPECT_TRUE(result->success());

    db->executeModify("DROP TABLE IF EXISTS test_users");
}
