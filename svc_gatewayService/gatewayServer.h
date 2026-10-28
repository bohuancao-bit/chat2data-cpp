#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <httplib.h>
#include <bite_scaffold/rpc.h>
#include "gatewayServiceImpl.h"

namespace bitesvc {
class SvcWatcher;
}

namespace GatewayService {

class GatewayServerBuilder;

/**
 * @brief 网关服务器类
 *
 * 负责HTTP服务器的启动、停止、路由绑定等核心功能
 */
class GatewayServer {
public:
    /**
     * @brief 构造函数
     * @param host 网关监听地址
     * @param port 网关监听端口
     */
    GatewayServer(const std::string& host, int port);

    /**
     * @brief 析构函数
     */
    ~GatewayServer();

    /**
     * @brief 启动网关服务器
     * @return 启动成功返回true，否则返回false
     */
    bool start();

    /**
     * @brief 停止网关服务器
     */
    void stop();

    /**
     * @brief 检查服务器是否正在运行
     * @return 服务器运行状态
     */
    bool isRunning() const;

    /**
     * @brief 绑定路由，将HTTP请求路由到对应的处理函数
     */
    void bindRoutes();

    /**
     * @brief 设置HTTP服务器实例
     * @param server 唯一的httplib::Server智能指针
     */
    void setServer(std::unique_ptr<httplib::Server> server);

    /**
     * @brief 设置网关服务实现实例
     * @param serviceImpl 唯一的GatewayServiceImpl智能指针
     */
    void setServiceImpl(std::unique_ptr<GatewayServiceImpl> serviceImpl);

    friend class GatewayServerBuilder;

private:
    std::string _host;
    int _port;
    std::unique_ptr<httplib::Server> _server;
    std::unique_ptr<GatewayServiceImpl> _serviceImpl;
    std::atomic<bool> _isRunning;
    std::mutex _mutex;
};

/**
 * @brief 网关服务器构建器类
 *
 * 使用Builder模式构建网关服务器，配置服务器地址、ETCD服务发现等参数
 */
class GatewayServerBuilder {
public:
    ~GatewayServerBuilder(){
        INF("GatewayServerBuilder destroyed");
    }
    /**
     * @brief 设置网关服务器监听地址
     * @param host 监听地址
     * @return 构建器自身引用，用于链式调用
     */
    GatewayServerBuilder& setHost(const std::string& host);

    /**
     * @brief 设置网关服务器监听端口
     * @param port 监听端口
     * @return 构建器自身引用，用于链式调用
     */
    GatewayServerBuilder& setPort(int port);

    /**
     * @brief 设置ETCD注册中心地址
     * @param etcdAddr ETCD地址
     * @return 构建器自身引用，用于链式调用
     */
    GatewayServerBuilder& setEtcdAddr(const std::string& etcdAddr);

    /**
     * @brief 添加需要发现的服务名称
     * @param serviceName 服务名称
     * @return 构建器自身引用，用于链式调用
     */
    GatewayServerBuilder& addService(const std::string& serviceName);

    /**
     * @brief 构建并启动网关服务器
     * @return 共享指针指向构建的网关服务器实例
     */
    std::shared_ptr<GatewayServer> build();

private:
    /**
     * @brief 初始化服务发现组件
     *
     * 创建SvcChannels管理服务节点信道，创建SvcWatcher监听服务上下线事件
     */
    void initServiceDiscovery();

private:
    std::string _host;
    int _port;
    std::string _etcdAddr;
    std::vector<std::string> _serviceNames;
    std::shared_ptr<biterpc::SvcChannels> _svcChannels;
    std::shared_ptr<bitesvc::SvcWatcher> _serviceWatcher;
    std::shared_ptr<GatewayServer> _server;
};

}
