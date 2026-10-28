#pragma once

#include <string>
#include <vector>
#include <regex>

namespace chat2Data {

// 定义支持的SQL类型
enum class SqlType {
    SELECT,
    SHOW,
    DESC,
    INSERT,
    UPDATE,
    DELETE,
    REPLACE,
    TRUNCATE,
    CREATE,
    DROP,
    ALTER,
    UNKNOWN
};

class SQLValidator {
public:
    // 去除SQL语句首尾空白字符
    static std::string trim(const std::string& sql);
    // 去除SQL语句中的注释
    static std::string removeComments(const std::string& sql);
    // 规范SQL语句
    static std::string normalize(const std::string& sql);
    // 验证SQL语句是否有效
    static bool validate(const std::string& sql);
    // 获取SQL语句的类型
    static SqlType getSQLType(const std::string& sql);
    // 判断SQL语句是否为只读操作
    static bool isReadOnly(const std::string& sql);
    // 判断SQL语句是否为修改操作
    static bool isModifySQL(const std::string& sql);
    // 检测SQL语句是否包含危险关键词
    static bool containsDangerousKeywords(const std::string& sql);
    // 检测SQL语句是否包含多个语句
    static bool hasMultipleStatements(const std::string& sql);
    // 从SQL语句中提取表名
    static std::string extractTableName(const std::string& sql);
    // 判断表名是否有效
    static bool isValidTableName(const std::string& tableName);
    // 判断列名是否有效
    static bool isValidColumnName(const std::string& columnName);

private:
    static const std::vector<std::string> DANGEROUS_KEYWORDS;
    static const std::regex COMMENT_SINGLE_LINE;
    static const std::regex COMMENT_MULTI_LINE;
    static const std::regex PATTERN_TABLE_NAME;
};

} // namespace databaseService
