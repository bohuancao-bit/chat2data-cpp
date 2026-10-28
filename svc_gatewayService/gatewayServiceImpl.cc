#include <ctime>
#include <chrono>
#include <sstream>
#include <unistd.h>
#include <limits.h>
#include <bite_scaffold/log.h>
#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/reader.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/util.h>
#include "gatewayServiceImpl.h"
#include "../proto/protoCode/userService.pb.h"
#include "../proto/protoCode/fileService.pb.h"
#include "../proto/protoCode/dbService.pb.h"
#include "../proto/protoCode/aiService.pb.h"

namespace GatewayService {

GatewayServiceImpl::GatewayServiceImpl(std::shared_ptr<biterpc::SvcChannels> svcChannels, std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher)
    : _svcChannels(svcChannels), _serviceWatcher(serviceWatcher) {
    char result[PATH_MAX];
    // 获取当前可执行程序的路径
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        _exePath = std::string(result, count);
        size_t lastSlash = _exePath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            _exePath = _exePath.substr(0, lastSlash);
        }
        INF("Executable path: {}", _exePath);
        _exePath += "/www";
        INF("Executable path: {}, Static resources path: {}", result, _exePath);
    } else {
        _exePath = "/www";
        WRN("Failed to get executable path, using default static resources path: {}", _exePath);
    }
    INF("GatewayServiceImpl initialized");
}

GatewayServiceImpl::~GatewayServiceImpl() {
    INF("GatewayServiceImpl destroyed");
}

void GatewayServiceImpl::bindRoutes(httplib::Server& server) {
    INF("Binding HTTP routes...");

    // 设置静态资源的路径（基于可执行程序路径）
    INF("Setting static resources path to {}", _exePath);
    server.set_mount_point("/", _exePath);// ./build/www--此处是相当于工作目录的路径

    // 路由绑定流程说明：
    // 网关共实现30个HTTP接口，分为5大类：
    // 1. 健康检测接口（1个）：用于检查网关服务是否正常运行
    // 2. 用户子服务接口（9个）：处理用户注册、登录、信息查询等
    // 3. 文件子服务接口（9个）：处理文件上传、下载、预览等
    // 4. 存储子服务接口（5个）：处理数据库连接、数据查询等
    // 5. AI子服务接口（6个）：处理聊天会话、模型交互等
    //
    // 每个路由使用Lambda表达式捕获this指针，将请求转发到对应的处理函数
    // 处理函数内部会调用SvcChannels获取后端服务的Channel，发起RPC请求

    // ==================== 健康检测接口 ====================
    server.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealthCheck(req, res);
    });
    
    // ==================== 用户子服务接口 ====================
    server.Post("/api/user/valid/nickname", [this](const httplib::Request& req, httplib::Response& res) {
        handleValidNickname(req, res);
    });
    
    server.Post("/api/user/valid/email", [this](const httplib::Request& req, httplib::Response& res) {
        handleValidEmail(req, res);
    });
    
    server.Post("/api/user/register", [this](const httplib::Request& req, httplib::Response& res) {
        handleUserRegister(req, res);
    });
    
    server.Post("/api/user/passwd/login", [this](const httplib::Request& req, httplib::Response& res) {
        handlePasswordLogin(req, res);
    });
    
    server.Post("/api/user/code", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetVerifyCode(req, res);
    });
    
    server.Post("/api/user/vcode/login", [this](const httplib::Request& req, httplib::Response& res) {
        handleVerifyCodeLogin(req, res);
    });
    
    server.Post("/api/user/session/login", [this](const httplib::Request& req, httplib::Response& res) {
        handleSessionLogin(req, res);
    });
    
    server.Post("/api/user/logout", [this](const httplib::Request& req, httplib::Response& res) {
        handleLogout(req, res);
    });
    
    server.Get("/api/user/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetUserInfo(req, res);
    });
    
    // ==================== 文件子服务接口 ====================
    server.Post("/api/file/upload/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileUploadInfo(req, res);
    });
    
    server.Get("/api/file/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetFileInfo(req, res);
    });
    
    server.Post("/api/file/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileUpload(req, res);
    });
    
    server.Get("/api/file/download", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileDownload(req, res);
    });
    
    server.Delete(R"(/api/file/([\w-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileDelete(req, res);
    });
    
    server.Post("/api/file/preview", [this](const httplib::Request& req, httplib::Response& res) {
        handleFilePreview(req, res);
    });
    
    server.Post("/api/file/list", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetFileList(req, res);
    });
    
    server.Post("/api/file/chat/map", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileChatMap(req, res);
    });
    
    server.Post("/api/file/sqlite/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleSqliteUpload(req, res);
    });
    
    // ==================== 存储子服务接口 ====================
    server.Post("/api/db/connect", [this](const httplib::Request& req, httplib::Response& res) {
        handleDbConnect(req, res);
    });
    
    server.Post("/api/db/disconnect", [this](const httplib::Request& req, httplib::Response& res) {
        handleDbDisconnect(req, res);
    });
    
    server.Get("/api/db/tables", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetDbTables(req, res);
    });
    
    server.Post("/api/db/table/data", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetTableData(req, res);
    });
    
    server.Post("/api/db/connection/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetConnectionStatus(req, res);
    });
    
    // ==================== AI子服务接口 ====================
    server.Post("/api/ai/models", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetModels(req, res);
    });
    
    server.Post("/api/ai/session/create", [this](const httplib::Request& req, httplib::Response& res) {
        handleCreateChatSession(req, res);
    });
    
    server.Post("/api/ai/chatSessionLists", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetChatSessionLists(req, res);
    });
    
    server.Post("/api/ai/history", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetHistory(req, res);
    });
    
    server.Post("/api/ai/sendStreamMessage", [this](const httplib::Request& req, httplib::Response& res) {
        handleAiChat(req, res);
    });
    
    server.Post("/api/ai/delete", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteChatSession(req, res);
    });
    
    INF("HTTP routes bound successfully, total 30 routes");
}

void GatewayServiceImpl::sendJsonResponse(httplib::Response& res, const std::string& jsonData, int statusCode) {
    res.set_content(jsonData, "application/json");
    res.status = statusCode;
}

void GatewayServiceImpl::sendErrorResponse(httplib::Response& res, 
                                          const std::string& requestId, 
                                          int errorCode, 
                                          const std::string& errorMsg, 
                                          int statusCode) {
    Json::Value response;
    response["requestId"] = requestId;
    response["errorCode"] = errorCode;
    response["errorMsg"] = errorMsg;
    
    Json::StreamWriterBuilder writer;
    std::string jsonResponse = Json::writeString(writer, response);
    
    sendJsonResponse(res, jsonResponse, statusCode);
}

// 检测会话是否有效，在文件子服务中是通过获取文件信息，检测用户是否登录
// 即鉴权操作就是检测用户是否登录
bool GatewayServiceImpl::validateSession(const std::string& requestId,
                                        const std::string& sessionId,
                                        httplib::Response& res,
                                        std::string& userId) {
    // 1. 获取用户子服务的rpc通信信道
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return false;
    }

    // 2. 构建rpc请求
    chat2Data::userService::IsSessionValidRequest validRequest;
    validRequest.set_request_id(requestId);
    validRequest.set_session_id(sessionId);

    // 3. 创建用户子服务的rpc客户端
    chat2Data::userService::IsSessionValidResponse validResponse;
    brpc::Controller validController;
    chat2Data::userService::UserService_Stub stub(channel.get());

    // 4. 发起检测会话是否有效的rpc调用
    stub.IsSessionValid(&validController, &validRequest, &validResponse, nullptr);

    // 5. 检查rpc调用是否成功
    if (validController.Failed()) {
        ERR("IsSessionValid RPC failed: {}", validController.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return false;
    }
    if (validResponse.error_code() != 0) {
        sendErrorResponse(res, validResponse.request_id(), validResponse.error_code(), validResponse.error_msg());
        return false;
    }
    if (!validResponse.is_valid()) {
        sendErrorResponse(res, requestId, 401, "Session is invalid or expired");
        return false;
    }

    // 6. 通过引用参数带出userId
    userId = validResponse.user_id();

    // 7. 返回成功响应
    return true;
}

// ==================== 健康检测接口实现 ====================

void GatewayServiceImpl::handleHealthCheck(const httplib::Request& req, httplib::Response& res) {
    INF("Handling health check request");
    
    // 1. 生成当前时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    // 2. 构建响应
    Json::Value response;
    response["status"] = "healthy";
    response["service"] = "GatewayService";
    response["timestamp"] = static_cast<Json::Int64>(timestamp);
    
    // 3. 序列化响应为JSON字符串
    Json::StreamWriterBuilder writer;
    std::string jsonResponse = Json::writeString(writer, response);
    
    // 4. 发送JSON响应
    sendJsonResponse(res, jsonResponse, 200);
    INF("Health check response sent successfully");
}

// ==================== 用户子服务接口实现 ====================

void GatewayServiceImpl::handleValidNickname(const httplib::Request& req, httplib::Response& res) {
    INF("Handling valid nickname request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    Json::Value requestJson = jsonOpt.value();

    // 2. 从JSON对象中提取请求参数
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string nickname = requestJson.get("nickname", "").asString();

    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 3.2 构建rpc请求
    chat2Data::userService::ValidNicknameRequest request;
    request.set_request_id(requestId);
    request.set_nickname(nickname);

    // 3.3 创建rpc的客户端
    chat2Data::userService::ValidNicknameResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());

    // 3.4 发起rpc调用
    stub.ValidNickname(&controller, &request, &response, nullptr);

    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("ValidNickname RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Valid nickname request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleValidEmail(const httplib::Request& req, httplib::Response& res) {
    INF("Handling valid email request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    Json::Value requestJson = jsonOpt.value();

    // 2. 从JSON对象中提取请求参数
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string email = requestJson.get("email", "").asString();

    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 3.2 构建rpc请求
    chat2Data::userService::ValidEmailRequest request;
    request.set_request_id(requestId);
    request.set_email(email);
    
    // 3.3 创建rpc的客户端
    chat2Data::userService::ValidEmailResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    
    // 3.4 发起rpc调用
    stub.ValidEmail(&controller, &request, &response, nullptr);
    
    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("ValidEmail RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }
    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Valid email request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleUserRegister(const httplib::Request& req, httplib::Response& res) {
    INF("Handling user register request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    
    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string nickname = requestJson.get("nickname", "").asString();
    std::string password = requestJson.get("password", "").asString();
    std::string email = requestJson.get("email", "").asString();

    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 3.2 构建rpc请求
    chat2Data::userService::UserRegisterRequest request;
    request.set_request_id(requestId);
    request.set_nickname(nickname);
    request.set_password(password);
    request.set_email(email);
    
    // 3.3 创建rpc的客户端
    chat2Data::userService::UserRegisterResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    
    // 3.4 发起rpc调用
    stub.UserRegister(&controller, &request, &response, nullptr);

    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("UserRegister RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("User register request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handlePasswordLogin(const httplib::Request& req, httplib::Response& res) {
    INF("Handling password login request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    
    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string username = requestJson.get("username", "").asString();
    std::string password = requestJson.get("password", "").asString();
    
    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 3.2 构建rpc请求
    chat2Data::userService::PasswdLoginRequest request;
    request.set_request_id(requestId);
    request.set_username(username);
    request.set_password(password);
    
    // 3.3 创建rpc的客户端
    chat2Data::userService::PasswdLoginResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());

    // 3.4 发起rpc调用
    stub.PasswdLogin(&controller, &request, &response, nullptr);
    
    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("PasswdLogin RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }
    
    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["sessionId"] = response.result().session_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Password login request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetVerifyCode(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get verify code request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    
    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string email = requestJson.get("email", "").asString();

    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 3.2 构建rpc请求
    chat2Data::userService::GetCodeRequest request;
    request.set_request_id(requestId);
    request.set_email(email);

    // 3.3 创建rpc的客户端
    chat2Data::userService::GetCodeResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    
    // 3.4 发起rpc调用
    stub.GetCode(&controller, &request, &response, nullptr);
    
    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetCode RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["codeId"] = response.result().code_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get verify code request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleVerifyCodeLogin(const httplib::Request& req, httplib::Response& res) {
    INF("Handling verify code login request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    
    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string email = requestJson.get("email", "").asString();
    std::string verifyCode = requestJson.get("verifyCode", "").asString();
    std::string codeId = requestJson.get("codeId", "").asString();
    
    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 3.2 构建rpc请求
    chat2Data::userService::VcodeLoginRequest request;
    request.set_request_id(requestId);
    request.set_email(email);
    request.set_verify_code(verifyCode);
    request.set_code_id(codeId);
    
    // 3.3 创建rpc的客户端
    chat2Data::userService::VcodeLoginResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());
    
    // 3.4 发起rpc调用
    stub.VcodeLogin(&controller, &request, &response, nullptr);
    
    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("VcodeLogin RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }
    
    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["sessionId"] = response.result().session_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Verify code login request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleSessionLogin(const httplib::Request& req, httplib::Response& res) {
    INF("Handling session login request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    
    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    
    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 3.2 构建rpc请求
    chat2Data::userService::SessionLoginRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    
    // 3.3 创建rpc的客户端
    chat2Data::userService::SessionLoginResponse response;
    brpc::Controller controller;
    chat2Data::userService::UserService_Stub stub(channel.get());

    // 3.4 发起rpc调用
    stub.SessionLogin(&controller, &request, &response, nullptr);

    // 3.5 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("SessionLogin RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 4 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Session login request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleLogout(const httplib::Request& req, httplib::Response& res) {
    INF("Handling logout request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }
    
    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();

    // 3. 发送rpc调用
    // 3.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 3.2 构建rpc请求
    chat2Data::userService::IsSessionValidRequest validRequest;
    validRequest.set_request_id(requestId);
    validRequest.set_session_id(sessionId);

    // 3.3 创建rpc的客户端
    chat2Data::userService::IsSessionValidResponse validResponse;
    brpc::Controller validController;
    chat2Data::userService::UserService_Stub stub(channel.get());
    // 3.4 鉴权
    stub.IsSessionValid(&validController, &validRequest, &validResponse, nullptr);
    if (validController.Failed()) {
        ERR("IsSessionValid RPC failed: {}", validController.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (validResponse.error_code() != 0) {
        sendErrorResponse(res, validResponse.request_id(), validResponse.error_code(), validResponse.error_msg());
        return;
    }
    if (!validResponse.is_valid()) {
        sendErrorResponse(res, requestId, 401, "Session is invalid or expired");
        return;
    }
    
    // 3.5 构建rpc请求
    chat2Data::userService::LogoutRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    
    // 3.6 发送rpc调用
    chat2Data::userService::LogoutResponse response;
    brpc::Controller controller;
    stub.Logout(&controller, &request, &response, nullptr);

    // 3.7 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("Logout RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 4. 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Logout request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetUserInfo(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get user info request");
    // 1 从url中提取请求参数
    if (!req.has_param("requestId") || !req.has_param("sessionId")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }
    
    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");
    INF("RequestId: {}, SessionId: {}", requestId, sessionId);

    // 2 发送rpc调用
    // 2.1 获取UserService服务的channel
    auto channel = _svcChannels->getNode(FLAGS_user_service);
    if (!channel) {
        ERR("Failed to get UserService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    
    // 2.2 构建rpc请求
    chat2Data::userService::IsSessionValidRequest validRequest;
    validRequest.set_request_id(requestId);
    validRequest.set_session_id(sessionId);
    
    // 2.3 创建rpc的客户端
    chat2Data::userService::IsSessionValidResponse validResponse;
    brpc::Controller validController;
    chat2Data::userService::UserService_Stub stub(channel.get());

    // 2.4 鉴权
    stub.IsSessionValid(&validController, &validRequest, &validResponse, nullptr);
    
    // 2.5 检测rpc调用是否成功
    if (validController.Failed()) {
        ERR("IsSessionValid RPC failed: {}", validController.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (validResponse.error_code() != 0) {
        sendErrorResponse(res, validResponse.request_id(), validResponse.error_code(), validResponse.error_msg());
        return;
    }
    if (!validResponse.is_valid()) {
        sendErrorResponse(res, requestId, 401, "Session is invalid or expired");
        return;
    }
    
    // 2.6 构建rpc请求
    chat2Data::userService::GetUserInfoRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);

    // 2.7 发送rpc调用
    chat2Data::userService::GetUserInfoResponse response;
    brpc::Controller controller;
    stub.GetUserInfo(&controller, &request, &response, nullptr);
    
    // 2.8 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetUserInfo RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 4. 根据rpc响应构建HTTP响应，序列化成功之后将结果返回给前端
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        Json::Value userInfoJson;
        userInfoJson["userId"] = response.result().user_info().user_id();
        userInfoJson["nickname"] = response.result().user_info().nickname();
        userInfoJson["email"] = response.result().user_info().email();
        resultJson["userInfo"] = userInfoJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get user info request handled successfully, requestId: {}", requestId);
}

// ==================== 文件子服务接口实现 ====================

void GatewayServiceImpl::handleFileUploadInfo(const httplib::Request& req, httplib::Response& res) {
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::fileService::UploadFileInfoRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_user_id(userId);

    if (requestJson.isMember("fileInfo")) {
        auto& fileInfoJson = requestJson["fileInfo"];
        chat2Data::fileService::FileInfo* fileInfo = request.mutable_file_info();
        fileInfo->set_filename(fileInfoJson.get("filename", "").asString());
        fileInfo->set_file_size(fileInfoJson.get("fileSize", 0).asInt64());
        fileInfo->set_file_ext(fileInfoJson.get("fileExt", "").asString());
    }

    // 6. 发起rpc调用
    chat2Data::fileService::UploadFileInfoResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.UploadFileInfo(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("UploadFileInfo RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = response.result().file_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("File upload info request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetFileInfo(const httplib::Request& req, httplib::Response& res) {
    // 1. 提取请求参数
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("fileId")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }

    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");
    std::string fileId = req.get_param_value("fileId");

    // 2. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 4. 构建rpc请求
    chat2Data::fileService::GetFileInfoRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_file_id(fileId);
    request.set_user_id(userId);

    // 5. 发起rpc调用
    chat2Data::fileService::GetFileInfoResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.GetFileInfo(&controller, &request, &response, nullptr);

    // 6. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetFileInfo RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 7. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = response.result().file_id();
        resultJson["fileName"] = response.result().file_name();
        resultJson["fileSize"] = static_cast<Json::Int64>(response.result().file_size());
        resultJson["uploadTime"] = static_cast<Json::Int64>(response.result().upload_time());
        resultJson["fileExt"] = response.result().file_ext();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get file info request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleFileUpload(const httplib::Request& req, httplib::Response& res) {
    // 1. 提取请求参数
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("fileId")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }

    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");
    std::string fileId = req.get_param_value("fileId");

    // 2. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 4. 构建rpc请求
    chat2Data::fileService::UploadFileRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_file_id(fileId);
    request.set_user_id(userId);

    // 5. 发起rpc调用，文件数据通过attachment传输
    chat2Data::fileService::UploadFileResponse response;
    brpc::Controller controller;
    controller.request_attachment().append(req.body);
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.UploadFile(&controller, &request, &response, nullptr);

    // 6. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("UploadFile RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 7. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("File upload request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleFileDownload(const httplib::Request& req, httplib::Response& res) {
    // 1. 提取请求参数
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("fileId")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }

    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");
    std::string fileId = req.get_param_value("fileId");

    // 2. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 4. 构建rpc请求
    chat2Data::fileService::DownloadFileRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_file_id(fileId);
    request.set_user_id(userId);

    // 5. 发起rpc调用，文件数据通过attachment返回
    chat2Data::fileService::DownloadFileResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.DownloadFile(&controller, &request, &response, nullptr);

    // 6. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("DownloadFile RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 7. 从attachment中获取文件数据并返回给HTTP客户端
    std::string fileData = controller.response_attachment().to_string();
    res.set_content(fileData, "application/octet-stream");
    INF("File download request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleFileDelete(const httplib::Request& req, httplib::Response& res) {
    // 1. 提取请求参数
    if (!req.has_param("requestId") || !req.has_param("sessionId")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }

    std::string fileId = req.matches[1];
    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");

    // 2. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 4. 构建rpc请求
    chat2Data::fileService::DeleteFileRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_file_id(fileId);
    request.set_user_id(userId);

    // 5. 发起rpc调用
    chat2Data::fileService::DeleteFileResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.DeleteFile(&controller, &request, &response, nullptr);

    // 6. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("DeleteFile RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 7. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("File delete request handled successfully, requestId: {}, fileId: {}", requestId, fileId);
}

void GatewayServiceImpl::handleFilePreview(const httplib::Request& req, httplib::Response& res) {
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string fileId = requestJson.get("fileId", "").asString();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::fileService::PreviewExcelRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_file_id(fileId);
    request.set_user_id(userId);

    if (requestJson.isMember("pageNumber")) {
        request.set_page_number(requestJson["pageNumber"].asInt());
    }
    if (requestJson.isMember("pageSize")) {
        request.set_page_size(requestJson["pageSize"].asInt());
    }

    // 6. 发起rpc调用
    chat2Data::fileService::PreviewExcelResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.PreviewExcel(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("PreviewExcel RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = response.result().file_id();
        resultJson["fileName"] = response.result().file_name();
        resultJson["fileSize"] = static_cast<Json::Int64>(response.result().file_size());
        resultJson["fileExt"] = response.result().file_ext();

        Json::Value sheetsJson;
        for (int i = 0; i < response.result().excel_data().sheets_size(); ++i) {
            const auto& sheet = response.result().excel_data().sheets(i);
            Json::Value sheetJson;
            sheetJson["name"] = sheet.name();
            sheetJson["totalRows"] = sheet.total_rows();
            sheetJson["colCount"] = sheet.col_count();
            sheetJson["currentPage"] = sheet.current_page();
            sheetJson["totalPages"] = sheet.total_pages();
            sheetJson["pageSize"] = sheet.page_size();

            Json::Value columnsJson;
            for (int k = 0; k < sheet.columns_size(); ++k) {
                columnsJson.append(sheet.columns(k));
            }
            sheetJson["columns"] = columnsJson;

            Json::Value dataJson;
            for (int j = 0; j < sheet.data_size(); ++j) {
                const auto& row = sheet.data(j);
                Json::Value rowJson;
                for (int k = 0; k < row.cells_size(); ++k) {
                    rowJson.append(row.cells(k));
                }
                dataJson.append(rowJson);
            }
            sheetJson["data"] = dataJson;
            sheetsJson.append(sheetJson);
        }
        resultJson["excelData"]["sheets"] = sheetsJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("File preview request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetFileList(const httplib::Request& req, httplib::Response& res) {
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::fileService::GetFileListRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_user_id(userId);

    // 6. 发起rpc调用
    chat2Data::fileService::GetFileListResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.GetFileList(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetFileList RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value fileListJson;
        for (int i = 0; i < response.result().file_list_size(); ++i) {
            const auto& fileItem = response.result().file_list(i);
            Json::Value itemJson;
            itemJson["fileId"] = fileItem.file_id();
            itemJson["fileName"] = fileItem.file_name();
            itemJson["fileSize"] = static_cast<Json::Int64>(fileItem.file_size());
            itemJson["uploadTime"] = static_cast<Json::Int64>(fileItem.upload_time());
            itemJson["chatSessionId"] = fileItem.chat_session_id();
            fileListJson.append(itemJson);
        }
        responseJson["result"]["fileList"] = fileListJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get file list request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleFileChatMap(const httplib::Request& req, httplib::Response& res) {
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string fileId = requestJson.get("fileId", "").asString();
    std::string chatSessionId = requestJson.get("chatSessionId", "").asString();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::fileService::HandleFileChatSessionMapRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_file_id(fileId);
    request.set_chat_session_id(chatSessionId);
    request.set_user_id(userId);

    // 6. 发起rpc调用
    chat2Data::fileService::HandleFileChatSessionMapResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.HandleFileChatSessionMap(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("HandleFileChatSessionMap RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("File chat map request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleSqliteUpload(const httplib::Request& req, httplib::Response& res) {
    // 1. 提取请求参数
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("filename")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }

    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");
    std::string filename = req.get_param_value("filename");

    // 2. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 3. 获取FileService服务的channel
    auto fileChannel = _svcChannels->getNode(FLAGS_file_service);
    if (!fileChannel) {
        ERR("Failed to get FileService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 4. 构建rpc请求
    chat2Data::fileService::UploadSQLiteFileRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_filename(filename);
    request.set_user_id(userId);

    // 5. 发起rpc调用，文件数据通过attachment传输
    chat2Data::fileService::UploadSQLiteFileResponse response;
    brpc::Controller controller;
    controller.request_attachment().append(req.body);
    chat2Data::fileService::FileService_Stub fileStub(fileChannel.get());
    fileStub.UploadSQLiteFile(&controller, &request, &response, nullptr);

    // 6. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("UploadSQLiteFile RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 7. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["fileId"] = response.result().file_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Sqlite upload request handled successfully, requestId: {}", requestId);
}

// ==================== 存储子服务接口实现 ====================
void GatewayServiceImpl::handleDbConnect(const httplib::Request& req, httplib::Response& res) {
    INF("Handling db connect request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取DatabaseService服务的channel
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("Failed to get DatabaseService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::DatabaseService::ConnectDatabaseRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_user_id(userId);

    if (requestJson.isMember("database")) {
        auto& dbJson = requestJson["database"];
        auto* database = request.mutable_database();

        if (dbJson.isMember("type")) {
            std::string typeStr = dbJson["type"].asString();
            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
            if (typeStr == "MYSQL") {
                database->set_type(chat2Data::DatabaseService::DATABASE_TYPE_MYSQL);
            } else if (typeStr == "SQLITE") {
                database->set_type(chat2Data::DatabaseService::DATABASE_TYPE_SQLITE);
            } else {
                database->set_type(chat2Data::DatabaseService::DATABASE_TYPE_UNKNOWN);
            }
        }

        if (dbJson.isMember("MySQL") && dbJson["MySQL"].isObject()) {
            auto& mysqlJson = dbJson["MySQL"];
            auto* mysqlConfig = database->mutable_mysql_config();
            mysqlConfig->set_host(mysqlJson.get("host", "").asString());
            mysqlConfig->set_port(mysqlJson.get("port", 0).asInt());
            mysqlConfig->set_name(mysqlJson.get("name", "").asString());
            mysqlConfig->set_username(mysqlJson.get("username", "").asString());
            mysqlConfig->set_password(mysqlJson.get("password", "").asString());
            mysqlConfig->set_charset(mysqlJson.get("charset", "utf8mb4").asString());
        }

        if (dbJson.isMember("SQLite") && dbJson["SQLite"].isObject()) {
            auto& sqliteJson = dbJson["SQLite"];
            auto* sqliteConfig = database->mutable_sqlite_config();
            sqliteConfig->set_file_id(sqliteJson.get("fileId", "").asString());
            sqliteConfig->set_readonly(sqliteJson.get("readonly", false).asBool());
        }
    }

    // 6. 创建rpc客户端
    chat2Data::DatabaseService::ConnectDatabaseResponse response;
    brpc::Controller controller;
    chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());

    // 7. 发起rpc调用
    dbStub.ConnectDatabase(&controller, &request, &response, nullptr);

    // 8. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("ConnectDatabase RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 9. 解析rpc响应，组织HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["connectionId"] = response.result().connection_id();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Db connect request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleDbDisconnect(const httplib::Request& req, httplib::Response& res) {
    INF("Handling db disconnect request");
    
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string connectionId = requestJson.get("connectionId", "").asString();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取DatabaseService服务的channel
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("Failed to get DatabaseService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::DatabaseService::DisconnectDatabaseRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_connection_id(connectionId);

    // 6. 创建rpc客户端
    chat2Data::DatabaseService::DisconnectDatabaseResponse response;
    brpc::Controller controller;
    chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());

    // 7. 发起rpc调用
    dbStub.DisconnectDatabase(&controller, &request, &response, nullptr);

    // 8. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("DisconnectDatabase RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 9. 解析rpc响应，组织HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Db disconnect request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetDbTables(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get db tables request");

    // 1. 检测请求参数是否完整
    if (!req.has_param("requestId") || !req.has_param("sessionId") || !req.has_param("dbConnectId")) {
        ERR("Missing required parameters");
        sendErrorResponse(res, "", 400, "Missing required parameters");
        return;
    }

    // 2. 提取请求参数
    std::string requestId = req.get_param_value("requestId");
    std::string sessionId = req.get_param_value("sessionId");
    std::string dbConnectId = req.get_param_value("dbConnectId");

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取DatabaseService服务的channel
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("Failed to get DatabaseService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::DatabaseService::ListTablesRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_db_connect_id(dbConnectId);

    // 6. 创建rpc客户端
    chat2Data::DatabaseService::ListTablesResponse response;
    brpc::Controller controller;
    chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());

    // 7. 发起rpc调用
    dbStub.ListTables(&controller, &request, &response, nullptr);

    // 8. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("ListTables RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 9. 解析rpc响应，组织HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        Json::Value tablesJson;
        for (int i = 0; i < response.result().tables_size(); ++i) {
            tablesJson.append(response.result().tables(i));
        }
        resultJson["tables"] = tablesJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get db tables request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetTableData(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get table data request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string dbConnectId = requestJson.get("dbConnectId", "").asString();
    std::string tableName = requestJson.get("tableName", "").asString();
    bool forceOriginal = requestJson.get("forceOriginal", false).asBool();
    int pageNumber = requestJson.get("pageNumber", 1).asInt();
    int pageSize = requestJson.get("pageSize", 50).asInt();

    // 3. 鉴权操作，通过validateSession方法检测会话是否有效，同时通过引用参数获取userId
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取DatabaseService服务的channel
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("Failed to get DatabaseService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::DatabaseService::GetTableDataRequest request;
    request.set_request_id(requestId);
    request.set_session_id(sessionId);
    request.set_db_connect_id(dbConnectId);
    request.set_table_name(tableName);
    request.set_force_original(forceOriginal);
    request.set_page_number(pageNumber);
    request.set_page_size(pageSize);

    // 6. 创建rpc客户端
    chat2Data::DatabaseService::GetTableDataResponse response;
    brpc::Controller controller;
    chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());

    // 7. 发起rpc调用
    dbStub.GetTableData(&controller, &request, &response, nullptr);

    // 8. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetTableData RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 9. 解析rpc响应，组织HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        if (response.result().has_table_schema()) {
            Json::Value schemaJson;
            const auto& schema = response.result().table_schema();

            Json::Value columnInfoJson;
            for (int i = 0; i < schema.column_info_size(); ++i) {
                Json::Value colJson;
                colJson["name"] = schema.column_info(i).name();
                colJson["type"] = schema.column_info(i).type();
                columnInfoJson.append(colJson);
            }
            schemaJson["columnInfo"] = columnInfoJson;

            if (schema.has_table_data()) {
                Json::Value tableDataJson;
                tableDataJson["totalRows"] = schema.table_data().total_rows();
                tableDataJson["currentPage"] = schema.table_data().current_page();
                tableDataJson["totalPages"] = schema.table_data().total_pages();
                tableDataJson["pageSize"] = schema.table_data().page_size();

                Json::Value rowsJson;
                for (int i = 0; i < schema.table_data().rows_size(); ++i) {
                    Json::Value rowJson;
                    for (int j = 0; j < schema.table_data().rows(i).cells_size(); ++j) {
                        rowJson.append(schema.table_data().rows(i).cells(j));
                    }
                    rowsJson.append(rowJson);
                }
                tableDataJson["rows"] = rowsJson;
                schemaJson["tableData"] = tableDataJson;
            }
            resultJson["tableSchema"] = schemaJson;
        }
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get table data request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetConnectionStatus(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get connection status request");
    // 1. 将请求体反序列化为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string dbConnectId = requestJson.get("dbConnectId", "").asString();
    
    // 3. 获取DatabaseService服务的channel
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("Failed to get DatabaseService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 4. 构建rpc请求
    chat2Data::DatabaseService::GetConnTempTablesRequest request;
    request.set_request_id(requestId);
    request.set_db_connect_id(dbConnectId);
    
    // 5. 创建rpc客户端
    chat2Data::DatabaseService::GetConnTempTablesResponse response;
    brpc::Controller controller;
    chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());
    
    // 6. 发起rpc调用
    dbStub.GetConnTempTables(&controller, &request, &response, nullptr);
    
    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetConnTempTables RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }
    
    // 8. 解析rpc响应，组织HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    Json::Value resultJson;
    Json::Value tempTablesJson;
    for (int i = 0; i < response.temp_tables_size(); ++i) {
        tempTablesJson.append(response.temp_tables(i));
    }
    resultJson["tempTables"] = tempTablesJson;
    resultJson["hasTempTables"] = response.has_temp_tables();
    responseJson["result"] = resultJson;

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get connection status request handled successfully, requestId: {}", requestId);
}

// ==================== AI子服务接口实现 ====================

void GatewayServiceImpl::handleGetModels(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get models request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();

    // 3. 鉴权操作
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取AIService服务的channel
    auto aiChannel = _svcChannels->getNode(FLAGS_ai_service);
    if (!aiChannel) {
        ERR("Failed to get AIService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::AiService::GetModelsRequest request;
    request.set_request_id(requestId);

    // 6. 发起rpc调用
    chat2Data::AiService::GetModelsResponse response;
    brpc::Controller controller;
    chat2Data::AiService::AIService_Stub stub(aiChannel.get());
    stub.GetModels(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetModels RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        Json::Value modelListJson;
        for (int i = 0; i < response.result().models_size(); ++i) {
            Json::Value modelJson;
            modelJson["modelName"] = response.result().models(i).name();
            modelJson["modelDesc"] = response.result().models(i).desc();
            modelListJson.append(modelJson);
        }
        resultJson["modelList"] = modelListJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get models request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleCreateChatSession(const httplib::Request& req, httplib::Response& res) {
    INF("Handling create chat session request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string modelName = requestJson.get("modelName", "").asString();
    std::string sessionType = requestJson.get("sessionType", "").asString();
    std::string dbConnectionInfo = requestJson.get("dbConnectionInfo", "").asString();

    // 3. 鉴权操作
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取AIService服务的channel
    auto aiChannel = _svcChannels->getNode(FLAGS_ai_service);
    if (!aiChannel) {
        ERR("Failed to get AIService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::AiService::CreateChatSessionRequest request;
    request.set_request_id(requestId);
    request.set_user_id(userId);
    request.set_model(modelName);
    request.set_session_type(sessionType);
    request.set_db_connection_info(dbConnectionInfo);

    // 6. 发起rpc调用
    chat2Data::AiService::CreateChatSessionResponse response;
    brpc::Controller controller;
    chat2Data::AiService::AIService_Stub stub(aiChannel.get());
    stub.CreateSession(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("CreateSession RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        resultJson["chatSessionId"] = response.result().session().chat_session_id();
        resultJson["modelName"] = response.result().session().model();
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Create chat session request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetChatSessionLists(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get chat session lists request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();

    // 3. 鉴权操作
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取AIService服务的channel
    auto aiChannel = _svcChannels->getNode(FLAGS_ai_service);
    if (!aiChannel) {
        ERR("Failed to get AIService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::AiService::GetSessionsRequest request;
    request.set_request_id(requestId);
    request.set_user_id(userId);

    // 6. 发起rpc调用
    chat2Data::AiService::GetSessionsResponse response;
    brpc::Controller controller;
    chat2Data::AiService::AIService_Stub stub(aiChannel.get());
    stub.GetSessions(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetSessions RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        Json::Value chatSessionListsJson;
        for (int i = 0; i < response.result().sessioninfo_size(); ++i) {
            Json::Value sessionJson;
            sessionJson["chatSessionId"] = response.result().sessioninfo(i).id();
            sessionJson["modelName"] = response.result().sessioninfo(i).model();
            sessionJson["title"] = response.result().sessioninfo(i).title();
            sessionJson["createdAt"] = static_cast<Json::Int64>(response.result().sessioninfo(i).created_at());
            sessionJson["updatedAt"] = static_cast<Json::Int64>(response.result().sessioninfo(i).updated_at());
            sessionJson["messageCount"] = response.result().sessioninfo(i).message_count();
            sessionJson["firstUserMessageContent"] = response.result().sessioninfo(i).first_user_message_content();
            chatSessionListsJson.append(sessionJson);
        }
        resultJson["chatSessionLists"] = chatSessionListsJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get chat session lists request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleGetHistory(const httplib::Request& req, httplib::Response& res) {
    INF("Handling get history request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string chatSessionId = requestJson.get("chatSessionId", "").asString();

    // 3. 鉴权操作
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取AIService服务的channel
    auto aiChannel = _svcChannels->getNode(FLAGS_ai_service);
    if (!aiChannel) {
        ERR("Failed to get AIService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::AiService::GetSessionHistoryRequest request;
    request.set_request_id(requestId);
    request.set_user_id(userId);
    request.set_chat_session_id(chatSessionId);

    // 6. 发起rpc调用
    chat2Data::AiService::GetSessionHistoryResponse response;
    brpc::Controller controller;
    chat2Data::AiService::AIService_Stub stub(aiChannel.get());
    stub.GetSessionHistory(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("GetSessionHistory RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();
    if (response.has_result()) {
        Json::Value resultJson;
        if (!response.result().file_id().empty()) {
            resultJson["fileId"] = response.result().file_id();
        }
        if (!response.result().session_type().empty()) {
            resultJson["sessionType"] = response.result().session_type();
        }
        if (!response.result().db_connection_info().empty()) {
            resultJson["dbConnectionInfo"] = response.result().db_connection_info();
        }
        Json::Value messageListJson;
        for (int i = 0; i < response.result().messages_size(); ++i) {
            Json::Value msgJson;
            msgJson["id"] = response.result().messages(i).id();
            msgJson["role"] = response.result().messages(i).role();
            msgJson["content"] = response.result().messages(i).content();
            msgJson["timestamp"] = static_cast<Json::Int64>(response.result().messages(i).timestamp());
            messageListJson.append(msgJson);
        }
        resultJson["messageList"] = messageListJson;
        responseJson["result"] = resultJson;
    }

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Get history request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleAiChat(const httplib::Request& req, httplib::Response& res) {
    INF("Handling ai chat request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string chatSessionId = requestJson.get("chatSessionId", "").asString();
    std::string message = requestJson.get("message", "").asString();
    std::string chatType = requestJson.get("chatType", "").asString();
    std::string fileId = requestJson.get("fileId", "").asString();
    int dbType = requestJson.get("dbType", 0).asInt();
    std::string dbConnectId = requestJson.get("dbConnectId", "").asString();
    std::string tableName = requestJson.get("tableName", "").asString();

    // 3. 鉴权操作
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取AI子服务的服务器地址
    auto aiAddrOpt = _svcChannels->getNodeAddr(FLAGS_ai_service);
    if (!aiAddrOpt) {
        ERR("Failed to get AIService address");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    std::string aiAddr = aiAddrOpt.value();
    INF("AIService address: {}", aiAddr);

    // 5. 构建请求参数
    Json::Value aiRequestJson;
    aiRequestJson["request_id"] = requestId;
    aiRequestJson["session_id"] = sessionId;
    aiRequestJson["user_id"] = userId;
    aiRequestJson["chat_session_id"] = chatSessionId;
    aiRequestJson["chat_type"] = chatType;
    aiRequestJson["message"] = message;
    aiRequestJson["file_id"] = fileId;
    aiRequestJson["db_type"] = dbType;
    aiRequestJson["db_connect_id"] = dbConnectId;
    aiRequestJson["table_name"] = tableName;

    auto requestBodyOpt = biteutil::JSON::serialize(aiRequestJson);
    if (!requestBodyOpt) {
        ERR("Failed to serialize request body");
        sendErrorResponse(res, requestId, 500, "Internal server error");
        return;
    }
    std::string requestBody = requestBodyOpt.value();

    // 6. 设置SSE响应头，响应头中需开启流式传输，并设置SSE数据块处理回调
    res.status = 200;
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "*");

    // 7. 在回调函数中，构建HTTP客户端，向AI子服务发起发送消息请求
    res.set_chunked_content_provider("text/event-stream", [this, aiAddr, requestBody](size_t offset, httplib::DataSink& dataSink) -> bool {
        // 7.1 解析AI服务地址 "host:port"
        size_t colonPos = aiAddr.find(':');  // 127.0.0.1:9006
        if (colonPos == std::string::npos) {
            ERR("Invalid AIService address format: {}", aiAddr);
            std::string errorData = "data: [ERROR]\n\n";
            dataSink.write(errorData.c_str(), errorData.size());
            dataSink.done();
            return false;
        }
        std::string host = aiAddr.substr(0, colonPos);
        int port = std::stoi(aiAddr.substr(colonPos + 1));

        // 7.2 创建HTTP客户端
        httplib::Client client(host, port);

        // 7.3 设置HTTP请求头参数
        client.set_read_timeout(300, 0);  // 读超时时间300s
        client.set_write_timeout(300, 0);  // 写超时时间300s
        client.set_connection_timeout(60, 0);  // 连接超时时间60s

        // 7.4 构建HTTP请求
        httplib::Request req;
        req.method = "POST";
        req.path = "/chat2Data.AiService.AIService/SendMessage";
        req.headers = {
            {"Content-Type", "application/json"},
            {"Accept", "text/event-stream"}
        };
        req.body = requestBody;

        // 7.5 设置响应头处理器回调：检测是否成功建立连接
        bool connectionFailed = false;
        req.response_handler = [&](const httplib::Response& res) {
            if (res.status != 200) {
                connectionFailed = true;
                return false;     // 终止请求
            }
            return true;         // 继续接收后续响应数据(SSE)
        };

        // 7.6 设置内容接收器回调：将AI子服务返回的SSE数据块，主动推送给前端
        req.content_receiver = [&](const char* data, size_t len, size_t offset, size_t totalLength) {
            if (connectionFailed) {
                return false;
            }
            // 将AI子服务返回的SSE数据块直接写入响应流
            dataSink.write(data, len);
            return true;
        };

        // 7.7 发送HTTP请求
        auto httpResponse = client.send(req);

        // 7.8 检测是否成功建立连接
        if (!httpResponse) {
            ERR("HTTP request to AIService failed");
            std::string errorData = "data: [ERROR]\n\n";
            dataSink.write(errorData.c_str(), errorData.size());
            dataSink.done();
            return false;
        }

        // 7.9 发送结束标记
        std::string doneData = "data: [DONE]\n\n";
        dataSink.write(doneData.c_str(), doneData.size());
        dataSink.done();

        return false;
    });

    INF("Ai chat request handled successfully, requestId: {}", requestId);
}

void GatewayServiceImpl::handleDeleteChatSession(const httplib::Request& req, httplib::Response& res) {
    INF("Handling delete chat session request");
    // 1. 反序列化请求体为JSON对象
    auto jsonOpt = biteutil::JSON::unserialize(req.body);
    if (!jsonOpt) {
        ERR("Failed to parse request body");
        sendErrorResponse(res, "", 400, "Invalid request body");
        return;
    }

    // 2. 从JSON对象中提取请求参数
    Json::Value requestJson = jsonOpt.value();
    std::string requestId = requestJson.get("requestId", "").asString();
    std::string sessionId = requestJson.get("sessionId", "").asString();
    std::string chatSessionId = requestJson.get("chatSessionId", "").asString();

    // 3. 鉴权操作
    std::string userId;
    if (!validateSession(requestId, sessionId, res, userId)) {
        return;
    }

    // 4. 获取AIService服务的channel
    auto aiChannel = _svcChannels->getNode(FLAGS_ai_service);
    if (!aiChannel) {
        ERR("Failed to get AIService channel");
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }

    // 5. 构建rpc请求
    chat2Data::AiService::DeleteSessionRequest request;
    request.set_request_id(requestId);
    request.set_user_id(userId);
    request.set_chat_session_id(chatSessionId);

    // 6. 发起rpc调用
    chat2Data::AiService::DeleteSessionResponse response;
    brpc::Controller controller;
    chat2Data::AiService::AIService_Stub stub(aiChannel.get());
    stub.DeleteSession(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        ERR("DeleteSession RPC failed: {}", controller.ErrorText());
        sendErrorResponse(res, requestId, 503, "Service not available");
        return;
    }
    if (response.error_code() != 0) {
        sendErrorResponse(res, response.request_id(), response.error_code(), response.error_msg());
        return;
    }

    // 8. 构建HTTP响应
    Json::Value responseJson;
    responseJson["requestId"] = response.request_id();
    responseJson["errorCode"] = response.error_code();
    responseJson["errorMsg"] = response.error_msg();

    auto jsonStr = biteutil::JSON::serialize(responseJson);
    if (jsonStr) {
        sendJsonResponse(res, jsonStr.value());
    } else {
        sendErrorResponse(res, requestId, 500, "Internal server error");
    }
    INF("Delete chat session request handled successfully, requestId: {}", requestId);
}

}
