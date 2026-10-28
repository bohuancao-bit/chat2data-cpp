#include <algorithm>
#include <chrono>
#include <bite_scaffold/log.h>
#include "dbConnMgr.h"
#include "../common/errorHandler.h"
#include "../../common/utils.h"

namespace databaseService {

DBConnMgr::DBConnMgr() {
    // 该线程每5分钟检查一次过期连接，如果存在则移除
    _cleanThread = std::thread([this]() {
        while (!_stopClean) {
            std::this_thread::sleep_for(std::chrono::milliseconds(MINUTE_5));
            if (_stopClean) {
                break;
            }
            cleanExpiredConnections();
        }
    });
    INF("DBConnMgr started, cleanup thread running");
}

DBConnMgr::~DBConnMgr() {
    // 1. 将子线程设置为停止状态
    _stopClean = true;

    // 2. 将子线程等待完成
    if (_cleanThread.joinable()) {
        _cleanThread.join();
    }

    // 3. 清理所有连接
    _connections.clear();
    _userIdToConnIds.clear();
    INF("DBConnMgr destroyed");
}

std::string DBConnMgr::createConnection(const std::string& userId, std::shared_ptr<IDatabase> db, bool isDefaultConnection, const std::string& sqliteFilePath) {
    // 1. 设置连接id的值。默认连接使用excel_default，非默认连接使用uuid
    std::string connectionId;
    if (isDefaultConnection) {
        connectionId = EXCEL_DEFAULT_CONN_ID;
    } else {
        connectionId = chat2Data::Utils::generateUuid();
    }

    std::lock_guard<std::shared_mutex> lock(_mutex);

    // 2. 构造连接对象
    ConnInfo connInfo;
    connInfo.connectionId = connectionId;
    connInfo.db = db;
    connInfo.userId = userId;
    connInfo.createTime = getCurrentTimeMillis();
    connInfo.lastActiveTime = connInfo.createTime;
    connInfo.isDefaultConnection = isDefaultConnection;
    connInfo.sqliteFilePath = sqliteFilePath;

    // 3. 建立连接的映射关系
    _connections[connectionId] = connInfo;
    // 4. 非默认连接添加用户连接管理器中
    if (!isDefaultConnection && !userId.empty()) {
        _userIdToConnIds[userId].push_back(connectionId);
    }

    INF("Connection created: connId={}, userId={}, isDefault={}", connectionId, userId, isDefaultConnection);
    return connectionId;
}

std::shared_ptr<ConnInfo> DBConnMgr::getConnection(const std::string& connectionId) {
    std::shared_lock<std::shared_mutex> lock(_mutex);

    // 查找指定连接ID的连接信息，并检测该连接是否有效
    auto it = _connections.find(connectionId);
    if (it != _connections.end()) {
        if (it->second.db->ping()) {
            it->second.lastActiveTime = getCurrentTimeMillis();
            return std::make_shared<ConnInfo>(it->second);
        }
    }
    return nullptr;
}

bool DBConnMgr::removeConnection(const std::string& connectionId) {
    std::lock_guard<std::shared_mutex> lock(_mutex);

    auto it = _connections.find(connectionId);
    if (it != _connections.end()) {
        
        // 默认连接不删除，删除非默认连接
        if (!it->second.isDefaultConnection) {
            // 断开数据库连接
            it->second.db->disconnect();

            // 删除用户连接管理集合中的connectionId
            auto userIt = _userIdToConnIds.find(it->second.userId);
            if (userIt != _userIdToConnIds.end()) {
                // 在vector中找到ConnectionId并删除
                std::vector<std::string>& connIds = userIt->second;
                connIds.erase(std::find(connIds.begin(), connIds.end(), connectionId));
                // 删除之后该vector如果为空，即该用户创建的连接已经被全部删除了
                if (connIds.empty()) {
                    _userIdToConnIds.erase(userIt);
                }
            }

            // 从连接管理器中删除待删除的连接
             _connections.erase(it);
        }
       
        INF("Connection removed: connId={}", connectionId);
        return true;
    }
    return false;
}

std::vector<std::string> DBConnMgr::deleteUserAllConnections(const std::string& userId) {
    std::lock_guard<std::shared_mutex> lock(_mutex);

    auto userIt = _userIdToConnIds.find(userId);
    if (userIt == _userIdToConnIds.end()) {
        return {};
    }

    std::vector<std::string> deletedConnIds;
    // 删除该用户创建的所有连接
    for (const auto& connId : userIt->second) {
        auto connIt = _connections.find(connId);
        if (connIt != _connections.end()) {
            connIt->second.db->disconnect();
            _connections.erase(connIt);
            deletedConnIds.push_back(connId);
            INF("User connection deleted: connId={}", connId);
        }
    }
    _userIdToConnIds.erase(userIt);
    INF("User all connections deleted: userId={}", userId);
    return deletedConnIds;
}

std::vector<std::string> DBConnMgr::getUserConnectionIds(const std::string& userId) {
    std::lock_guard<std::shared_mutex> lock(_mutex);
    auto userIt = _userIdToConnIds.find(userId);
    if (userIt == _userIdToConnIds.end()) {
        return {};
    }
    return userIt->second;
}

void DBConnMgr::cleanExpiredConnections() {
    std::lock_guard<std::shared_mutex> lock(_mutex);
    // 1. 获取当前时间戳
    int64_t now = getCurrentTimeMillis();
    std::vector<std::string> expiredConnIds;

    // 2. 检测所有的过期连接
    for (const auto& pair : _connections) {
        // 注意：默认连接不需要清理
        if (pair.second.isDefaultConnection) {
            continue;
        }
        // 超过1个小时未使用的非默认连接需要被清理
        if (now - pair.second.lastActiveTime > HOUR_1) {
            expiredConnIds.push_back(pair.first);
        }
    }

    // 3. 清理过期连接
    for (const auto& connId : expiredConnIds) {
        auto it = _connections.find(connId);
        if (it != _connections.end()) {
            if (!it->second.isDefaultConnection) {
                auto userIt = _userIdToConnIds.find(it->second.userId);
                if (userIt != _userIdToConnIds.end()) {
                    auto& connIds = userIt->second;
                    connIds.erase(std::find(connIds.begin(), connIds.end(), connId));
                    if (connIds.empty()) {
                        _userIdToConnIds.erase(userIt);
                    }
                }

                // 非默认连接需要断开
                it->second.db->disconnect();
            }
            
            INF("Expired connection cleaned: connId={}", connId);
        }
    }

    // 清除_connections 中的过期连接
    if (!expiredConnIds.empty()) {
        for (const auto& connId : expiredConnIds) {
            _connections.erase(connId);
        }
    }
}

// 获取当前时间戳，单位毫秒
int64_t DBConnMgr::getCurrentTimeMillis() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace databaseService
