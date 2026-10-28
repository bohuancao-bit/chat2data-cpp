#pragma once

#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <brpc/server.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/etcd.h>
#include "fileServiceImpl.h"
#include "fileBusiness.h"
#include "common.h"
#include "../data/fileInfoData.h"
#include "../data/worksheetData.h"

namespace bitesvc {
class SvcWatcher;
class SvcProvider;
}

namespace fileService {

class FileServer {
public:
    explicit FileServer(std::shared_ptr<FileServiceImpl> serviceImpl,
                        std::shared_ptr<brpc::Server> server,
                        std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher,
                        std::shared_ptr<bitesvc::SvcProvider> serviceProvider);
    ~FileServer();
    // 启动RPC服务器
    void start();

private:
    std::shared_ptr<brpc::Server> _server;                   // RPC服务器实例指针
    std::shared_ptr<FileServiceImpl> _serviceImpl;           // 接口层实例指针
    std::shared_ptr<bitesvc::SvcWatcher> _serviceWatcher;     // 服务监听实例指针
    std::shared_ptr<bitesvc::SvcProvider> _serviceProvider;   // 服务注册实例指针
};

// 服务器构建器
class FileServerBuilder {
public:
    ~FileServerBuilder();

    // 设置数据库配置
    FileServerBuilder& setMysqlConfig(const MysqlConfig& mysqlConfig);
    // 设置Redis配置
    FileServerBuilder& setRedisConfig(const RedisConfig& redisConfig);
    // 设置RPC服务器端口
    FileServerBuilder& setPort(int port);
    // 设置ETCD地址
    FileServerBuilder& setEtcdAddr(const std::string& etcdAddr);
    // 设置服务注册中心配置
    FileServerBuilder& setRegisterCenterConfig(const RegisterCenterConfig& registerCenterConfig);
    // 设置FastDFS配置
    FileServerBuilder& setFastDFSConfig(const FastDFSConfig& fastDFSConfig);
    // 设置需要监听的服务名称
    FileServerBuilder& setWatchServices(const std::vector<std::string>& serviceNames);

    // 构建各个实例，并返回RPC服务器实例指针
    std::shared_ptr<FileServer> build();

private:
    MysqlConfig _mysqlConfig;                          // 数据库配置
    RedisConfig _redisConfig;                          // Redis配置
    FastDFSConfig _fastDFSConfig;                      // FastDFS配置
    int _port;                                         // RPC服务器端口
    RegisterCenterConfig _registerCenterConfig;        // 服务注册中心配置
    std::vector<std::string> _watchServices;          // 需要监听的服务名称
    std::shared_ptr<biterpc::SvcChannels> _svcChannels; // channel管理器实例指针
    std::shared_ptr<FileServer> _server;                 // RPC服务器实例指针
};

} // namespace fileService