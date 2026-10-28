#include <sstream>
#include <spdlog/spdlog.h>
#include <bite_scaffold/log.h>
#include "../../common/errorHandler.h"
#include "../../common/utils.h"
#include "mysqlDatabase.h"

namespace databaseService {

MySQLDatabase::MySQLDatabase(const MySQLConfig& config)
    : _config(config) {
}

MySQLDatabase::~MySQLDatabase() {
    disconnect();
}

// 初始化MySQL
bool MySQLDatabase::initMysql() {
    // 1. 检测是否已初始化MySQL
    if (_mysql != nullptr) {
       return true;
    }

    // 2. 初始化MySQL
    _mysql = mysql_init(nullptr);
    if (_mysql == nullptr) {
        ERR("MySQL init failed: {}", mysql_error(_mysql));
        return false;
    }

    return true;
}

void MySQLDatabase::closeMysql() {
     // 1. 检测是否初始化MySQL
    if (_mysql == nullptr) {
       return;
    }

    // 2. 关闭MySQL连接
    mysql_close(_mysql);
    _mysql = nullptr;
    _connected = false;
}

bool MySQLDatabase::connect() {
    // 1. 检测是否已连接MySQL
    if (_connected) {
        return true;
    }

    // 2. 初始化MySQL
    if (!initMysql()) {
        return false;
    }

    // 3. 设置连接超时时间
    unsigned int timeout = static_cast<unsigned int>(_config.timeout);
    // 设置连接超时时间
    mysql_options(_mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    // 设置字符集
    mysql_options(_mysql, MYSQL_SET_CHARSET_NAME, _config.charset.c_str());
    // 检测是否开启SSL
    if (_config.hasSSL) {
        mysql_options(_mysql, MYSQL_OPT_SSL_CA, _config.sslCaCert.c_str());
        mysql_options(_mysql, MYSQL_OPT_SSL_CERT, _config.sslCert.c_str());
        mysql_options(_mysql, MYSQL_OPT_SSL_KEY, _config.sslKey.c_str());
    }

    // 4. 连接MySQL
    MYSQL* conn = mysql_real_connect(
        _mysql,
        _config.host.c_str(),
        _config.username.c_str(),
        _config.password.c_str(),
        _config.database.c_str(),
        _config.port,
        nullptr,
        0);
    if (conn == nullptr) {
        ERR("MySQL connect failed: {}", mysql_error(_mysql));
        closeMysql();
        return false;
    }

    // 5. MySQL连接成功，设置连接状态
    _connected = true;
    INF("MySQL connected successfully to {}:{}/{}",_config.host, _config.port, _config.database);
    return true;
}

void MySQLDatabase::disconnect() {
    // 1. 检测是否已连接MySQL
    if (!_connected) {
        return;
    }

    // 如果建立了数据库连接则断开
    closeMysql();
    INF("MySQL disconnected");
}

// 检查MySQL连接是否有效
bool MySQLDatabase::ping() {
    // 1. 检测是否已连接MySQL
    if (!_connected || _mysql == nullptr) {
        return false;
    }

    // 2. 检查MySQL连接是否有效
    if (mysql_ping(_mysql) != 0) {
        ERR("MySQL ping failed: {}", mysql_error(_mysql));
        _connected = false;
        return false;
    }
    return true;
}

// 将MySQL字段类型转换为字符串
std::string MySQLDatabase::convertColumnType(enum_field_types type, unsigned int charsetnr) {
    switch (type) {
        case MYSQL_TYPE_NULL:
            return "NULL";
        case MYSQL_TYPE_LONG:
            return "INT";
        case MYSQL_TYPE_LONGLONG:
            return "BIGINT";
        case MYSQL_TYPE_DOUBLE:
            return "DOUBLE";
        case MYSQL_TYPE_STRING:
            return "VARCHAR";
        case MYSQL_TYPE_VAR_STRING:
            return "VARCHAR";
        case MYSQL_TYPE_BLOB:
            if (charsetnr == 63) {
                return "BLOB";
            } else {
                return "TEXT";
            }
        case MYSQL_TYPE_DATE:
            return "DATE";
        case MYSQL_TYPE_DATETIME:
            return "DATETIME";
        case MYSQL_TYPE_TIMESTAMP:
            return "TIMESTAMP";
        default:
            return "UNKNOWN";
    }
}

// 执行查询
std::shared_ptr<QueryResult> MySQLDatabase::executeQuery(const std::string& sql) {
    // 1. 创建保存sql执行结果的对象
    auto result = std::make_shared<QueryResult>();

    // 2. 检测是否已连接MySQL
    if (!_connected || _mysql == nullptr) {
        result->_success = false;
        result->_errorMsg = "Not connected to MySQL";
        return result;
    }

    // 3. 执行查询操作
    int queryResult = mysql_real_query(_mysql, sql.c_str(), sql.length() + 1);
    if (queryResult != 0) {
        result->_success = false;
        result->_errorMsg = mysql_error(_mysql);
        ERR("MySQL query failed: {}", result->_errorMsg);
        return result;
    }

    // 4. 获取查询结果
    MYSQL_RES* res = mysql_store_result(_mysql);
    if (res == nullptr) {
        result->_success = false;
        result->_errorMsg = mysql_error(_mysql);
        return result;
    }

    // 5. 获取从列数以及字段列表信息
    unsigned int numFields = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    // 6. 保存结果
    // 保存列名和列类型
    for (unsigned int i = 0; i < numFields; ++i) {
        result->_columns.push_back(fields[i].name);
        INF("Column name: {}", fields[i].name);
        result->_columnTypes.push_back(convertColumnType(fields[i].type, fields[i].charsetnr));
    }

    // 逐行获取查询结果并保存
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        std::vector<std::string> rowData;
        for (unsigned int i = 0; i < numFields; ++i) {
            INF("Column value: {}  Column type: {}", row[i], convertColumnType(fields[i].type, fields[i].charsetnr));
            if (row[i] == nullptr) {
                rowData.push_back("");
            } else if (fields[i].type == MYSQL_TYPE_BLOB) {
                // BLOB类型数据直接作为文本字符串时，会导致protobuf序列化失败
                // 因为proto3语法规定：protobuf对文本字符串要求必须是有效的UTF-8格式
                // 而BLOB类型中可能包含非法的UTF-8字节
                // 因此，需要将BLOB类型对应的字符串进行base64编码
                unsigned long length = mysql_fetch_lengths(res)[i];
                if (fields[i].charsetnr == 63) {
                    std::vector<char> blobVec(row[i], row[i] + length);
                    rowData.push_back(chat2Data::Utils::base64Encode(blobVec));
                } else {
                    rowData.push_back(std::string(row[i], length));
                }
            } else {
                rowData.push_back(row[i]);
            }
        }
        result->_rows.push_back(rowData);
    }

    // 7. 释放查询结果集
    mysql_free_result(res);

    // 8. 设置sql执行成功并返回
    result->_success = true;
    return result;
}

// 执行修改
std::shared_ptr<QueryResult> MySQLDatabase::executeModify(const std::string& sql) {
    // 1. 创建保存sql执行结果的对象
    auto result = std::make_shared<QueryResult>();

    // 2. 检测是否已连接MySQL
    if (!_connected || _mysql == nullptr) {
        result->_success = false;
        result->_errorMsg = "Not connected to MySQL";
        return result;
    }

    // 3. 执行修改操作
    int queryResult = mysql_real_query(_mysql, sql.c_str(), sql.length() + 1);
    if (queryResult != 0) {
        result->_success = false;
        result->_errorMsg = mysql_error(_mysql);
        ERR("MySQL modify failed: {}", result->_errorMsg);
        return result;
    }

    // 4. 获取受影响的行数
    result->_affectedRows = static_cast<int>(mysql_affected_rows(_mysql));
    result->_success = true;
    return result;
}

// 执行预编译查询
std::shared_ptr<QueryResult> MySQLDatabase::executePreparedQuery(const std::string& sql, const std::vector<PreparedParam>& params) {
    // 1. 创建保存sql执行结果的对象
    auto result = std::make_shared<QueryResult>();

    // 2. 检测是否已连接MySQL
    if (!_connected || _mysql == nullptr) {
        result->_success = false;
        result->_errorMsg = "Not connected to MySQL";
        return result;
    }

    // 3. 初始化预编译语句
    MYSQL_STMT* stmt = mysql_stmt_init(_mysql);
    if (stmt == nullptr) {
        result->_success = false;
        result->_errorMsg = mysql_error(_mysql);
        return result;
    }

    // 4. 准备预编译语句
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length() + 1) != 0) {
        result->_success = false;
        result->_errorMsg = mysql_stmt_error(stmt);
        ERR("MySQL stmt prepare failed: {}", result->_errorMsg);
        mysql_stmt_close(stmt);
        return result;
    }

    // 5. 绑定参数
    ParamBinder paramBinder(sql, params);
    if (mysql_stmt_bind_param(stmt, paramBinder.getBinds()) != 0) {
        result->_success = false;
        result->_errorMsg = mysql_stmt_error(stmt);
        ERR("MySQL stmt bind param failed: {}", result->_errorMsg);
        mysql_stmt_close(stmt);
        return result;
    }

    // 6. 执行预编译语句
    if (mysql_stmt_execute(stmt) != 0) {
        result->_success = false;
        result->_errorMsg = mysql_stmt_error(stmt);
        ERR("MySQL stmt execute failed: {}", result->_errorMsg);
        mysql_stmt_close(stmt);
        return result;
    }

    // 7. 获取查询结果
    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
    if (meta != nullptr) {
        ResultBinder resultBinder(stmt, meta, result);
        resultBinder.bind();
        mysql_free_result(meta);
    }

    // 8. 关闭预编译语句
    mysql_stmt_close(stmt);

    // 9. 设置sql执行成功并返回
    result->_success = true;
    return result;
}

// 执行预编译修改
std::shared_ptr<QueryResult> MySQLDatabase::executePreparedModify(const std::string& sql, const std::vector<PreparedParam>& params) {
    // 1. 创建保存sql执行结果的对象
    auto result = std::make_shared<QueryResult>();

    // 2. 检测是否已连接MySQL
    if (!_connected || _mysql == nullptr) {
        result->_success = false;
        result->_errorMsg = "Not connected to MySQL";
        return result;
    }

    // 3. 初始化预编译语句
    MYSQL_STMT* stmt = mysql_stmt_init(_mysql);
    if (stmt == nullptr) {
        result->_success = false;
        result->_errorMsg = mysql_error(_mysql);
        return result;
    }

    // 4. 准备预编译语句
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length() + 1) != 0) {
        result->_success = false;
        result->_errorMsg = mysql_stmt_error(stmt);
        ERR("MySQL stmt prepare failed: {}", result->_errorMsg);
        mysql_stmt_close(stmt);
        return result;
    }

    // 5. 绑定参数
    ParamBinder paramBinder(sql, params);
    if (mysql_stmt_bind_param(stmt, paramBinder.getBinds()) != 0) {
        result->_success = false;
        result->_errorMsg = mysql_stmt_error(stmt);
        ERR("MySQL stmt bind param failed: {}", result->_errorMsg);
        mysql_stmt_close(stmt);
        return result;
    }

    // 6. 执行预编译语句
    if (mysql_stmt_execute(stmt) != 0) {
        result->_success = false;
        result->_errorMsg = mysql_stmt_error(stmt);
        ERR("MySQL stmt execute failed: {}", result->_errorMsg);
        mysql_stmt_close(stmt);
        return result;
    }

    // 7. 获取受影响的行数
    result->_affectedRows = static_cast<int>(mysql_stmt_affected_rows(stmt));
    // 8. 关闭预编译语句
    mysql_stmt_close(stmt);
    // 9. 设置sql执行成功并返回
    result->_success = true;
    return result;
}

// 开启事务
bool MySQLDatabase::beginTransaction() {
    // 1. 检查数据库是否已连接
    if (!_connected || _mysql == nullptr) {
        return false;
    }

    // 2. 开启事务
    if (mysql_autocommit(_mysql, 0) != 0) {
        ERR("MySQL begin transaction failed: {}", mysql_error(_mysql));
        return false;
    }
    return true;
}

// 提交事务
bool MySQLDatabase::commit() {
    // 1. 检查数据库是否已连接
    if (!_connected || _mysql == nullptr) {
        return false;
    }
    // 2. 提交事务
    if (mysql_commit(_mysql) != 0) {
        ERR("MySQL commit failed: {}", mysql_error(_mysql));
        return false;
    }
    // 3. 重置自动提交模式
    mysql_autocommit(_mysql, 1);
    return true;
}

// 回滚事务
bool MySQLDatabase::rollback() {
    // 1. 检查数据库是否已连接
    if (!_connected || _mysql == nullptr) {
        return false;
    }
    // 2. 回滚事务
    if (mysql_rollback(_mysql) != 0) {
        ERR("MySQL rollback failed: {}", mysql_error(_mysql));
        return false;
    }
    // 3. 重置自动提交模式
    mysql_autocommit(_mysql, 1);
    return true;
}

// 对标识符进行转义
// 如果标识符已经被反引号包裹，则不再重复包裹
std::string MySQLDatabase::quoteIdentifier(const std::string& identifier) {
    if (!identifier.empty() && identifier.front() == '`' && identifier.back() == '`') {
        return identifier;
    }
    return "`" + identifier + "`";
}

// 获取数据库表列表
std::vector<std::string> MySQLDatabase::listTables() {
    // 1. 执行查询语句
    std::vector<std::string> tables;
    auto result = executeQuery("SHOW TABLES");
    if (result->success()) {
        // 2. 解析查询结果
        tables.reserve(result->_rows.size());
        for (const auto& row : result->_rows) {
            if (!row.empty()) {
                tables.push_back(row[0]);
            }
        }
    }
    return tables;
}

// 获取表结构
std::vector<ColumnInfo> MySQLDatabase::getTableStruct(const std::string& tableName) {
    std::vector<ColumnInfo> columns;
    std::string sql = "DESC " + quoteIdentifier(tableName);
    auto result = executeQuery(sql);
    if (!result->success()) {
        return columns;
    }

    for (const auto& row : result->_rows) {
        if (row.size() >= 6) {
            ColumnInfo col;
            col.name = row[0];
            col.type = row[1];
            col.nullable = (row[2] == "YES");
            col.primaryKey = (row[3] == "PRI");
            col.defaultValue = row[4];
            col.autoIncrement = (row[5].find("auto_increment") != std::string::npos);
            columns.push_back(col);
        }
    }
    return columns;
}

// 将Excel解析服务输出的类型转换为MySQL支持的SQL类型
std::string MySQLDatabase::convertExcelTypeToSql(const std::string& excelType) {
    // Excel解析服务inferColumnType输出的类型： "BIGINT", "DOUBLE", "BOOLEAN", "DATE", "TEXT"
    // MySQL直接透传这些类型
    return excelType;
}

// 获取数据库类型
DBType MySQLDatabase::getDatabaseType() const {
    return DBType::MYSQL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MySQLDatabase::ParamBinder::ParamBinder(const std::string& sql, const std::vector<PreparedParam>& params)
    : _paramCount(params.size()) {
    if (_paramCount == 0) {
        return;
    }

    // 预留内存空间
    _binds.resize(_paramCount);
    memset(_binds.data(), 0, _paramCount * sizeof(MYSQL_BIND));
    _longBuffer.resize(_paramCount);
    _doubleBuffer.resize(_paramCount);
     _stringLengthBuffer.resize(_paramCount);
    _boolBuffer = new bool[_paramCount];
    memset(_boolBuffer, 0, _paramCount * sizeof(bool));
    _nullBuffer = new bool[_paramCount];
    memset(_nullBuffer, 0, _paramCount * sizeof(bool));
   
    // 绑定参数
    for (size_t i = 0; i < _paramCount; ++i) {
        bindParam(i, params[i]);
    }
}

MySQLDatabase::ParamBinder::~ParamBinder() {
    if (_paramCount > 0) {
        delete[] _boolBuffer;
        delete[] _nullBuffer;
    }
}

MYSQL_BIND* MySQLDatabase::ParamBinder::getBinds() {
    return _binds.data();
}

size_t MySQLDatabase::ParamBinder::getParamCount() const {
    return _paramCount;
}

void MySQLDatabase::ParamBinder::bindParam(size_t index, const PreparedParam& param) {
    // 根据参数的类型进行绑定
    switch (param.getType()) {
        case ParamType::Null:
            _binds[index].buffer_type = MYSQL_TYPE_NULL;
            _binds[index].is_null = &_nullBuffer[index];
            _nullBuffer[index] = 1;
            break;
        case ParamType::Int:
            _binds[index].buffer_type = MYSQL_TYPE_LONGLONG;
            _longBuffer[index] = param.getIntValue();
            _binds[index].buffer = &_longBuffer[index];
            _binds[index].is_null = &_nullBuffer[index];
            _nullBuffer[index] = 0;
            break;
        case ParamType::Double:
            _binds[index].buffer_type = MYSQL_TYPE_DOUBLE;
            _doubleBuffer[index] = param.getDoubleValue();
            _binds[index].buffer = &_doubleBuffer[index];
            _binds[index].is_null = &_nullBuffer[index];
            _nullBuffer[index] = 0;
            break;
        case ParamType::String:
            _binds[index].buffer_type = MYSQL_TYPE_STRING;
            _stringBuffer.push_back(param.getStringValue());
            _stringLengthBuffer[index] = static_cast<unsigned long>(_stringBuffer.back().length());
            _binds[index].buffer = const_cast<char*>(_stringBuffer.back().c_str());
            _binds[index].buffer_length = _stringLengthBuffer[index];
            _binds[index].is_null = &_nullBuffer[index];
            _nullBuffer[index] = 0;
            break;
        case ParamType::Bool:
            _binds[index].buffer_type = MYSQL_TYPE_TINY;
            _boolBuffer[index] = param.getBoolValue();
            _binds[index].buffer = &_boolBuffer[index];
            _binds[index].is_null = &_nullBuffer[index];
            _nullBuffer[index] = 0;
            break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MySQLDatabase::ResultBinder::ResultBinder(MYSQL_STMT* stmt, MYSQL_RES* meta, std::shared_ptr<QueryResult> result)
    : _stmt(stmt), _meta(meta), _result(result) {
    _fieldCount = mysql_num_fields(_meta);
}

MySQLDatabase::ResultBinder::~ResultBinder() {
    if (_fieldCount > 0) {
        delete[] _boolBuffer;
        delete[] _nullBuffer;
    }
}

bool MySQLDatabase::ResultBinder::bind() {
    if (_fieldCount == 0) {
        return false;
    }
    // 预留空间
    _binds.resize(_fieldCount);
    memset(_binds.data(), 0, _fieldCount * sizeof(MYSQL_BIND));
    _stringBuffer.resize(_fieldCount);
    _stringLengthBuffer.resize(_fieldCount);
    _longBuffer.resize(_fieldCount);
    _doubleBuffer.resize(_fieldCount);
    _boolBuffer = new bool[_fieldCount];
    memset(_boolBuffer, 0, _fieldCount * sizeof(bool));
    _nullBuffer = new bool[_fieldCount];
    memset(_nullBuffer, 0, _fieldCount * sizeof(bool));

    // 获取字段信息
    MYSQL_FIELD* fields = mysql_fetch_fields(_meta);
    for (size_t i = 0; i < _fieldCount; ++i) {
        _result->_columns.push_back(fields[i].name);
        switch (fields[i].type) {
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_LONGLONG:
            case MYSQL_TYPE_INT24:
                _binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
                _binds[i].buffer = &_longBuffer[i];
                _binds[i].is_null = &_nullBuffer[i];
                _result->_columnTypes.push_back("BIGINT");
                break;
            case MYSQL_TYPE_DOUBLE:
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_FLOAT:
                _binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
                _binds[i].buffer = &_doubleBuffer[i];
                _binds[i].is_null = &_nullBuffer[i];
                _result->_columnTypes.push_back("DOUBLE");
                break;
            case MYSQL_TYPE_TINY:
                _binds[i].buffer_type = MYSQL_TYPE_TINY;
                _binds[i].buffer = &_boolBuffer[i];
                _binds[i].is_null = &_nullBuffer[i];
                _result->_columnTypes.push_back("BOOLEAN");
                break;
            default:
                _binds[i].buffer_type = MYSQL_TYPE_STRING;
                _binds[i].buffer = const_cast<char*>(_stringBuffer[i].data());
                _binds[i].buffer_length = 1024;
                _binds[i].length = &(_stringLengthBuffer[i]);
                _binds[i].is_null = &_nullBuffer[i];
                _result->_columnTypes.push_back("VARCHAR");
                break;
        }
    }

    // 绑定结果
    if (mysql_stmt_bind_result(_stmt, _binds.data()) != 0) {
        ERR("MySQL stmt bind result failed: {}", mysql_stmt_error(_stmt));
        return false;
    }

    // 存储结果
    if (mysql_stmt_store_result(_stmt) != 0) {
        ERR("MySQL stmt store result failed: {}", mysql_stmt_error(_stmt));
        return false;
    }
    
    // 逐行获取结果
    while (mysql_stmt_fetch(_stmt) == 0) {
        std::vector<std::string> rowData;
        for (size_t i = 0; i < _fieldCount; ++i) {
            if (_nullBuffer[i]) {
                rowData.push_back("");
            } else {
                switch (fields[i].type) {
                    case MYSQL_TYPE_LONG:
                    case MYSQL_TYPE_LONGLONG:
                    case MYSQL_TYPE_INT24:
                        rowData.push_back(std::to_string(_longBuffer[i]));
                        break;
                    case MYSQL_TYPE_DOUBLE:
                    case MYSQL_TYPE_DECIMAL:
                    case MYSQL_TYPE_FLOAT:
                        rowData.push_back(std::to_string(_doubleBuffer[i]));
                        break;
                    case MYSQL_TYPE_TINY:
                        rowData.push_back(_boolBuffer[i] ? "1" : "0");
                        break;
                    default:
                        rowData.push_back(_stringBuffer[i].substr(0, _stringLengthBuffer[i]));
                        break;
                }
            }
        }
        _result->_rows.push_back(rowData);
    }
    return true;
}

} // namespace databaseService
