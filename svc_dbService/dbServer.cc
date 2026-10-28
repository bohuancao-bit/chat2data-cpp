#include <thread>
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/odb.h>
#include "dbServer.h"
#include "dbConnMgr.h"
#include "dbBusiness.h"
#include "dbDriver/databaseFactory.h"
#include "dbDriver/databaseSchema.h"
#include <bite_scaffold/fdfs.h>

namespace databaseService {

DBServer::DBServer(std::shared_ptr<DBServiceImpl> serviceImpl,
                  std::shared_ptr<brpc::Server> server,
                  std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher,
                  std::shared_ptr<bitesvc::SvcProvider> serviceProvider)
    : _serviceImpl(serviceImpl)
    , _server(server)
    , _serviceWatcher(serviceWatcher)
    , _serviceProvider(serviceProvider) {
}

DBServer::~DBServer() {
    INF("DBServer destroyed");
}

void DBServer::start() {
    if (_server) {
        _server->RunUntilAskedToQuit();
        INF("DBServer started on port");
    }
}

DBServerBuilder::~DBServerBuilder() {
    INF("DBServerBuilder destroyed");
}

DBServerBuilder& DBServerBuilder::setMysqlConfig(const MysqlConfig& mysqlConfig) {
    _mysqlConfig = mysqlConfig;
    return *this;
}

DBServerBuilder& DBServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

DBServerBuilder& DBServerBuilder::setEtcdAddr(const std::string& etcdAddr) {
    _registerCenterConfig._etcdAddr = etcdAddr;
    return *this;
}

DBServerBuilder& DBServerBuilder::setRegisterCenterConfig(const RegisterCenterConfig& registerCenterConfig) {
    _registerCenterConfig = registerCenterConfig;
    return *this;
}

DBServerBuilder& DBServerBuilder::setFastDFSConfig(const FastDFSConfig& fastDFSConfig) {
    _fastDFSConfig = fastDFSConfig;
    return *this;
}

DBServerBuilder& DBServerBuilder::setWatchServices(const std::vector<std::string>& serviceNames) {
    _watchServices = serviceNames;
    return *this;
}

std::shared_ptr<DBServer> DBServerBuilder::build() {
    // 1. 初始化FastDFS客户端
    bitefdfs::fdfs_settings fdfsSettings;
    fdfsSettings.trackers = _fastDFSConfig._trackers;
    fdfsSettings.connect_timeout = _fastDFSConfig._connectTimeout;
    fdfsSettings.network_timeout = _fastDFSConfig._networkTimeout;
    bitefdfs::FDFSClient::init(fdfsSettings);
    INF("FastDFS client initialized");

    // 2. 初始化信道管理器
    _svcChannels = std::make_shared<biterpc::SvcChannels>();
    for (const auto& serviceName : _watchServices) {
        _svcChannels->setWatch(serviceName);
        INF("Set watch for service: {}", serviceName);
    }

    auto connMgr = std::make_shared<DBConnMgr>();

    // 3. 初始化MySQL配置
    MySQLConfig mysqlConf;
    mysqlConf.host = _mysqlConfig._host;
    mysqlConf.port = _mysqlConfig._port;
    mysqlConf.username = _mysqlConfig._user;
    mysqlConf.password = _mysqlConfig._passwd;
    mysqlConf.database = _mysqlConfig._db;
    mysqlConf.charset = _mysqlConfig._cset;

    // 4. 初始化DBBusiness（默认连接在DBBusiness构造函数中创建）
    auto dbBusiness = std::make_shared<DBBusiness>(connMgr, _svcChannels, mysqlConf);

    // 5. 初始化DBServiceImpl
    auto serviceImpl = std::make_shared<DBServiceImpl>(dbBusiness);

    // 6. 初始化RPC服务器
    auto rpcServer = biterpc::RpcServerFactory::create(_port, serviceImpl.get());
    if (!rpcServer) {
        ERR("Failed to create RPC server");
        return nullptr;
    }

    // 7. 设置服务监控的回调函数
    auto onlineCallback = [this](const std::string& serviceName, const std::string& serviceAddr) {
        INF("Service online: {} at {}", serviceName, serviceAddr);
        _svcChannels->addNode(serviceName, serviceAddr);
    };

    auto offlineCallback = [this](const std::string& serviceName, const std::string& serviceAddr) {
        INF("Service offline: {} at {}", serviceName, serviceAddr);
        _svcChannels->delNode(serviceName, serviceAddr);
    };

    // 8. 创建服务监控对象，并开始监控
    INF("Watch etcd addr: {}", _registerCenterConfig._etcdAddr);
    auto watcher = std::make_shared<bitesvc::SvcWatcher>(
        _registerCenterConfig._etcdAddr, onlineCallback, offlineCallback);

    std::thread watcherThread([watcher]() {
        watcher->watch();
    });
    watcherThread.detach();

    // 9. 创建服务注册对象，并注册服务
    INF("Register etcd addr: {}", _registerCenterConfig._etcdAddr);
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr,
        _registerCenterConfig._serviceName,
        _registerCenterConfig._serviceAddr);

    if (!provider->registry()) {
        ERR("Failed to registry service: {}", _registerCenterConfig._serviceName);
        return nullptr;
    }

    // 10. 创建DBServer
    auto server = std::make_shared<DBServer>(serviceImpl, rpcServer, watcher, provider);

    INF("DBServer built successfully");
    return server;
}

} // namespace databaseService