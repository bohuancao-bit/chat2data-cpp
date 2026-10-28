#include "excelParserServer.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/etcd.h>
#include <bite_scaffold/fdfs.h>

namespace excelParserService {

ExcelParserServer::ExcelParserServer(std::shared_ptr<ExcelParserServiceImpl> serviceImpl,
                                     std::shared_ptr<brpc::Server> server,
                                     std::shared_ptr<bitesvc::SvcProvider> serviceProvider)
    : _serviceImpl(serviceImpl)
    , _server(server)
    , _serviceProvider(serviceProvider) {
    INF("ExcelParserServer initialized");
}

ExcelParserServer::~ExcelParserServer() {
    INF("ExcelParserServer destroyed");
}

void ExcelParserServer::start() {
    _server->RunUntilAskedToQuit();
}

ExcelParserServerBuilder::~ExcelParserServerBuilder() {
    INF("ExcelParserServerBuilder destroyed");
}

ExcelParserServerBuilder& ExcelParserServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

ExcelParserServerBuilder& ExcelParserServerBuilder::setRegisterCenterConfig(
    const RegisterCenterConfig& registerCenterConfig) {
    _registerCenterConfig = registerCenterConfig;
    return *this;
}

ExcelParserServerBuilder& ExcelParserServerBuilder::setFastDFSConfig(const FastDFSConfig& fastDFSConfig) {
    _fastDFSConfig = fastDFSConfig;
    return *this;
}

std::shared_ptr<ExcelParserServer> ExcelParserServerBuilder::build() {
    // 1. 初始化FastDFS客户端
    bitefdfs::fdfs_settings fdfsSettings;
    fdfsSettings.trackers = _fastDFSConfig._trackers;
    fdfsSettings.connect_timeout = _fastDFSConfig._connectTimeout;
    fdfsSettings.network_timeout = _fastDFSConfig._networkTimeout;
    bitefdfs::FDFSClient::init(fdfsSettings);
    INF("FastDFS client initialized");

    // 2. 创建业务层实例
    auto business = std::make_shared<ExcelParserBusiness>();
    // 3. 创建接口层实例
    auto serviceImpl = std::make_shared<ExcelParserServiceImpl>(business);
    // 4. 创建RPC服务器实例
    auto rpcServer = biterpc::RpcServerFactory::create(_port, serviceImpl.get());
    if (!rpcServer) {
        ERR("Failed to create RPC server");
        return nullptr;
    }
    // 5. 创建服务注册层实例，并注册服务
    auto provider = std::make_shared<bitesvc::SvcProvider>(
        _registerCenterConfig._etcdAddr,
        _registerCenterConfig._serviceName,
        _registerCenterConfig._serviceAddr);

    if (!provider->registry()) {
        ERR("Failed to registry service: {}", _registerCenterConfig._serviceName);
        return nullptr;
    }
    // 6. 创建RPC服务器实例
    auto server = std::make_shared<ExcelParserServer>(serviceImpl, rpcServer, provider); 
    return server;
}

} // namespace excelParserService