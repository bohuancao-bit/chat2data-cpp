#include <signal.h>
#include <iostream>
#include <gflags/gflags.h>
#include <bite_scaffold/log.h>
#include "excelParserServer.h"

// 配置文件路径
DEFINE_string(conf, "chat2Data.conf", "配置文件路径");

// Excel解析子服务配置
DEFINE_int32(listen_port, 0, "Excel解析服务RPC端口");
DEFINE_string(service_name, "", "Excel解析子服务名称");
DEFINE_string(service_addr, "", "Excel解析子服务地址");

// ETCD配置
DEFINE_string(etcd_addr, "", "ETCD地址");

// FastDFS配置
DEFINE_string(fdfs_trackers, "", "FastDFS tracker服务器地址");
DEFINE_int32(fdfs_connect_timeout, 0, "FastDFS连接超时时间");
DEFINE_int32(fdfs_network_timeout, 0, "FastDFS网络超时时间");

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

    // 4. 设置注册中心配置
    excelParserService::RegisterCenterConfig registerCenterConfig;
    registerCenterConfig._etcdAddr = FLAGS_etcd_addr;
    registerCenterConfig._serviceName = FLAGS_service_name;
    registerCenterConfig._serviceAddr = FLAGS_service_addr;

    // 5. 设置FastDFS配置
    excelParserService::FastDFSConfig fastDFSConfig;
    fastDFSConfig._trackers = {FLAGS_fdfs_trackers};
    fastDFSConfig._connectTimeout = FLAGS_fdfs_connect_timeout;
    fastDFSConfig._networkTimeout = FLAGS_fdfs_network_timeout;

    // 6. 构建RPC服务器以及其他实例并返回rpc服务器
    auto rpcServer = excelParserService::ExcelParserServerBuilder()
        .setPort(FLAGS_listen_port)
        .setRegisterCenterConfig(registerCenterConfig)
        .setFastDFSConfig(fastDFSConfig)
        .build();

    if (!rpcServer) {
        ERR("Failed to build ExcelParserServer");
        return -1;
    }

    INF("serviceName : {}, serviceAddr: {}, port: {}", FLAGS_service_name, FLAGS_service_addr, FLAGS_listen_port);

    // 6. 启动RPC服务器，持续提供excel解析的rpc服务
    rpcServer->start();
    INF("ExcelParserService is running on port {}", FLAGS_listen_port);
    return 0;
}