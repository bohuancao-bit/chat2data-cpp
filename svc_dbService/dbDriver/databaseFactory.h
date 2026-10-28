#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "database.h"
#include "databaseSchema.h"

namespace databaseService {

// 数据库工厂类：工厂模式 + 单例模式，主要负责创建不同类型的数据库实例
class DatabaseFactory {
public:
    static std::shared_ptr<DatabaseFactory> getInstance();

    // 防拷贝
    DatabaseFactory(const DatabaseFactory&) = delete;
    DatabaseFactory& operator=(const DatabaseFactory&) = delete;

    // 注册数据库类型
    void registerDatabase(DBType type, std::function<std::shared_ptr<IDatabase>(DBConfig*)> creator);

    // 创建数据库实例
    std::shared_ptr<IDatabase> createDatabase(DBConfig* config);

    // 获取支持的数据库类型
    std::vector<DBType> getSupportedTypes();

    // 检查是否支持指定数据库类型
    bool isSupported(DBType type);

private:
    DatabaseFactory();

    // 数据库类型到 数据库创建器的映射
    // key: 数据库类型   value：对应数据库类型的创建器
    std::unordered_map<DBType, std::function<std::shared_ptr<IDatabase>(DBConfig*)>> _creators;
    static std::shared_ptr<DatabaseFactory> _instance;
    static std::recursive_mutex _mutex;
};

} // namespace databaseService
