#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <brpc/server.h>
#include "userServiceImpl.h"

namespace bitesvc {
class SvcWatcher;
class SvcProvider;
}

namespace userService {

class UserServer {
public:
    explicit UserServer(std::shared_ptr<UserServiceImpl> serviceImpl, 
                        std::shared_ptr<brpc::Server> server, 
                        std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher,
                        std::shared_ptr<bitesvc::SvcProvider> serviceProvider);
    ~UserServer();
    void start();
private:
    std::shared_ptr<brpc::Server> _server;                  // RPC服务器
    std::shared_ptr<UserServiceImpl> _serviceImpl;          // RPC接口定义实例
    std::shared_ptr<bitesvc::SvcWatcher> _serviceWatcher;   // 服务发现对象
    std::shared_ptr<bitesvc::SvcProvider> _serviceProvider; // 服务注册对象
};

} // namespace userService