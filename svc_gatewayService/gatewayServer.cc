#include <bite_scaffold/log.h>
#include <bite_scaffold/etcd.h>
#include <bite_scaffold/rpc.h>
#include "gatewayServer.h"

namespace GatewayService {

GatewayServer::GatewayServer(const std::string& host, int port)
    : _host(host)
    , _port(port)
    , _server(nullptr)
    , _isRunning(false) {
    INF("GatewayServer initialized with host: {}, port: {}", _host, _port);
}

GatewayServer::~GatewayServer() {
    if (_isRunning.load()) {
        stop();
    }
}

bool GatewayServer::start() {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_isRunning.load()) {
        WRN("GatewayServer is already running!");
        return false;
    }

    if (!_server) {
        ERR("GatewayServer http server is null!");
        return false;
    }

    std::thread serverThread([this]() {
        INF("GatewayServer starting on {}:{}", _host, _port);
        if (!_server->listen(_host, _port)) {
            ERR("GatewayServer failed to listen on {}:{}", _host, _port);
        }
    });

    serverThread.detach();

    _isRunning.store(true);
    INF("GatewayServer started successfully!");
    return true;
}

void GatewayServer::stop() {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_isRunning.load()) {
        WRN("GatewayServer is not running!");
        return;
    }

    if (_server) {
        _server->stop();
    }

    _isRunning.store(false);
    INF("GatewayServer stopped successfully!");
}

bool GatewayServer::isRunning() const {
    return _isRunning.load();
}

void GatewayServer::bindRoutes() {
    if (_serviceImpl && _server) {
        _serviceImpl->bindRoutes(*_server);
        INF("Routes bound to GatewayServer");
    }
}

void GatewayServer::setServer(std::unique_ptr<httplib::Server> server) {
    _server = std::move(server);
}

void GatewayServer::setServiceImpl(std::unique_ptr<GatewayServiceImpl> serviceImpl) {
    _serviceImpl = std::move(serviceImpl);
}

GatewayServerBuilder& GatewayServerBuilder::setHost(const std::string& host) {
    _host = host;
    return *this;
}

GatewayServerBuilder& GatewayServerBuilder::setPort(int port) {
    _port = port;
    return *this;
}

GatewayServerBuilder& GatewayServerBuilder::setEtcdAddr(const std::string& etcdAddr) {
    _etcdAddr = etcdAddr;
    return *this;
}

GatewayServerBuilder& GatewayServerBuilder::addService(const std::string& serviceName) {
    _serviceNames.push_back(serviceName);
    return *this;
}

void GatewayServerBuilder::initServiceDiscovery() {
    // 步骤1：为每个服务名称在SvcChannels中创建对应的节点管理集合
    // SvcChannels内部为每个服务维护一个 Channels 对象，用于管理该服务的所有可用节点
    for (const auto& serviceName : _serviceNames) {
        _svcChannels->setWatch(serviceName);
        INF("Set watch for service: {}", serviceName);
    }

    // 步骤2：定义服务上线回调函数
    // 当服务节点在ETCD中注册时，SvcWatcher会触发此回调
    // 回调中调用SvcChannels::addNode将新节点添加到对应服务的节点集合中
    auto onlineCallback = [this](const std::string& serviceName, const std::string& serviceAddr) {
        INF("Service online: {} at {}", serviceName, serviceAddr);
        _svcChannels->addNode(serviceName, serviceAddr);
    };

    // 步骤3：定义服务下线回调函数
    // 当服务节点从ETCD中注销时，SvcWatcher会触发此回调
    // 回调中调用SvcChannels::delNode将下线节点从对应服务的节点集合中移除
    auto offlineCallback = [this](const std::string& serviceName, const std::string& serviceAddr) {
        INF("Service offline: {} at {}", serviceName, serviceAddr);
        _svcChannels->delNode(serviceName, serviceAddr);
    };

    // 步骤4：创建SvcWatcher实例
    // SvcWatcher负责连接ETCD注册中心，监控服务节点的上下线事件
    _serviceWatcher = std::make_shared<bitesvc::SvcWatcher>(_etcdAddr, onlineCallback, offlineCallback);

    // 步骤5：启动独立的监控线程
    // watch()方法会阻塞当前线程，因此需要在新线程中运行
    // 新线程会持续监控ETCD，服务节点变化时触发回调函数
    std::thread watcherThread([this]() {
        _serviceWatcher->watch();
    });
    watcherThread.detach();
    
    INF("Service discovery watcher started");
}

std::shared_ptr<GatewayServer> GatewayServerBuilder::build() {
    // 0. 构建SvcChannels实例
    _svcChannels = std::make_shared<biterpc::SvcChannels>();

    // 步骤1：初始化服务发现组件
    // 创建SvcChannels管理所有服务的节点信道，创建SvcWatcher监听ETCD服务变化
    initServiceDiscovery();

    // 步骤2：创建网关服务实现实例
    // 将SvcChannels引用传递给GatewayServiceImpl，供其获取后端服务的Channel
    auto serviceImpl = std::make_unique<GatewayServiceImpl>(_svcChannels, _serviceWatcher);

    // 步骤3：创建网关服务器实例
    // 初始状态下_httpServer和_serviceImpl均为nullptr
    auto server = std::make_shared<GatewayServer>(_host, _port);

    // 步骤4：创建并配置HTTP服务器
    // 设置读写超时时间为5分钟，支持长连接和流式响应
    auto httpServer = std::make_unique<httplib::Server>();
    httpServer->set_read_timeout(std::chrono::minutes(5));
    httpServer->set_write_timeout(std::chrono::minutes(5));

    // 步骤5：注入HTTP服务器和服务实现到网关服务器
    server->setServer(std::move(httpServer));
    server->setServiceImpl(std::move(serviceImpl));

    // 步骤6：绑定路由
    // 将GatewayServiceImpl中的30个HTTP接口绑定到HTTP服务器
    server->bindRoutes();

    // 步骤7：启动服务器
    // 服务器会在独立线程中运行，监听指定地址和端口
    if (!server->start()) {
        ERR("Failed to start GatewayServer");
        return nullptr;
    }

    // 步骤8：保存服务器实例并返回
    _server = server;
    INF("GatewayServer built and started successfully");
    return server;
}

}
