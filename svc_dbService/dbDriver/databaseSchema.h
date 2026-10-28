#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace databaseService {

// 支持的数据库类型
enum class DBType {
    MYSQL,
    SQLITE
};

// 数据库配置基类
class DBConfig {
public:
    virtual ~DBConfig() = default;
    // 验证配置是否有效
    virtual bool validConfig() const = 0;
    // 获取数据库类型
    virtual DBType getType() const = 0;
};

// MySQL数据库配置
struct MySQLConfig : public DBConfig {
    std::string host;                     // 数据库主机地址
    int port = 3306;                      // 数据库端口
    std::string username;                 // 数据库用户名
    std::string password;                 // 数据库密码
    std::string database;                 // 数据库名称
    std::string charset = "utf8mb4";      // 数据库字符集
    int timeout = 10;                     // 数据库超时时间
    bool hasSSL = false;                  // 是否使用SSL连接
    std::string sslCaCert;                // SSL证书路径
    std::string sslCert;                  // SSL证书路径
    std::string sslKey;                   // SSL密钥路径
    std::unordered_map<std::string, std::string> options; // 其他连接选项

    // 验证配置是否有效
    bool validConfig() const override;
    // 获取数据库类型
    DBType getType() const override;
};

// SQLite数据库配置
struct SQLiteConfig : public DBConfig {
    // 数据库文件路径
    std::string dbPath;

    // 验证配置是否有效
    bool validConfig() const override;
    // 获取数据库类型
    DBType getType() const override;
};

// 列信息
struct ColumnInfo {
    std::string name;                 // 列名
    std::string type;                 // 列类型
    bool nullable = true;             // 是否可空
    bool primaryKey = false;          // 是否主键
    bool autoIncrement = false;       // 是否自增
    std::string defaultValue;         // 默认值
    int maxLength = 0;                // 最大长度
};

// 表信息
struct TableInfo {
    std::string tableName;                  // 表名
    std::vector<ColumnInfo> columns;        // 列信息
    std::vector<std::string> primaryKeys;   // 主键列名
    std::vector<std::string> indexes;       // 索引列名
    std::string comment;                    // 表注释
};

// 参数类型
enum class ParamType {
    Null,
    Int,
    Double,
    String,
    Bool
};

// 预编译参数包装器
class PreparedParam {
private:
    ParamType _type = ParamType::Null;// 参数类型
    long long _intValue = 0;          // 整数值
    double _doubleValue = 0.0;        // 浮点数值
    std::string _stringValue;         // 字符串值
    bool _boolValue = false;          // 布尔值

public:
    PreparedParam();
    PreparedParam(std::nullptr_t);
    PreparedParam(long long value);
    PreparedParam(double value);
    PreparedParam(const std::string& value);
    PreparedParam(bool value);

    // 获取参数类型
    ParamType getType() const;
    // 是否为空值
    bool isNull() const;
    // 获取整数值
    long long getIntValue() const;
    // 获取浮点数值
    double getDoubleValue() const;
    // 获取字符串值
    std::string getStringValue() const;
    // 获取布尔值
    bool getBoolValue() const;
};

// sql执行结果
class QueryResult {
public:
    bool _success = true;                           // 是否成功
    std::string _errorMsg;                          // 错误信息
    int _affectedRows = 0;                         // 受影响的行数
    std::vector<std::string> _columns;             // 列名
    std::vector<std::string> _columnTypes;          // 列类型
    std::vector<std::vector<std::string>> _rows;    // 行数据

    // sql是否执行成功
    bool success() const;
    const std::string& errorMsg() const;
    // 获取列数
    int columnCount() const;
    // 获取行数
    int rowCount() const;
    // 获取指定行数据
    std::vector<std::string> getRow(int index) const;
    // 转换为JSON字符串
    std::string toJson() const;
};

} // namespace databaseService
