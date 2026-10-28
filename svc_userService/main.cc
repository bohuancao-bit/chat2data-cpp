#include <signal.h>
#include <iostream>
#include <thread>
#include <gflags/gflags.h>
#include <bite_scaffold/log.h>
#include "userServerBuilder.h"

// 配置文件路径
DEFINE_string(conf, "chat2Data.conf", "配置文件路径");

// RPC服务器配置
DEFINE_int32(listen_port, 0, "用户服务RPC端口");
DEFINE_string(service_name, "", "用户子服务名称");
DEFINE_string(service_addr, "", "用户子服务地址");

// 其他子服务名称配置（用于RPC调用）
DEFINE_string(notify_service, "", "通知子服务名称");
DEFINE_string(db_service, "", "数据库子服务名称");

// ETCD配置
DEFINE_string(etcd_addr, "", "ETCD地址");

// MySQL配置
DEFINE_string(mysql_host, "", "MySQL主机地址");
DEFINE_string(mysql_user, "", "MySQL用户名");
DEFINE_string(mysql_passwd, "", "MySQL密码");
DEFINE_string(mysql_db, "", "MySQL数据库名称");
DEFINE_int32(mysql_port, 0, "MySQL端口号");
DEFINE_string(mysql_charset, "", "MySQL字符集");

// Redis配置
DEFINE_string(redis_host, "", "Redis主机地址");
DEFINE_int32(redis_port, 0, "Redis端口号");
DEFINE_string(redis_passwd, "", "Redis密码");
DEFINE_int32(redis_db, 0, "Redis数据库索引");
DEFINE_int32(redis_pool_size, 0, "Redis连接池大小");

// 日志配置
DEFINE_bool(log_async, false, "是否启用异步日志");
DEFINE_int32(log_level, 2, "日志输出等级: 1-debug;2-info;3-warn;4-error;6-off");
DEFINE_string(log_format, "", "日志输出格式");
DEFINE_string(log_path, "", "日志输出路径");

void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    // 1. 解析命令行参数
    google::ParseCommandLineFlags(&argc, &argv, true);
    // 设置配置文件路径，从命令行参数获取
    gflags::SetCommandLineOption("flagfile", FLAGS_conf.c_str());

    // 2. 初始化日志
    bitelog::log_settings logSettings;
    logSettings.async = FLAGS_log_async;
    logSettings.level = FLAGS_log_level;
    logSettings.format = FLAGS_log_format;
    logSettings.path = FLAGS_log_path;
    bitelog::bitelog_init(logSettings);
    INF("Bitelog initialized");

    // 3. 注册信号处理函数
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    INF("Signal handlers registered");

    // 4. 配置MySQL
    biteodb::mysql_settings mysqlConfig;
    mysqlConfig.host = FLAGS_mysql_host;
    mysqlConfig.user = FLAGS_mysql_user;
    mysqlConfig.passwd = FLAGS_mysql_passwd;
    mysqlConfig.db = FLAGS_mysql_db;
    mysqlConfig.port = FLAGS_mysql_port;
    mysqlConfig.cset = FLAGS_mysql_charset;

    // 5. 配置Redis
    biteredis::redis_settings redisConfig;
    redisConfig.host = FLAGS_redis_host;
    redisConfig.port = FLAGS_redis_port;
    redisConfig.passwd = FLAGS_redis_passwd;
    redisConfig.db = FLAGS_redis_db;
    redisConfig.connection_pool_size = FLAGS_redis_pool_size;

    // 6. 配置要监听的服务名称
    std::vector<std::string> watchServices;
    if (!FLAGS_notify_service.empty()) {
        watchServices.push_back(FLAGS_notify_service);
    }
    if (!FLAGS_db_service.empty()) {
        watchServices.push_back(FLAGS_db_service);
    }

    // 7. 构建RPC服务器以及其他配置
    auto rpcServer = userService::UserServerBuilder()
        .setMysqlConfig(mysqlConfig)
        .setRedisConfig(redisConfig)
        .setPort(FLAGS_listen_port)
        .setEtcdAddr(FLAGS_etcd_addr)
        .setRegisterCenterConfig({FLAGS_etcd_addr, FLAGS_service_name, FLAGS_service_addr})
        .setWatchServices(watchServices)
        .build();

    INF("serviceName : {}, serviceAddr: {}, port: {}", FLAGS_service_name, FLAGS_service_addr, FLAGS_listen_port);
    if (!rpcServer) {
        ERR("Failed to build UserServer");
        return -1;
    }

    // 8. 启动RPC服务器
    rpcServer->start();
    INF("UserServer is running on port {}", FLAGS_listen_port);
    return 0;
}