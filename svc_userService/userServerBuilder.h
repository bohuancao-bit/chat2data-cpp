#pragma once

#include <string>
#include <memory>
#include <vector>

#include <bite_scaffold/odb.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/redis.h>
#include <bite_scaffold/log.h>

#include "common.h"
#include "userServer.h"
#include "userBusiness.h"
#include "sessionManager.h"
#include "../data/userData.h"
#include "../data/verifyCodeData.h"
#include "../data/sessionData.h"

namespace userService {

struct registerCenterConfig{
    std::string _etcdAddr;       // etcd地址
    std::string _serviceName;    // 服务名称
    std::string _serviceAddr;    // 服务地址
};
        
class UserServerBuilder {
public:
    ~UserServerBuilder() {
        INF("UserServerBuilder destroyed");
    }

    // 设置MySQL配置
    UserServerBuilder& setMysqlConfig(const biteodb::mysql_settings& mysqlConfig);
    // 设置Redis配置
    UserServerBuilder& setRedisConfig(const biteredis::redis_settings& redisConfig);
    // 设置RPC服务器端口号
    UserServerBuilder& setPort(int port);
    // 设置etcd地址
    UserServerBuilder& setEtcdAddr(const std::string& etcdAddr);
    // 设置注册中心配置
    UserServerBuilder& setRegisterCenterConfig(const registerCenterConfig& registerCenterConfig);
    // 设置要监听的服务名称
    UserServerBuilder& setWatchServices(const std::vector<std::string>& serviceNames);
    // 构建UserServer
    std::shared_ptr<UserServer> build();

private:
    biteodb::mysql_settings _mysqlConfig;           // MySQL配置
    biteredis::redis_settings _redisConfig;         // Redis配置
    int _port;                                      // RPC服务器端口号
    registerCenterConfig _registerCenterConfig;     // 注册中心配置
    std::shared_ptr<UserServer> _server;            // UserServer实例
    std::string _etcdAddr;                          // etcd地址
    std::vector<std::string> _watchServices;        // 要监听的服务名称
    std::shared_ptr<biterpc::SvcChannels> _svcChannels; // 服务通道
};

} // namespace userService