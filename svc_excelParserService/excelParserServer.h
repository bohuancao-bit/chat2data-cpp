#pragma once

#include <string>
#include <vector>
#include <memory>
#include <brpc/server.h>
#include "excelParserServiceImpl.h"

namespace bitesvc {
class SvcProvider;
}

namespace excelParserService {

// 注册中心配置结构
struct RegisterCenterConfig {
    std::string _etcdAddr;       // etcd注册中心地址
    std::string _serviceName;    // 服务名称
    std::string _serviceAddr;    // 服务地址
};

// FastDFS配置结构
struct FastDFSConfig {
    std::vector<std::string> _trackers;   // FastDFS tracker服务器地址列表
    int _connectTimeout;                  // 连接超时时间
    int _networkTimeout;                  // 网络超时时间
};

// RPC服务器类
class ExcelParserServer {
public:
    explicit ExcelParserServer(std::shared_ptr<ExcelParserServiceImpl> serviceImpl,
                                std::shared_ptr<brpc::Server> server,
                                std::shared_ptr<bitesvc::SvcProvider> serviceProvider);
    ~ExcelParserServer();

    // 启动RPC服务器
    void start();

private:
    std::shared_ptr<brpc::Server> _server;                      // brpc服务器实例指针
    std::shared_ptr<ExcelParserServiceImpl> _serviceImpl;       // 接口层实例指针
    std::shared_ptr<bitesvc::SvcProvider> _serviceProvider;     // 服务注册层实例指针
};

// RPC服务器构建器类
class ExcelParserServerBuilder {
public:
    ~ExcelParserServerBuilder();

    // 设置RPC服务器端口
    ExcelParserServerBuilder& setPort(int port);
    // 设置注册中心配置
    ExcelParserServerBuilder& setRegisterCenterConfig(const RegisterCenterConfig& registerCenterConfig);
    // 设置FastDFS配置
    ExcelParserServerBuilder& setFastDFSConfig(const FastDFSConfig& fastDFSConfig);
    // 构建RPC服务器实例
    std::shared_ptr<ExcelParserServer> build();

private:
    int _port;                                              // RPC服务器端口
    RegisterCenterConfig _registerCenterConfig;              // 注册中心配置
    FastDFSConfig _fastDFSConfig;                           // FastDFS配置
};

} // namespace excelParserService