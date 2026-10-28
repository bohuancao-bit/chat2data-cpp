#include <signal.h>
#include <iostream>
#include <gflags/gflags.h>
#include <bite_scaffold/log.h>
#include <ai_chat_sdk/ChatSDK.h>
#include <ai_chat_sdk/util/myLog.h>
#include "aiServer.h"
#include "common.h"

// 配置文件路径
DEFINE_string(conf, "chat2Data.conf", "配置文件路径");

// AI子服务配置
DEFINE_int32(listen_port, 0, "AI服务RPC端口");
DEFINE_string(service_name, "", "AI子服务名称");
DEFINE_string(service_addr, "", "AI子服务地址");

// 其他子服务名称配置（用于RPC调用）
DEFINE_string(file_service, "", "文件子服务名称");
DEFINE_string(db_service, "", "数据库子服务名称");
DEFINE_string(user_service, "", "用户子服务名称");
DEFINE_string(notify_service, "", "通知子服务名称");

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

// ChatSDK模型配置
DEFINE_double(temperature, 0.7, "温度值，影响生成文本的随机性");
DEFINE_int32(max_tokens, 2048, "最大token数");
// Ollama配置参数
DEFINE_string(ollama_model_name, "", "Ollama模型名称");
DEFINE_string(ollama_model_desc, "", "Ollama模型描述");
DEFINE_string(ollama_endpoint, "", "Ollama API地址");

// 日志器配置
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
    // 1. 解析gflags参数
    google::ParseCommandLineFlags(&argc, &argv, true);
    // 设置配置文件路径，从命令行参数获取
    gflags::SetCommandLineOption("flagfile", FLAGS_conf.c_str());

    // 2. 初始化日志器
    bitelog::log_settings logSettings;
    logSettings.async = FLAGS_log_async;
    logSettings.level = FLAGS_log_level;
    logSettings.format = FLAGS_log_format;
    logSettings.path = FLAGS_log_path;
    bitelog::bitelog_init(logSettings);
    bite::Logger::initLogger("aiService", FLAGS_log_path, (spdlog::level::level_enum)FLAGS_log_level);
    INF("Bitelog initialized");

    // 3. 注册信号处理函数
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    INF("Signal handlers registered");

    // 4. 初始化数据库配置
    aiService::MysqlConfig mysqlConfig;
    mysqlConfig._host = FLAGS_mysql_host;
    mysqlConfig._user = FLAGS_mysql_user;
    mysqlConfig._passwd = FLAGS_mysql_passwd;
    mysqlConfig._db = FLAGS_mysql_db;
    mysqlConfig._port = FLAGS_mysql_port;
    mysqlConfig._cset = FLAGS_mysql_charset;

    // 5. 初始化Redis配置
    aiService::RedisConfig redisConfig;
    redisConfig._host = FLAGS_redis_host;
    redisConfig._port = FLAGS_redis_port;
    redisConfig._passwd = FLAGS_redis_passwd;
    redisConfig._db = FLAGS_redis_db;
    redisConfig._connectionPoolSize = FLAGS_redis_pool_size;

    // 6. 初始化注册中心配置
    aiService::RegisterCenterConfig registerCenterConfig;
    registerCenterConfig._etcdAddr = FLAGS_etcd_addr;
    registerCenterConfig._serviceName = FLAGS_service_name;
    registerCenterConfig._serviceAddr = FLAGS_service_addr;

    // 7. 初始化ChatSDK模型配置
    std::vector<std::shared_ptr<ai_chat_sdk::Config>> modelConfigs;

    // DeepSeek模型
    auto deepseekConfig = std::make_shared<ai_chat_sdk::APIConfig>();
    deepseekConfig->_modelName = "deepseek-chat";
    deepseekConfig->_apiKey = std::getenv("deepseek_apikey");
    deepseekConfig->_temperature = FLAGS_temperature;
    deepseekConfig->_maxTokens = FLAGS_max_tokens;
    modelConfigs.push_back(deepseekConfig);

    // GPT模型
    auto gptConfig = std::make_shared<ai_chat_sdk::APIConfig>();
    gptConfig->_modelName = "gpt-4o-mini";
    gptConfig->_apiKey = std::getenv("chatgpt_apikey");
    gptConfig->_temperature = FLAGS_temperature;
    gptConfig->_maxTokens = FLAGS_max_tokens;
    modelConfigs.push_back(gptConfig);

    // Gemini模型
    auto geminiConfig = std::make_shared<ai_chat_sdk::APIConfig>();
    geminiConfig->_modelName = "gemini-2.0-flash";
    geminiConfig->_apiKey = std::getenv("gemini_apikey");
    geminiConfig->_temperature = FLAGS_temperature;
    geminiConfig->_maxTokens = FLAGS_max_tokens;
    modelConfigs.push_back(geminiConfig);

    // Ollama模型（本地部署，不需要API密钥）
    auto ollamaConfig = std::make_shared<ai_chat_sdk::OllamaConfig>();
    ollamaConfig->_modelName = FLAGS_ollama_model_name;
    ollamaConfig->_modelDesc = FLAGS_ollama_model_desc;
    ollamaConfig->_endpoint = FLAGS_ollama_endpoint;
    ollamaConfig->_temperature = FLAGS_temperature;
    ollamaConfig->_maxTokens = FLAGS_max_tokens;
    modelConfigs.push_back(ollamaConfig);

    // 8. 初始化要监控的服务名称
    std::vector<std::string> watchServices;
    watchServices.push_back(FLAGS_file_service);
    watchServices.push_back(FLAGS_db_service);
    watchServices.push_back(FLAGS_user_service);
    watchServices.push_back(FLAGS_notify_service);

    // 9. 构建各个实例，获取rpc服务器实例指针
    auto rpcServer = aiService::AIServiceBuilder()
        .setMysqlConfig(mysqlConfig)
        .setRedisConfig(redisConfig)
        .setPort(FLAGS_listen_port)
        .setEtcdAddr(FLAGS_etcd_addr)
        .setRegisterCenterConfig(registerCenterConfig)
        .setChatSDKConfig(modelConfigs)
        .setWatchServices(watchServices)
        .builder();

    if (!rpcServer) {
        ERR("Failed to build AIServiceServer");
        return -1;
    }

    INF("serviceName : {}, serviceAddr: {}, port: {}", FLAGS_service_name, FLAGS_service_addr, FLAGS_listen_port);

    // 10. 启动rpc服务器
    rpcServer->start();
    INF("AIServiceServer is running on port {}", FLAGS_listen_port);
    return 0;
}
