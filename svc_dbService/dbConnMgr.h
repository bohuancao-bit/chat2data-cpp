#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <thread>
#include <chrono>
#include <atomic>
#include "dbDriver/database.h"
#include "dbDriver/databaseFactory.h"
#include "dbDriver/databaseSchema.h"

namespace databaseService {

// 如果连接超过1个小时未使用，则判定为过期连接
// 连接管理器每5分钟检查一次，如果发现过期连接，则删除该连接
constexpr int64_t HOUR_1 = 60 * 60 * 1000;
constexpr int64_t MINUTE_5 = 5 * 60 * 1000;

// 默认连接，专门应对excel相关操作
const std::string EXCEL_DEFAULT_CONN_ID = "excel_default";

// 连接信息结构体
struct ConnInfo {
    std::string connectionId;         // 连接Id，非默认连接的id使用uuid，默认连接id为excel_default
    std::shared_ptr<IDatabase> db;    // 数据库连接
    std::string userId;               // 用户id，即连接所属的用户
    int64_t createTime;               // 创建时间
    int64_t lastActiveTime;           // 连接最近一次使用的时间
    bool isDefaultConnection;         // 是否为默认连接
    std::string sqliteFilePath;       // SQLite本地文件路径，连接为SQLite时有效
};

// 连接管理器
class DBConnMgr {
public:
    DBConnMgr();
    ~DBConnMgr();
    // 创建数据库连接，接收已创建好的数据库实例
    // @param sqliteFilePath SQLite本地文件路径，SQLite连接时传入，非SQLite连接传入空字符串
    std::string createConnection(const std::string& userId, std::shared_ptr<IDatabase> db, bool isDefaultConnection, const std::string& sqliteFilePath = "");
    // 获取数据库连接
    std::shared_ptr<ConnInfo> getConnection(const std::string& connectionId);
    // 删除数据库连接
    bool removeConnection(const std::string& connectionId);
    // 删除用户所有数据库连接，返回被删除的连接ID列表
    std::vector<std::string> deleteUserAllConnections(const std::string& userId);
    // 获取用户的所有连接ID列表
    std::vector<std::string> getUserConnectionIds(const std::string& userId);
    // 清理过期连接
    void cleanExpiredConnections();

private:
    int64_t getCurrentTimeMillis();

private:
    // 连接id和连接信息的映射关系
    std::unordered_map<std::string, ConnInfo> _connections;
    // 用户id和连接id的映射关系，一个用户可能创建多个连接
    std::unordered_map<std::string, std::vector<std::string>> _userIdToConnIds;
    std::shared_mutex _mutex;
    // 清理过期连接的线程
    std::thread _cleanThread;
    // 清理过期连接的线程是否停止
    std::atomic<bool> _stopClean{false};
};

} // namespace databaseService
