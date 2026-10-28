#pragma once

#include <sqlite3.h>
#include <memory>
#include <vector>
#include <string>
#include "database.h"
#include "databaseSchema.h"

namespace databaseService {

class SQLiteDatabase : public IDatabase {
public:
    explicit SQLiteDatabase(const SQLiteConfig& config);
    ~SQLiteDatabase() override;
    // 连接数据库
    bool connect() override;
    // 断开数据库连接
    void disconnect() override;
    // 检查数据库连接是否有效
    bool ping() override;
    // 执行查询语句
    std::shared_ptr<QueryResult> executeQuery(const std::string& sql) override;
    // 执行修改语句
    std::shared_ptr<QueryResult> executeModify(const std::string& sql) override;
    // 执行预处理查询语句
    std::shared_ptr<QueryResult> executePreparedQuery(const std::string& sql, const std::vector<PreparedParam>& params) override;
    // 执行预处理修改语句
    std::shared_ptr<QueryResult> executePreparedModify(const std::string& sql, const std::vector<PreparedParam>& params) override;
    // 开始事务
    bool beginTransaction() override;
    // 提交事务
    bool commit() override;
    // 回滚事务
    bool rollback() override;
    // 转移字段名
    std::string quoteIdentifier(const std::string& identifier) override;
    // 获取数据库表列表
    std::vector<std::string> listTables() override;
    // 获取指定表的列信息
    std::vector<ColumnInfo> getTableStruct(const std::string& tableName) override;
    // 将Excel解析服务输出的类型转换为SQLite支持的SQL类型
    std::string convertExcelTypeToSql(const std::string& excelType) override;
    // 获取数据库类型
    DBType getDatabaseType() const override;
private:
    // 初始化数据库连接
    bool initDatabase();
    // 关闭数据库连接
    void closeDatabase();

    // 参数绑定器辅助类
    class ParamBinder;

private:
    SQLiteConfig _config;
    sqlite3* _db = nullptr;
    bool _connected = false;
};

// 参数绑定器辅助类
class SQLiteDatabase::ParamBinder {
public:
    ParamBinder(sqlite3* db, const std::string& sql, const std::vector<PreparedParam>& params);
    ~ParamBinder();

    // 获取参数绑定结果
    sqlite3_stmt* getStmt() const;
    // 执行查询语句，返回sqlite3_step的返回值
    int step();
private:
    sqlite3_stmt* _stmt = nullptr;    // 预编译语句句柄
    bool _executed = false;           // 是否已执行
    int _columnCount = 0;             // 查询结果列数
};

} // namespace databaseService
