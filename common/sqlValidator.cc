#include "sqlValidator.h"
#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>
#include <bite_scaffold/log.h>

namespace chat2Data {

const std::vector<std::string> SQLValidator::DANGEROUS_KEYWORDS = {
    "DROP DATABASE", 
    "TRUNCATE", 
    "EXEC", 
    "EXECUTE", 
    "SCRIPT", 
    "JAVASCRIPT", 
    "EVAL",
    "UNION ALL SELECT", 
    "1=1", "OR 1=1", 
    "' OR '1'='1'",
    "SLEEP(", 
    "BENCHMARK(", 
    "LOAD_FILE(", 
    "INTO OUTFILE", 
    "INTO DUMPFILE"
};

// R"(--[^\r\n]*)" 匹配单行注释，从"--"开始，直到换行符或字符串结束
const std::regex SQLValidator::COMMENT_SINGLE_LINE(R"(--[^\r\n]*)");
// R"(/\*[\s\S]*?\*/)" 匹配多行注释，从"/*"开始，直到"*/"结束  \s 匹配任意空白字符 \S 匹配任意非空白字符
const std::regex SQLValidator::COMMENT_MULTI_LINE(R"(/\*[\s\S]*?\*/)");
// R"(^([a-zA-Z_][a-zA-Z0-9_\.\-]*|[^\s]+)$)" 匹配表名，表名以字母或下划线开头，后面可以跟字母、数字、下划线、点或短横线，不能以数字开头
const std::regex SQLValidator::PATTERN_TABLE_NAME(R"(^([a-zA-Z_][a-zA-Z0-9_\.\-]*|[^\s]+)$)");

// 去除SQL语句首尾空白字符
// xxxselect * from users;xxx x表示空格
std::string SQLValidator::trim(const std::string& sql) {
    // 1. 从前往后找第一个非空白字符的位置---正向迭代器
    auto start = std::find_if_not(sql.begin(), sql.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });

    // 2. 从后往前找第一个非空白字符的位置，find_if_not返回的是一个反向迭代器
    // 反向迭代器类型中，base()方法返回的是正向迭代器类型
    auto end = std::find_if_not(sql.rbegin(), sql.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();

    if (start >= end) {
        return "";
    }

    // 3. 用[begin, end)之间的非空白字符重新构建sql语句
    return std::string(start, end);
}

// 去除SQL语句中的注释
// 实现原理：使用状态机逐字符扫描SQL，根据当前字符和状态决定如何处理
// 状态包括：普通字符、单行注释、多行注释、单引号字符串、双引号字符串、转义字符
//
// 处理流程：
// 1. 单行注释"--"：遇到"--"后，跳过所有字符直到遇到换行符或字符串结束
// 2. 多行注释"/* */"：遇到"/*"后，跳过所有字符直到遇到"*/"
// 3. 字符串（单引号或双引号）：遇到引号开始，遇到相同引号结束，处理转义字符和转义引号
// 4. 普通字符：直接保留到结果中
std::string SQLValidator::removeComments(const std::string& sql) {
    std::string result;
    result.reserve(sql.length());

    size_t i = 0;
    while (i < sql.length()) {
        // 检测单行注释 "--"
        if (i + 1 < sql.length() && sql[i] == '-' && sql[i + 1] == '-') {
            // 跳过单行注释直到行尾或字符串结束
            while (i < sql.length() && sql[i] != '\n' && sql[i] != '\r') {
                ++i;
            }
            continue;
        }

        // 检测多行注释 "/* */"
        if (i + 1 < sql.length() && sql[i] == '/' && sql[i + 1] == '*') {
            size_t start = i;
            // 跳过/*字符
            i += 2;

            // 跳过*/前的所有字符，包括*/
            while (i + 1 < sql.length()) {
                if (sql[i] == '*' && sql[i + 1] == '/') {
                    i += 2;
                    break;
                }
                ++i;    //跳过注释
            }
            continue;
        }

        // 检测字符串开始：单引号或双引号
        if (sql[i] == '\'' || sql[i] == '"') {
            char quoteChar = sql[i];
            // 跳过字符串的左引号
            result += sql[i];
            ++i;
            // 跳过字符串内容，处理转义字符
            while (i < sql.length()) {
                if (sql[i] == '\\' && i + 1 < sql.length()) {
                    // 转义字符，跳过反斜杠和下一个字符
                    result += sql[i];
                    // 跳过被\转过的字符
                    ++i;
                    if (i < sql.length()) {
                        result += sql[i];
                        ++i;
                    }
                } else if (sql[i] == quoteChar) {
                    // 检查是否是转义的单引号（例如 '' 表示转义的单引号）
                    if (i + 1 < sql.length() && sql[i + 1] == quoteChar) {
                        // 转义的单引号/双引号，保留两个字符
                        result += sql[i];
                        result += sql[i + 1];
                        i += 2;
                    } else {
                        // 字符串结束
                        result += sql[i];
                        ++i;
                        break;
                    }
                } else {
                    result += sql[i];
                    ++i;
                }
            }
            continue;
        }

        result += sql[i];
        ++i;
    }

    return result;
}

// 规范SQL语句
std::string SQLValidator::normalize(const std::string& sql) {
    // 1. 移除注释
    std::string result = removeComments(sql);

    // 2. 移除首尾空白字符
    result = trim(result);

    return result;
}

// 验证SQL语句是否有效
bool SQLValidator::validate(const std::string& sql) {
    // 1. 检查是否包含危险关键词
    if (containsDangerousKeywords(sql)) {
        return false;
    }

    // 2. 检查是否包含多个语句
    if (hasMultipleStatements(sql)) {
        return false;
    }

    // 3. 检测是否为支持的SQL类型
    return getSQLType(sql) != SqlType::UNKNOWN;
}

SqlType SQLValidator::getSQLType(const std::string& sql) {
    // 1. 规范SQL语句
    std::string normalizedSql = normalize(sql);

    // 2. 将sql语句转换为大写 ::toupper: 将字符转换为大写
    std::transform(normalizedSql.begin(), normalizedSql.end(), normalizedSql.begin(), ::toupper);

    // 3. 检测sql语句是否包含支持的SQL类型关键词
    if(normalizedSql.find("SELECT") != std::string::npos) {
        return SqlType::SELECT;
    }

    if(normalizedSql.find("INSERT") != std::string::npos) {
        return SqlType::INSERT;
    }

    if(normalizedSql.find("UPDATE") != std::string::npos) {
        return SqlType::UPDATE;
    }

    if(normalizedSql.find("DELETE") != std::string::npos) {
        return SqlType::DELETE;
    }

    if(normalizedSql.find("DELETE") != std::string::npos) {
        return SqlType::DELETE;
    }

    if(normalizedSql.find("REPLACE") != std::string::npos) {
        return SqlType::REPLACE;
    }

    if(normalizedSql.find("TRUNCATE") != std::string::npos) {
        return SqlType::TRUNCATE;
    }

    if(normalizedSql.find("CREATE") != std::string::npos) {
        return SqlType::CREATE;
    }

    if(normalizedSql.find("DROP") != std::string::npos) {
        return SqlType::DROP;
    }

    if(normalizedSql.find("ALTER") != std::string::npos) {
        return SqlType::ALTER;
    }

    if(normalizedSql.find("SHOW") != std::string::npos) {
        return SqlType::SHOW;
    }

    if(normalizedSql.find("DESC") != std::string::npos) {
        return SqlType::DESC;
    }

    // 4. 其他类型，即不支持的SQL语句
    return SqlType::UNKNOWN;
}

// 判断SQL语句是否为只读操作
bool SQLValidator::isReadOnly(const std::string& sql) {
    // 1. 获取SQL语句的类型
    SqlType type = getSQLType(sql);

    // 2. 检查是否为只读操作
    return type == SqlType::SELECT || type == SqlType::SHOW || type == SqlType::DESC;
}

// 判断SQL语句是否为修改操作
bool SQLValidator::isModifySQL(const std::string& sql) {
    // 1. 获取SQL语句的类型
    SqlType type = getSQLType(sql);

    // 2. 不支持的sql语句是非修改类sql
    if(type == SqlType::UNKNOWN) {
        return false;
    }

    // 3. 检查是否为修改操作
    return !isReadOnly(sql);
}

bool SQLValidator::containsDangerousKeywords(const std::string& sql) {
    // 1. 规范SQL语句
    std::string normalizedSql = normalize(sql);

    // 2. 将sql语句转换为大写 ::toupper: 将字符转换为大写
    std::transform(normalizedSql.begin(), normalizedSql.end(), normalizedSql.begin(), ::toupper);
    
    // 3. 检查sql语句是否包含危险关键词
    for (const auto& keyword : DANGEROUS_KEYWORDS) {
        // 如果sql语句中包含了危险的关键字
        if (normalizedSql.find(keyword) != std::string::npos) {    
            return true;
        }
    }

    return false;
}

// 检测SQL中是否包含多条SQL语句
// 实现原理：遍历SQL字符串，找到第一个分号，判断该分号是否在字符串内部
// 如果分号不在字符串内部，则检查分号后面是否还有非空白字符，如果有则说明有多条SQL语句
//
// 处理流程：
// 1. 遍历SQL字符串中的每个字符
// 2. 如果遇到单引号或双引号，进入字符串模式，跳过字符串内容直到遇到结束引号
// 3. 在字符串模式下遇到转义字符时，跳过转义字符和其后面的字符
// 4. 在字符串模式下遇到结束引号时，检查是否是转义引号（两个连续引号）
// 5. 如果遇到分号且不在字符串模式，检查分号后面是否还有非空白字符
// 6. 如果分号后面还有非空白字符，则返回true表示有多条SQL语句
bool SQLValidator::hasMultipleStatements(const std::string& sql) {
    std::string normalizedSql = normalize(sql);
    if (normalizedSql.empty()) {
        return false;
    }

    // 遍历SQL语句，找到第一个分号，判断分号是否在字符串内部
    size_t i = 0;
    while (i < normalizedSql.length()) {
        if (normalizedSql[i] == '\'' || normalizedSql[i] == '"') {
            // 字符串开始，跳过字符串内容
            char quoteChar = normalizedSql[i];
            ++i;
            while (i < normalizedSql.length()) {
                if (normalizedSql[i] == '\\' && i + 1 < normalizedSql.length()) {
                    // 转义字符，跳过两个字符 // insert into user(name, age) values('\'zhang;san', 20)
                    i += 2;
                } else if (normalizedSql[i] == quoteChar) {
                    // 检查是否是转义的单引号
                    if (i + 1 < normalizedSql.length() && normalizedSql[i + 1] == quoteChar) {
                        // 转义的单引号/双引号，跳过两个字符
                        i += 2; // insert into user(name, age) values('''zhang;san', 20)
                    } else {
                        // 字符串结束  // insert into user(name, age) values('zhang;san', 20)
                        ++i;
                        break;
                    }
                } else {
                    // insert into user(name, age) values('''zhang;san', 20)
                    // 在字符串中的所有内容全部跳过，如果字符串内部包括;，该;也会被跳过
                    ++i;
                }
            }
        } else if (normalizedSql[i] == ';') {
            // 找到分号，分号不在字符串内部
            std::string afterSemicolon = normalizedSql.substr(i + 1);
            afterSemicolon = trim(afterSemicolon);

            if (afterSemicolon.empty()) {
                return false;
            }

            if (afterSemicolon[0] == ';') {
                while (i < normalizedSql.length() && (normalizedSql[i] == ';' || std::isspace(normalizedSql[i]))) {
                    ++i;
                }
                continue;
            }

            return true;
        } else {
            ++i;
        }
    }

    return false;
}

// 从SQL语句中提取表名
// 实现原理：通过关键字匹配找到SQL语句中的表名位置，然后提取表名字符串
// 只提取修改类SQL语句的表名：INSERT、UPDATE、DELETE、TRUNCATE、REPLACE、ALTER、CREATE、DROP
// 不使用正则表达式，支持中文表名和支持单引号包裹的表名
//
// 处理流程：
// 1. 判断是否为修改类SQL语句，如果不是则直接返回空
// 2. 根据不同SQL类型，找到表名开始位置：
//    - INSERT INTO table_name：表名在INTO关键字后面
//    - UPDATE table_name：表名在UPDATE关键字后面
//    - DELETE FROM table_name：表名在FROM关键字后面
//    - TRUNCATE TABLE table_name：表名在TABLE关键字后面
//    - REPLACE INTO table_name：表名在INTO关键字后面
//    - ALTER TABLE table_name：表名在TABLE关键字后面
//    - CREATE TABLE table_name：表名在TABLE关键字后面
//    - DROP TABLE table_name：表名在TABLE关键字后面
// 3. 跳过空白字符，找到表名起始位置
// 4. 判断表名是否被单引号包裹：
//    - 如果是单引号包裹，提取单引号内的内容（处理转义单引号''）
//    - 如果不是单引号包裹，找到空白字符、逗号、分号或左括号为止
// 5. 返回提取的表名列表

std::string SQLValidator::extractTableName(const std::string& sql) {
    std::string table;
    std::string normalizedSql = normalize(sql);
    if (normalizedSql.empty()) {
        return table;
    }

    // 判断是否为修改类SQL语句
    SqlType type = getSQLType(normalizedSql);
    if (type != SqlType::INSERT && type != SqlType::UPDATE && type != SqlType::DELETE &&
        type != SqlType::TRUNCATE && type != SqlType::REPLACE && type != SqlType::ALTER &&
        type != SqlType::CREATE && type != SqlType::DROP) {
        return table;
    }

    // 将sql语句转换为大写
    std::string upperSql;
    upperSql.reserve(normalizedSql.length());
    for (char c : normalizedSql) {
        upperSql += static_cast<char>(std::toupper(c));
    }

    // 查找表名
    size_t tableNameStart = std::string::npos;
    size_t tableNameEnd = std::string::npos;

    if (upperSql.find("INSERT") != std::string::npos) {
        // INSERT INTO table_name
        size_t intoPos = upperSql.find("INTO");
        if (intoPos != std::string::npos) {
            tableNameStart = intoPos + 4;
        }
    } else if (upperSql.find("UPDATE") != std::string::npos) {
        // UPDATE table_name
        size_t updatePos = upperSql.find("UPDATE");
        if (updatePos != std::string::npos) {
            tableNameStart = updatePos + 6;
        }
    } else if (upperSql.find("DELETE") != std::string::npos) {
        // DELETE FROM table_name
        size_t fromPos = upperSql.find("FROM");
        if (fromPos != std::string::npos) {
            tableNameStart = fromPos + 4;
        }
    } else if (upperSql.find("TRUNCATE") != std::string::npos) {
        // TRUNCATE TABLE table_name
        size_t tablePos = upperSql.find("TABLE");
        if (tablePos != std::string::npos) {
            tableNameStart = tablePos + 5;
        }
    } else if (upperSql.find("REPLACE") != std::string::npos) {
        // REPLACE INTO table_name
        size_t intoPos = upperSql.find("INTO");
        if (intoPos != std::string::npos) {
            tableNameStart = intoPos + 4;
        }
    } else if (upperSql.find("ALTER") != std::string::npos) {
        // ALTER TABLE table_name
        size_t tablePos = upperSql.find("TABLE");
        if (tablePos != std::string::npos) {
            tableNameStart = tablePos + 5;
        }
    } else if (upperSql.find("CREATE") != std::string::npos) {
        // CREATE TABLE table_name
        size_t tablePos = upperSql.find("TABLE");
        if (tablePos != std::string::npos) {
            tableNameStart = tablePos + 5;
        }
    } else if (upperSql.find("DROP") != std::string::npos) {
        // DROP TABLE table_name
        size_t tablePos = upperSql.find("TABLE");
        if (tablePos != std::string::npos) {
            tableNameStart = tablePos + 5;
        }
    }

    if (tableNameStart == std::string::npos) {
        return table;
    }

    // 跳过空白字符 INSERT INTO USER(NAME, AGE) VALUES('ZHANG SAN', 20);
    while (tableNameStart < normalizedSql.length() && std::isspace(normalizedSql[tableNameStart])) {
        ++tableNameStart;
    }

    if (tableNameStart >= normalizedSql.length()) {
        return table;
    }

    // 提取表名，表名可能被单引号或反引号包裹
    std::string tableName;
    if (normalizedSql[tableNameStart] == '\'' || normalizedSql[tableNameStart] == '`') {
        // 单引号或反引号包裹的表名
        char quoteChar = normalizedSql[tableNameStart];
        ++tableNameStart;
        size_t endQuotePos = tableNameStart;
        while (endQuotePos < normalizedSql.length()) {
            if (normalizedSql[endQuotePos] == quoteChar) {
                if (quoteChar == '\'' && endQuotePos + 1 < normalizedSql.length() && normalizedSql[endQuotePos + 1] == '\'') {
                    endQuotePos += 2;
                } else {
                    break;
                }
            } else {
                ++endQuotePos;
            }
        }
        tableName = normalizedSql.substr(tableNameStart, endQuotePos - tableNameStart);
        if (quoteChar == '\'') {
            std::string cleanTableName;
            for (size_t i = 0; i < tableName.length(); ++i) {
                if (tableName[i] == '\'' && i + 1 < tableName.length() && tableName[i + 1] == '\'') {
                    cleanTableName += '\'';
                    ++i;
                } else {
                    cleanTableName += tableName[i];
                }
            }
            tableName = cleanTableName;
        }
    } else {
        // 非引号包裹的表名
        size_t i = tableNameStart;
        while (i < normalizedSql.length() &&
               !std::isspace(normalizedSql[i]) &&
               normalizedSql[i] != ',' &&
               normalizedSql[i] != ';' &&
               normalizedSql[i] != '(') {
            ++i;
        }
        tableName = normalizedSql.substr(tableNameStart, i - tableNameStart);
    }

    return tableName;
}

// 判断表名是否合法
// 需要支持中文表名
//
// 验证规则：
// 1. 表名不能为空
// 2. 表名不能包含非法字符：分号、引号、空白、*、\、/
// 3. 表名不能包含连续的特殊字符：..、--
// 4. 表名长度不能超过64
// 5. 表名不能以数字开头（但可以以中文开头）
// 6. 表名只能包含：字母、数字、下划线、点、短横线、UTF-8中文字符
bool SQLValidator::isValidTableName(const std::string& tableName) {
    if (tableName.empty()) {
        return false;
    }

    // 检查是否包含非法字符
    for (unsigned char c : tableName) {
        if (c == ';' || c == '\'' || c == '"' || std::isspace(c) ||
            c == '*' || c == '\\' || c == '/') {
            return false;
        }
    }

    // 检查连续的特殊字符
    for (size_t i = 0; i + 1 < tableName.length(); ++i) {
        if (tableName[i] == '.' && tableName[i + 1] == '.') {
            return false;
        }
        if (tableName[i] == '-' && tableName[i + 1] == '-') {
            return false;
        }
    }

    // 检查长度
    if (tableName.length() > 64) {
        return false;
    }

    // 表名不能以数字开头（但可以以中文开头）
    if (std::isdigit(static_cast<unsigned char>(tableName[0]))) {
        return false;
    }

    // 验证每个字符是否合法（字母、数字、下划线、点、短横线、中文）
    for (size_t i = 0; i < tableName.length(); ) {
        unsigned char c = static_cast<unsigned char>(tableName[i]);

        // ASCII字符：字母、数字、下划线、点、短横线
        if (c <= 127) {
            if (std::isalpha(c) || std::isdigit(c) || c == '_' || c == '.' || c == '-') {
                ++i;
                continue;
            } else {
                return false;
            }
        }

        // 非ASCII字符（中文等）：UTF-8中文字符占3字节，最高位字节以1110开头
        if ((c & 0xF0) == 0xE0 && i + 2 < tableName.length()) {
            unsigned char c2 = static_cast<unsigned char>(tableName[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(tableName[i + 2]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                i += 3;
                continue;
            }
        }

        return false;
    }

    return true;
}

bool SQLValidator::isValidColumnName(const std::string& columnName) {
    return isValidTableName(columnName);
}

} // namespace databaseService
