#include <sstream>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <bite_scaffold/log.h>
#include "dbBusiness.h"
#include "dbConnMgr.h"
#include "dbDriver/databaseFactory.h"
#include "dbDriver/databaseSchema.h"
#include "../common/sqlValidator.h"
#include "../common/errorHandler.h"
#include "../common/utils.h"
#include "../proto/protoCode/fileService.pb.h"
#include <bite_scaffold/fdfs.h>

namespace databaseService {

DBBusiness::DBBusiness(std::shared_ptr<DBConnMgr> connMgr, std::shared_ptr<biterpc::SvcChannels> svcChannels, MySQLConfig defaultMySQLConfig)
    : _connMgr(connMgr)
    , _svcChannels(svcChannels) {
    // 在构造函数中创建默认MySQL连接
    auto db = DatabaseFactory::getInstance()->createDatabase(&defaultMySQLConfig);
    if (!db) {
        ERR("Failed to create default MySQL database");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_FAILED);
    }
    if (!db->connect()) {
        ERR("Failed to connect to default MySQL database");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_FAILED);
    }
    _connMgr->createConnection("", db, true);
    INF("DBBusiness initialized with default MySQL connection");
}

std::string DBBusiness::connectDatabase(const std::string& userId, const chat2Data::DatabaseService::DatabaseConfig& config) {
    DBConfig* dbConfig = nullptr;
    MySQLConfig mysqlConfig;
    SQLiteConfig sqliteConfig;
    std::string sqliteFilePath;

    // 1. 根据数据库类型设置数据库配置
    if (config.type() == chat2Data::DatabaseService::DATABASE_TYPE_MYSQL) {
        // 设置MySQL连接配置
        const auto& mysqlConf = config.mysql_config();
        mysqlConfig.host = mysqlConf.host();
        mysqlConfig.port = mysqlConf.port();
        mysqlConfig.username = mysqlConf.username();
        mysqlConfig.password = mysqlConf.password();
        mysqlConfig.database = mysqlConf.name();
        mysqlConfig.charset = mysqlConf.charset().empty() ? "utf8mb4" : mysqlConf.charset();
        dbConfig = &mysqlConfig;
    } else if (config.type() == chat2Data::DatabaseService::DATABASE_TYPE_SQLITE) {
        // 设置SQLite连接配置
        const auto& sqliteConf = config.sqlite_config();
        std::string fileId = sqliteConf.file_id();
        if (fileId.empty()) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_PARAM_INVALID);
        }
        sqliteFilePath = downloadSQLiteFile(fileId);
        sqliteConfig.dbPath = sqliteFilePath;
        dbConfig = &sqliteConfig;
    } else {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_PARAM_INVALID);
    }

    // 2. 验证数据库配置是否有效
    if (!dbConfig->validConfig()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_PARAM_INVALID);
    }

    // 3. 创建数据库连接实例
    auto db = DatabaseFactory::getInstance()->createDatabase(dbConfig);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_FAILED);
    }
    if (!db->connect()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_FAILED);
    }

    // 4. 将该实例交由连接管理器管理
    return _connMgr->createConnection(userId, db, false, sqliteFilePath);
}

bool DBBusiness::disconnectDatabase(const std::string& connectionId) {
    // 0. 获取连接信息，检查是否为SQLite连接
    auto connInfo = _connMgr->getConnection(connectionId);
    if (connInfo && connInfo->db->getDatabaseType() == DBType::SQLITE && !connInfo->sqliteFilePath.empty()) {
        std::filesystem::path sqlitePath(connInfo->sqliteFilePath);
        std::string baseName = sqlitePath.filename().string();
        std::filesystem::path parentDir = sqlitePath.parent_path();

        // 删除与sqlite文件相同名称的所有文件
        std::error_code ec;
        if (std::filesystem::exists(parentDir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(parentDir, ec)) {
                if (entry.is_regular_file(ec) && entry.path().filename().string() == baseName) {
                    std::filesystem::remove(entry.path(), ec);
                    if (ec) {
                        WRN("Failed to delete SQLite file: {}, error: {}", entry.path().string(), ec.message());
                    } else {
                        INF("Deleted SQLite file: {}", entry.path().string());
                    }
                }
            }
        }
    }

    // 1. 清除该连接下创建的所有的临时表
    deleteTempTablesForConnection(connectionId);

    // 2. 清除该数据库连接：从连接管理器中移除该连接，并断开数据库连接
    return _connMgr->removeConnection(connectionId);
}

std::vector<std::string> DBBusiness::listTables(const std::string& connectionId) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS);
    }

    // 2. 调用数据库驱动的listTables方法获取所有表名
    return db->listTables();
}

TableDataResult DBBusiness::getTableData(const std::string& connectionId,
                                         const std::string& tableName,
                                         bool forceOriginal,
                                         int32_t pageNumber,
                                         int32_t pageSize) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS);
    }

    // 2. 创建查询结果返回结构
    TableDataResult tableResult;
    tableResult.pageSize = pageSize > 0 ? pageSize : 50;
    tableResult.currentPage = pageNumber > 0 ? pageNumber : 1;

    // 3. 确定实际查询的表名
    std::string actualTableName = tableName;
    if (!forceOriginal) {
        // 获取临时表数据
        std::lock_guard<std::mutex> lock(_tempTableMapMutex);
        auto connIt = _tempTableMap.find(connectionId);
        if (connIt != _tempTableMap.end()) {
            for (const auto& backup : connIt->second) {
                if (backup.originalTable == tableName) {
                    actualTableName = backup.tempTable;
                    break;
                }
            }
        }
    }

    // 4. 获取表中数据的总行数
    std::string countSql = "SELECT COUNT(*) FROM " + db->quoteIdentifier(actualTableName);
    auto countResult = db->executeQuery(countSql);
    if (countResult->success() && !countResult->_rows.empty()) {
        tableResult.totalRows = std::stoi(countResult->_rows[0][0]);
    }

    // 5. 查询指定页码的数据
    tableResult.totalPages = (tableResult.totalRows + tableResult.pageSize - 1) / tableResult.pageSize;
    int offset = (tableResult.currentPage - 1) * tableResult.pageSize;
    std::string dataSql = "SELECT * FROM " + db->quoteIdentifier(actualTableName) +
                          " LIMIT " + std::to_string(tableResult.pageSize) +
                          " OFFSET " + std::to_string(offset);
    auto dataResult = db->executeQuery(dataSql);
    if (!dataResult->success()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_TABLE_DATA_GET_FAILED);
    }

    // 6. 解析查询结果，提取表数据
    tableResult.columns = dataResult->_columns;
    tableResult.rows = dataResult->_rows;

    // 7. 获取表结构
    auto columnInfos = db->getTableStruct(actualTableName);
    for (const auto& col : columnInfos) {
        tableResult.columnTypes.push_back(col.type);
    }

    return tableResult;
}

chat2Data::DatabaseService::ExecuteSQLResponse DBBusiness::executeSQL(const std::string& connectionId,const std::string& sql) {

    chat2Data::DatabaseService::ExecuteSQLResponse response;

    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        response.set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS));
        response.set_error_msg("Database connection not exists");
        return response;
    }

    // 2. 验证SQL语句是否有效
    std::string normalizedSql = sql;
    if (!chat2Data::SQLValidator::validate(normalizedSql)) {
        response.set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::DB_PARAM_INVALID));
        response.set_error_msg("Invalid SQL statement");
        return response;
    }

    // 3. 确定SQL语句的类型
    bool isModify = chat2Data::SQLValidator::isModifySQL(normalizedSql);

    // 4. 检测是否为修改类SQL，修改类SQL需要再备份表上操作
    std::string sqlToExecute = normalizedSql;
    if (isModify) {
        // 4.1 从SQL语句中提取表名
        std::string tableName = chat2Data::SQLValidator::extractTableName(normalizedSql);

        // 4.2 将原表进行备份
        std::string tempTableName = backupTable(connectionId, tableName);
        if (tempTableName.empty()) {
            response.set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::DB_BACKUP_TABLE_FAILED));
            response.set_error_msg("Failed to backup table");
            return response;
        }

        // 4.3 将原sql中的原表名替换为临时表名
        sqlToExecute = replaceTableNameWithTemp(connectionId, normalizedSql, tableName);
    }

    // 5. 执行SQL语句
    std::shared_ptr<QueryResult> result;
    if (isModify) {
        result = db->executeModify(sqlToExecute);
    } else {
        result = db->executeQuery(sqlToExecute);
    }

    // 6. 解析查询结果，提取表数据
    response.set_error_code(result->success() ? 0 :
        static_cast<int32_t>(chat2Data::ErrorCode::DB_SQL_EXECUTE_FAILED));
    response.set_error_msg(result->success() ? "" : result->_errorMsg);
    response.set_is_query(!isModify);
    response.set_affected_rows(result->_affectedRows);

    for (const auto& col : result->_columns) {
        response.add_columns(col);
    }
    for (const auto& colType : result->_columnTypes) {
        response.add_column_types(colType);
    }
    for (const auto& row : result->_rows) {
        auto* protoRow = response.add_rows();
        for (const auto& cell : row) {
            protoRow->add_cells(cell);
        }
    }

    return response;
}

std::string DBBusiness::getTableStruct(const std::string& connectionId, const std::string& tableName) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS);
    }

    // 2. 获取表结构并格式化为字符串
    auto columnInfos = db->getTableStruct(tableName);
    std::ostringstream oss;
    oss << "Table: " << tableName << "\n";
    for (const auto& col : columnInfos) {
        oss << col.name << " " << col.type;
        if (col.primaryKey) oss << " PRIMARY KEY";
        if (!col.nullable) oss << " NOT NULL";
        if (!col.defaultValue.empty()) oss << " DEFAULT " << col.defaultValue;
        if (col.autoIncrement) oss << " AUTO_INCREMENT";
        oss << "\n";
    }
    return oss.str();
}

std::string DBBusiness::getSampleData(const std::string& connectionId, const std::string& tableName, int32_t limit) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS);
    }

    // 2. 构造查询的sql语句
    int32_t sampleLimit = (limit > 0) ? limit : 5;
    std::string sql = "SELECT * FROM " + db->quoteIdentifier(tableName) +
                       " LIMIT " + std::to_string(sampleLimit);

    // 3. 执行SQL语句
       auto result = db->executeQuery(sql);
    if (!result->success()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_SAMPLE_DATA_GET_FAILED);
    }

    // 4. 解析查询结果，提取表数据
    std::ostringstream oss;
    bool firstRow = true;
    for (const auto& row : result->_rows) {
        if (!firstRow) oss << "\n";
        firstRow = false;
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) oss << "\t";
            oss << row[i];
        }
    }
    INF("sample data: {}", oss.str());
    return oss.str();
}

chat2Data::DatabaseService::ImportExcelDataResult DBBusiness::importExcelData(const std::string& connectionId, const std::string& tableName, const WorksheetData& worksheetData) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS);
    }

    // 2. 创建表
    if (!createTableForWorksheet(connectionId, tableName, worksheetData)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_IMPORT_DATA_FAILED);
    }

    // 3. 导入数据
    if (!importWorksheetData(connectionId, tableName, worksheetData)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_IMPORT_DATA_FAILED);
    }

    // 4. 返回导入结果
    chat2Data::DatabaseService::ImportExcelDataResult result;
    int rowCount = worksheetData.rows_size();
    result.set_table_name(tableName);
    result.set_imported_rows(rowCount);

    INF("Excel data imported: tableName={}, rows={}", tableName, rowCount);
    return result;
}

chat2Data::DatabaseService::DropTableExcelResult DBBusiness::dropTableExcel(const std::string& connectionId, const std::vector<std::string>& tableNames) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_NOT_EXISTS);
    }

    // 2. 遍历表名列表，执行删除表SQL语句
    chat2Data::DatabaseService::DropTableExcelResult result;
    for (const auto& tableName : tableNames) {
        // 2.1 构造sql语句
        std::string dropSql = "DROP TABLE IF EXISTS " + db->quoteIdentifier(tableName);

        // 2.2 执行删除表SQL语句
        auto dropResult = db->executeModify(dropSql);

        // 2.3 检查删除表操作是否成功
        bool success = true;
        if (!dropResult->success()) {
            success = false;
            result.add_failed_tables(tableName);
        } else {
            result.add_dropped_tables(tableName);
            // 2.4 根据原始表名查找对应的临时表名，并删除临时表
            std::lock_guard<std::mutex> lock(_tempTableMapMutex);
            auto connIt = _tempTableMap.find(connectionId);
            if (connIt != _tempTableMap.end()) {
                for (const auto& backup : connIt->second) {
                    if (backup.originalTable == tableName) {
                        deleteTempTable(connectionId, backup.tempTable);
                        break;
                    }
                }
            }
        }

        // 2.5 从临时表映射中删除该表的备份信息
        if (success) {
            std::lock_guard<std::mutex> lock(_tempTableMapMutex);
            auto connIt = _tempTableMap.find(connectionId);
            if (connIt != _tempTableMap.end()) {
                connIt->second.erase(
                    std::remove_if(connIt->second.begin(), connIt->second.end(),
                        [&tableName](const TempTableBackup& backup) {
                            return backup.originalTable == tableName;
                        }),
                    connIt->second.end());
            }
        }
    }

    result.set_dropped_count(result.dropped_tables_size());
    return result;
}

std::vector<std::string> DBBusiness::getConnTempTables(const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(_tempTableMapMutex);
    std::vector<std::string> tempTables;

    auto connIt = _tempTableMap.find(connectionId);
    if (connIt != _tempTableMap.end()) {
        for (const auto& backup : connIt->second) {
            tempTables.push_back(backup.tempTable);
        }
    }
    return tempTables;
}

bool DBBusiness::deleteUserAllConn(const std::string& userId) {
    // 1. 获取用户的所有连接ID
    auto connIds = _connMgr->getUserConnectionIds(userId);

    // 2. 删除每个连接的临时表
    for (const auto& connId : connIds) {
        auto tempTables = getConnTempTables(connId);
        for (const auto& tableName : tempTables) {
            deleteTempTable(connId, tableName);
        }
    }

    // 3. 删除用户所有连接
    _connMgr->deleteUserAllConnections(userId);
    return true;
}

std::shared_ptr<IDatabase> DBBusiness::getDatabase(const std::string& connectionId) {
    auto connInfo = _connMgr->getConnection(connectionId);
    if (!connInfo) {
        return nullptr;
    }
    return connInfo->db;
}

std::string DBBusiness::backupTable(const std::string& connectionId, const std::string& tableName) {
    // 1. 从连接管理器中获取数据库连接
    auto db = getDatabase(connectionId);
    if (!db) {
        return "";
    }

    std::lock_guard<std::mutex> lock(_tempTableMapMutex);

    // 2. 检查是否存在相同表名的临时表，如果存在则删除
    auto connIt = _tempTableMap.find(connectionId);
    if (connIt != _tempTableMap.end()) {
        for (const auto& backup : connIt->second) {
            if (backup.originalTable == tableName) {
                std::string dropOldTemp = "DROP TABLE IF EXISTS " + db->quoteIdentifier(backup.tempTable);
                db->executeModify(dropOldTemp);
            }
        }
    }

    // 3. 生成临时表名
    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string tempTableName = buildTempTableName(tableName) + "_" + std::to_string(timestamp);

    // 4. 执行备份SQL语句
    std::string backupSql = "CREATE TABLE " + db->quoteIdentifier(tempTableName) +
                            " AS SELECT * FROM " + db->quoteIdentifier(tableName);
    auto result = db->executeModify(backupSql);
    if (!result->success()) {
        ERR("Failed to backup table: {} to {}", tableName, tempTableName);
        return "";
    }

    // 5. 记录备份信息
    TempTableBackup backup;
    backup.originalTable = tableName;
    backup.tempTable = tempTableName;
    backup.backupTime = timestamp;
    _tempTableMap[connectionId].push_back(backup);

    INF("Table backed up: {} -> {}", tableName, tempTableName);
    return tempTableName;
}

bool DBBusiness::deleteTempTable(const std::string& connectionId, const std::string& tempTable) {
    // 1. 从连接管理器中获取数据库连接
    auto db = getDatabase(connectionId);
    if (!db) {
        return false;
    }

    // 2. 构造删除临时表SQL语句
    std::string dropSql = "DROP TABLE IF EXISTS " + db->quoteIdentifier(tempTable);

    // 3. 执行删除临时表SQL语句
    auto result = db->executeModify(dropSql);
    if (!result->success()) {
        ERR("Failed to delete temp table: {}", tempTable);
        return false;
    }

    INF("Temp table deleted: {}", tempTable);
    return true;
}

bool DBBusiness::deleteTempTablesForConnection(const std::string& connectionId) {
    // 1. 获取该连接下创建的所有的临时表
    auto tempTables = getConnTempTables(connectionId);

    // 2. 遍历临时表集合，删除每个临时表
    for (const auto& tempTable : tempTables) {
        if (!deleteTempTable(connectionId, tempTable)) {
            WRN("Failed to delete temp table: {} for connection: {}", tempTable, connectionId);
        }
    }

    // 3. 从临时表映射关系中删除该连接对应的临时表备份信息
    std::lock_guard<std::mutex> lock(_tempTableMapMutex);
    _tempTableMap.erase(connectionId);

    return true;
}

std::string DBBusiness::replaceTableNameWithTemp(const std::string& connectionId, const std::string& sql, const std::string& originalTable) {
    std::lock_guard<std::mutex> lock(_tempTableMapMutex);

    // 1. 获取连接对应的临时表备份信息
    auto connIt = _tempTableMap.find(connectionId);
    if (connIt == _tempTableMap.end()) {
        return sql;
    }

    // 2. 遍历备份表集合，将sql中的原表名替换为临时表名
    for (const auto& backup : connIt->second) {
        // 找到了
        if (backup.originalTable == originalTable) {
            std::string modifiedSql = sql;
            // 将sql中所有的原表名替换为临时表名
            size_t pos = 0;
            while ((pos = modifiedSql.find(originalTable, pos)) != std::string::npos) {
                modifiedSql.replace(pos, originalTable.length(), backup.tempTable);
                pos += backup.tempTable.length();
            }
            return modifiedSql;
        }
    }

    // 3. 返回替换之后的sql
    return sql;
}

std::string DBBusiness::buildTempTableName(const std::string& originalTable) {
    return originalTable + "_temp";
}

bool DBBusiness::createTableForWorksheet(const std::string& connectionId, const std::string& tableName, const WorksheetData& worksheetData) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        return false;
    }

    // 2. 构建创建表SQL语句
    std::ostringstream createSql;
    createSql << "CREATE TABLE IF NOT EXISTS " << db->quoteIdentifier(tableName) << " (";
    createSql << "id INTEGER PRIMARY KEY AUTO_INCREMENT, ";

    auto& columns = worksheetData.columns();
    bool first = true;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (!first) createSql << ", ";
        first = false;
        const auto& col = columns[i];
        std::string colType = db->convertExcelTypeToSql(col.type());
        createSql << db->quoteIdentifier(col.name()) << " " << colType;
    }
    createSql << ")";
    // MySQL支持ENGINE和CHARSET，SQLite不支持
    if (db->getDatabaseType() == DBType::MYSQL) {
        createSql << " ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    }
    INF("Create table SQL: {}", createSql.str());

    // 3. 执行创建表SQL语句
    auto result = db->executeModify(createSql.str());
    if (!result->success()) {
        ERR("Failed to create table: {}", tableName);
        return false;
    }

    INF("Table created: {}", tableName);
    return true;
}

bool DBBusiness::importWorksheetData(const std::string& connectionId, const std::string& tableName, const WorksheetData& worksheetData) {
    // 1. 从连接管理器中获取该连接的数据库实例
    auto db = getDatabase(connectionId);
    if (!db) {
        return false;
    }

    // 2. 获取列类型信息（使用驱动层转换后的实际SQL类型）
    std::vector<std::string> colTypes;
    for (const auto& col : worksheetData.columns()) {
        colTypes.push_back(db->convertExcelTypeToSql(col.type()));
    }

    // 3. 构建插入数据SQL语句
    int rowCount = worksheetData.rows_size();
    int colCount = worksheetData.columns_size();
    for (int i = 0; i < rowCount; i += BATCH_SIZE) {
        std::ostringstream insertSql;
        insertSql << "INSERT INTO " << db->quoteIdentifier(tableName) << " (";

        for(int i = 0; i < colCount; ++i){
            if (i > 0) insertSql << ", ";
            insertSql << db->quoteIdentifier(worksheetData.columns(i).name());
        }

        insertSql << ") VALUES ";

        bool firstRow = true;
        for (int j = i; j < std::min(i + BATCH_SIZE, rowCount); ++j) {
            if (!firstRow) insertSql << ", ";
            firstRow = false;

            insertSql << "(";
            const auto& row = worksheetData.rows(j);
            for (int k = 0; k < colCount; ++k) {
                if (k > 0) insertSql << ", ";
                if (row.cells_size() > k) {
                    std::string value = row.cells(k).value();
                    std::string colType = colTypes[k];
                    insertSql << convertValueToSqlType(value, colType);
                    INF("Insert value: {}", value);
                } else {
                    insertSql << "NULL";
                }
            }
            insertSql << ")";
        }

        // 4. 执行插入数据SQL语句
        auto result = db->executeModify(insertSql.str());
        if (!result->success()) {
            ERR("Failed to insert batch at row: {}", i);
            return false;
        }
    }

    INF("Data imported to table: {}, rows: {}", tableName, rowCount);
    return true;
}

std::string DBBusiness::convertValueToSqlType(const std::string& value, const std::string& colType) {
    if (value.empty() || value == "NULL") {
        return "NULL";
    }
    if (colType == "INTEGER" || colType == "BIGINT" || colType == "DOUBLE" || colType == "FLOAT" || colType == "INT") {
        return value;
    } else if (colType == "BOOLEAN") {
        if (value == "true" || value == "1" || value == "TRUE") {
            return "1";
        } else {
            return "0";
        }
    } else {
        std::string result = value;
        size_t pos = 0;
        while ((pos = result.find("'", pos)) != std::string::npos) {
            result.replace(pos, 1, "''");
            pos += 2;
        }
        return "'" + result + "'";
    }
}

std::string DBBusiness::downloadSQLiteFile(const std::string& fileId) {
    // 1. 获取文件子服务的通信信道
    auto fileServiceChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileServiceChannel) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_FAILED);
    }

    // 2. 构建rpc请求
    chat2Data::fileService::GetSQLiteFileRequest req;
    req.set_request_id(chat2Data::Utils::generateUuid());
    req.set_session_id(chat2Data::Utils::generateUuid());
    req.set_file_id(fileId);

    // 3. 创建rpc客户端
    chat2Data::fileService::FileService_Stub stub(fileServiceChannel.get());

    // 4. 发起rpc调用
    chat2Data::fileService::GetSQLiteFileResponse resp;
    brpc::Controller cntl;
    stub.GetSQLiteFile(&cntl, &req, &resp, nullptr);

    // 5. 检测rpc调用是否成功
    if (cntl.Failed() || resp.error_code() != 0) {
        ERR("Failed to get SQLite file from fileService: {}", cntl.ErrorText());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_CONNECTION_FAILED);
    }

    // 6. 解析RPC响应
    std::string fdfsFileId = resp.result().fdfsfileid();

    // 7. 构建目录
    std::filesystem::path exePath = std::filesystem::current_path();
    std::filesystem::path sqliteDir = exePath / "sqliteFiles";
    std::error_code ec;
    if (!std::filesystem::exists(sqliteDir)) {
        if (!std::filesystem::create_directory(sqliteDir, ec)) {
            ERR("Failed to create sqliteFiles directory: {}", ec.message());
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_SQLITE_DOWNLOAD_FAILED);
        }
    }

    // 8. 下载文件
    std::string localPath = (sqliteDir / (fileId + ".db")).string();
    if (!bitefdfs::FDFSClient::download_to_file(fdfsFileId, localPath)) {
        ERR("Failed to download SQLite file from FastDFS");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::DB_SQLITE_DOWNLOAD_FAILED);
    }
    INF("SQLite file downloaded: {}", localPath);
    return localPath;
}

} // namespace databaseService