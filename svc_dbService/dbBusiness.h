#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <gflags/gflags.h>
#include <bite_scaffold/rpc.h>
#include "dbConnMgr.h"
#include "../proto/protoCode/dbService.pb.h"

// 声明gflags变量（在main.cc中定义）
DECLARE_string(file_service);

namespace databaseService {

// 表数据查询结果
struct TableDataResult {
    std::vector<std::string> columns;             // 列名集合
    std::vector<std::string> columnTypes;         // 列类型集合
    std::vector<std::vector<std::string>> rows;   // 行数据集合
    int totalRows = 0;                            // 总行数
    int currentPage = 1;                          // 当前页码
    int totalPages = 1;                           // 总页数
    int pageSize = 50;                            // 每页行数
};

// 原表和临时表的映射关系结构
struct TempTableBackup {
    std::string originalTable; // 原表名
    std::string tempTable;     // 临时表名
    int64_t backupTime;       // 备份时间戳
};

class DBBusiness {
public:
    DBBusiness(std::shared_ptr<DBConnMgr> connMgr, std::shared_ptr<biterpc::SvcChannels> svcChannels, MySQLConfig defaultMySQLConfig);
    // 新建数据库连接
    std::string connectDatabase(const std::string& userId, const chat2Data::DatabaseService::DatabaseConfig& config);
    // 断开数据库连接
    bool disconnectDatabase(const std::string& connectionId);
    // 获取数据库表列表---除过系统表
    std::vector<std::string> listTables(const std::string& connectionId);
    // 获取指定表的数据
    TableDataResult getTableData(const std::string& connectionId,
                                 const std::string& tableName,
                                 bool forceOriginal,
                                 int32_t pageNumber,
                                 int32_t pageSize);
    // 执行SQL语句
    chat2Data::DatabaseService::ExecuteSQLResponse executeSQL(const std::string& connectionId, const std::string& sql);
    // 获取指定表的结构
    std::string getTableStruct(const std::string& connectionId, const std::string& tableName);
    // 获取表的采样数据
    std::string getSampleData(const std::string& connectionId, const std::string& tableName, int32_t limit);
    // 导入Excel数据到指定表
    using WorksheetData = chat2Data::excelParseService::WorksheetData;
    chat2Data::DatabaseService::ImportExcelDataResult importExcelData(const std::string& connectionId, const std::string& tableName, const WorksheetData& worksheetData);
    // 删除指定表的Excel数据
    chat2Data::DatabaseService::DropTableExcelResult dropTableExcel(const std::string& connectionId, const std::vector<std::string>& tableNames);
    // 获取指定连接下的所有临时表
    std::vector<std::string> getConnTempTables(const std::string& connectionId);
    // 删除指定用户创建的所有临时表
    bool deleteUserAllConn(const std::string& userId);

private:
    // 获取数据库连接
    std::shared_ptr<IDatabase> getDatabase(const std::string& connectionId);
    // 备份数据库表
    std::string backupTable(const std::string& connectionId, const std::string& tableName);
    // 删除指定临时表
    bool deleteTempTable(const std::string& connectionId, const std::string& tempTable);
    // 删除指定连接下的所有临时表
    bool deleteTempTablesForConnection(const std::string& connectionId);
    // 替换SQL语句中的表名为临时表名
    std::string replaceTableNameWithTemp(const std::string& connectionId, const std::string& sql, const std::string& originalTable);
    // 构建临时表名
    std::string buildTempTableName(const std::string& originalTable);
    // 创建worksheet对应的数据库表
    bool createTableForWorksheet(const std::string& connectionId, const std::string& tableName, const WorksheetData& worksheetData);
    // 导入worksheet数据到指定表
    bool importWorksheetData(const std::string& connectionId, const std::string& tableName, const WorksheetData& worksheetData);
    // 将值转换为SQL类型
    std::string convertValueToSqlType(const std::string& value, const std::string& colType);
    // 从文件子服务获取SQLite文件并下载到本地
    std::string downloadSQLiteFile(const std::string& fileId);

private:
    std::shared_ptr<DBConnMgr> _connMgr;                                // 数据库连接管理器
    std::shared_ptr<biterpc::SvcChannels> _svcChannels;                 // 信道管理器
    // key: 连接ID, value: 临时表备份列表
    std::map<std::string, std::vector<TempTableBackup>> _tempTableMap;  // 临时表备份映射表
    std::mutex _tempTableMapMutex;                                     // 临时表备份映射表互斥锁
    const static int BATCH_SIZE = 100;                                // 批量插入数据的批次大小
};

} // namespace databaseService
