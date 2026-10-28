#include <signal.h>
#include <iostream>
#include <gflags/gflags.h>
#include <bite_scaffold/log.h>
#include "notifyServer.h"

// 配置文件路径
DEFINE_string(conf, "chat2Data.conf", "配置文件路径");

// 通知子服务配置
DEFINE_int32(listen_port, 0, "通知服务RPC端口");
DEFINE_string(service_name, "", "通知子服务名称");
DEFINE_string(service_addr, "", "通知子服务地址");

// ETCD配置
DEFINE_string(etcd_addr, "", "ETCD地址");

// 邮箱配置
DEFINE_string(mail_username, "", "邮箱用户名");
DEFINE_string(mail_password, "", "邮箱密码");
DEFINE_string(mail_url, "", "邮箱服务器地址");
DEFINE_string(mail_from, "", "发件人邮箱");

// 日志配置
DEFINE_bool(log_async, false, "是否启用异步日志");
DEFINE_int32(log_level, 2, "日志输出等级: 1-debug;2-info;3-warn;4-error;6-off");
DEFINE_string(log_format, "", "日志输出格式");
DEFINE_string(log_path, "", "日志输出路径");

// 信号处理函数
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

    // 4. 构建邮箱配置对象
    notifyService::mail_settings mailConfig;
    mailConfig._username = FLAGS_mail_username;
    mailConfig._password = FLAGS_mail_password;
    mailConfig._url = FLAGS_mail_url;
    mailConfig._from = FLAGS_mail_from;

    // 5. 构建注册中心配置对象
    notifyService::registerCenterConfig registerCenterConfig;
    registerCenterConfig._etcdAddr = FLAGS_etcd_addr;
    registerCenterConfig._serviceName = FLAGS_service_name;
    registerCenterConfig._serviceAddr = FLAGS_service_addr;

    // 6. 构建rpc服务器对象，以及其他实例的创建
    auto rpcServer = notifyService::NotifyServerBuilder()
        .setPort(FLAGS_listen_port)
        .setMailConfig(mailConfig)
        .setRegisterCenterConfig(registerCenterConfig)
        .build();

    if (!rpcServer) {
        ERR("Failed to build NotifyServer");
        return -1;
    }

    INF("serviceName : {}, serviceAddr: {}, port: {}", FLAGS_service_name, FLAGS_service_addr, FLAGS_listen_port);

    // 7. 启动rpc服务器
    rpcServer->start();
    INF("NotifyService is running on port {}", FLAGS_listen_port);
    return 0;
}