#include <bite_scaffold/log.h>

#include "databaseFactory.h"
#include "mysqlDatabase.h"
#include "sqliteDatabase.h"


namespace databaseService {

std::shared_ptr<DatabaseFactory> DatabaseFactory::_instance = nullptr;
std::recursive_mutex DatabaseFactory::_mutex;

DatabaseFactory::DatabaseFactory(){
    // 注册MySQL数据库驱动
    registerDatabase(DBType::MYSQL, [](DBConfig* config) -> std::shared_ptr<IDatabase> {
        MySQLConfig* mysqlConfig = dynamic_cast<MySQLConfig*>(config);
        if (mysqlConfig == nullptr) {
            return std::shared_ptr<IDatabase>(nullptr);
        }
        return std::make_shared<MySQLDatabase>(*mysqlConfig);
    });

    // 注册SQLite数据库驱动   
    registerDatabase(DBType::SQLITE, [](DBConfig* config) -> std::shared_ptr<IDatabase> {
        SQLiteConfig* sqliteConfig = dynamic_cast<SQLiteConfig*>(config);
        if (sqliteConfig == nullptr) {
            return std::shared_ptr<IDatabase>(nullptr);
        }
        return std::make_shared<SQLiteDatabase>(*sqliteConfig);
    });
}

std::shared_ptr<DatabaseFactory> DatabaseFactory::getInstance() {
    if (_instance == nullptr) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (_instance == nullptr) {
            _instance = std::shared_ptr<DatabaseFactory>(new DatabaseFactory());
            INF("DatabaseFactory initialized with MySQL and SQLite drivers");
        }
    }
    return _instance;
}

void DatabaseFactory::registerDatabase(DBType type, std::function<std::shared_ptr<IDatabase>(DBConfig*)> creator) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // 1. 检查数据库类型是否已注册
    if (_creators.find(type) != _creators.end()) {
        ERR("DatabaseFactory: database type already registered");
        return;
    }

    // 2. 注册数据库类型
    _creators[type] = creator;
    std::string typeName = (type == DBType::MYSQL) ? "MySQL" : "SQLite";
    INF("Registered database driver: {}", typeName);
}

std::shared_ptr<IDatabase> DatabaseFactory::createDatabase(DBConfig* config) {
    // 1. 检查配置是否为空
    if (config == nullptr) {
        ERR("DatabaseFactory: config is nullptr");
        return nullptr;
    }
    
    // 2. 获取数据库的创建器
    DBType type = config->getType();
    auto it = _creators.find(type);
    if (it == _creators.end()) {
        std::string typeName = (type == DBType::MYSQL) ? "MySQL" : "SQLite";
        ERR("DatabaseFactory: unsupported database type: {}", typeName);
        return nullptr;
    }

    // 3. 检查配置是否有效
    if (!config->validConfig()) {
        std::string typeName = (type == DBType::MYSQL) ? "MySQL" : "SQLite";
        ERR("DatabaseFactory: invalid config for {}", typeName);
        return nullptr;
    }
    
    // 4. 创建数据库实例
    auto database = it->second(config);
    std::string typeName = (type == DBType::MYSQL) ? "MySQL" : "SQLite";
    INF("DatabaseFactory: created {} database instance", typeName);
    return database;
}

std::vector<DBType> DatabaseFactory::getSupportedTypes() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::vector<DBType> types;
    for (const auto& pair : _creators) {
        types.push_back(pair.first);
    }
    return types;
}

bool DatabaseFactory::isSupported(DBType type) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _creators.find(type) != _creators.end();
}

} // namespace databaseService
