#include <thread>
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/odb.h>
#include <bite_scaffold/redis.h>
#include <ai_chat_sdk/ChatSDK.h>
#include "aiServer.h"

namespace aiService {

AIServiceServer::AIServiceServer(std::shared_ptr<AIServiceImpl> serviceImpl,
                                std::shared_ptr<brpc::Server> server,
                                std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher,
                                std::shared_ptr<bitesvc::SvcProvider> serviceProvider)
    : _serviceImpl(serviceImpl)
    , _server(server)
    , _serviceWatcher(serviceWatcher)
    , _serviceProvider(serviceProvider) {
}

AIServiceServer::~AIServiceServer() {
    INF("AIServiceServer destroyed");
}

void AIServiceServer::start() {
    if (_server) {
        _server->RunUntilAskedToQuit();
        INF("AIServiceServer started on port");
    }
}

AIServiceBuilder::~AIServiceBuilder() {
    INF("AIServiceBuilder destroyed");
}

AIServiceBuilder& AIServiceBuilder::setMysqlConfig(const MysqlConfig& mysqlConfig) {
    _mysqlConfig = mysqlConfig;
    return *this;
}

AIServiceBuilder& AIServiceBuilder::setRedisConfig(const RedisConfig& redisConfig) {
    _redisConfig = redisConfig;
    return *this;
}

AIServiceBuilder& AIServiceBuilder::setPort(int port) {
    _port = port;
    return *this;
}

AIServiceBuilder& AIServiceBuilder::setEtcdAddr(const std::string& etcdAddr) {
    _registerCenterConfig._etcdAddr = etcdAddr;
    return *this;
}

AIServiceBuilder& AIServiceBuilder::setRegisterCenterConfig(const RegisterCenterConfig& registerCenterConfig) {
    _registerCenterConfig = registerCenterConfig;
    return *this;
}

AIServiceBuilder& AIServiceBuilder::setChatSDKConfig(const std::vector<std::shared_ptr<ai_chat_sdk::Config>>& modelConfigs) {
    _modelConfigs = modelConfigs;
    return *this;
}

AIServiceBuilder& AIServiceBuilder::setWatchServices(const std::vector<std::string>& serviceNames) {
    _watchServices = serviceNames;
    return *this;
}

std::shared_ptr<AIServiceServer> AIServiceBuilder::builder() {
    // 1. 初始化ChatSDK
    auto chatSdk = std::make_shared<ai_chat_sdk::ChatSDK>();
    if (!chatSdk->initModels(_modelConfigs)) {
        ERR("Failed to initialize ChatSDK models");
        return nullptr;
    }
    INF("ChatSDK initialized with {} models", _modelConfigs.size());

    // 2. 实例化channel管理器，并设置服务监控名称
    _svcChannels = std::make_shared<biterpc::SvcChannels>();
    for (const auto& serviceName : _watchServices) {
        _svcChannels->setWatch(serviceName);
        INF("Set watch for service: {}", serviceName);
    }

    // 3. 设置数据库配置并连接数据库
    biteodb::mysql_settings mysqlSettings;
    mysqlSettings.host = _mysqlConfig._host;
    mysqlSettings.user = _mysqlConfig._user;
    mysqlSettings.passwd = _mysqlConfig._passwd;
    mysqlSettings.db = _mysqlConfig._db;
    mysqlSettings.port = _mysqlConfig._port;
    mysqlSettings.cset = _mysqlConfig._cset;
    std::shared_ptr<odb::database> mysql = biteodb::DBFactory::mysql(mysqlSettings);

    // 4. 设置Redis配置并连接Redis
    biteredis::redis_settings redisSettings;
    redisSettings.host = _redisConfig._host;
    redisSettings.port = _redisConfig._port;
    redisSettings.passwd = _redisConfig._passwd;
    redisSettings.db = _redisConfig._db;
    redisSettings.connection_pool_size = _redisConfig._connectionPoolSize;
    std::shared_ptr<sw::redis::Redis> redis = biteredis::RedisFactory::create(redisSettings);

    // 5. 实例化数据层对象
    std::shared_ptr<ChatSessionData> chatSessionData = std::make_shared<ChatSessionData>(mysql, redis);

    // 6. 实例化会话管理器
    std::shared_ptr<ChatSessionMgr> chatSessionMgr = std::make_shared<ChatSessionMgr>(chatSessionData);

    // 7. 实例化业务层对象
    std::shared_ptr<AIBusiness> aiBusiness = std::make_shared<AIBusiness>(
        chatSdk,
        chatSessionMgr,
        _svcChannels
    );

    // 8. 实例化接口层对象
    std::shared_ptr<AIServiceImpl> serviceImpl = std::make_shared<AIServiceImpl>(aiBusiness);

    // 9. 实例化RPC服务器
    auto rpcServer = biterpc::RpcServerFactory::create(_port, serviceImpl.get());
    if (!rpcServer) {
        ERR("Failed to create RPC server");
        return nullptr;
    }

    // 10. 实例化服务监控回调函数
    auto onlineCallback = [this](const std::string& serviceName, const std::string& serviceAddr) {
        INF("Service online: {} at {}", serviceName, serviceAddr);
        _svcChannels->addNode(serviceName, serviceAddr);
    };

    auto offlineCallback = [this](const std::string& serviceName, const std::string& serviceAddr) {
        INF("Service offline: {} at {}", serviceName, serviceAddr);
        _svcChannels->delNode(serviceName, serviceAddr);
    };

    // 11. 实例化服务监控对象，并开启服务监控
    INF("Watch etcd addr: {}", _registerCenterConfig._etcdAddr);
    std::shared_ptr<bitesvc::SvcWatcher> watcher = std::make_shared<bitesvc::SvcWatcher>(
        _registerCenterConfig._etcdAddr, onlineCallback, offlineCallback);

    std::thread watcherThread([watcher]() {
        watcher->watch();
    });
    watcherThread.detach();

    // 12. 实例化服务注册对象，并注册服务
    INF("Register etcd addr: {}", _registerCenterConfig._etcdAddr);
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr,
        _registerCenterConfig._serviceName,
        _registerCenterConfig._serviceAddr);

    if (!provider->registry()) {
        ERR("Failed to registry service: {}", _registerCenterConfig._serviceName);
        return nullptr;
    }

    // 13. 实例化RPC服务器
    _server = std::make_shared<AIServiceServer>(serviceImpl, rpcServer, watcher, provider);

    // 14. 返回RPC服务器实例指针
    INF("AIServiceServer built successfully");
    return _server;
}

} // namespace aiService
