#pragma once

#include <string>
#include <memory>
#include <httplib.h>
#include <gflags/gflags.h>
#include <bite_scaffold/rpc.h>
#include <bite_scaffold/etcd.h>

// 声明gflags变量（在main.cc中定义）
DECLARE_string(user_service);
DECLARE_string(file_service);
DECLARE_string(db_service);
DECLARE_string(ai_service);

namespace GatewayService {

/**
 * @brief 网关HTTP接口实现类
 *
 * 该类负责HTTP接口的定义和路由绑定，与HTTP服务器搭建分离。
 * 包含30个HTTP接口，仅健康检测接口实现业务逻辑，其余接口只定义不实现。
 */
class GatewayServiceImpl {
public:
    /**
     * @brief 构造函数
     *
     * @param svcChannels 服务节点信道管理对象，用于获取RPC服务器Channel
     */
    explicit GatewayServiceImpl(std::shared_ptr<biterpc::SvcChannels> svcChannels, std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher);

    /**
     * @brief 析构函数
     */
    ~GatewayServiceImpl();

    /**
     * @brief 绑定路由到HTTP服务器
     *
     * @param server HTTP服务器实例
     *
     * @note 在网关服务器启动前调用，完成所有HTTP接口的路由绑定
     */
    void bindRoutes(httplib::Server& server);

private:
    /**
     * @brief 发送JSON响应
     * 
     * @param res HTTP响应对象
     * @param jsonData JSON数据字符串
     * @param statusCode HTTP状态码，默认200
     */
    void sendJsonResponse(httplib::Response& res, const std::string& jsonData, int statusCode = 200);
    
    /**
     * @brief 发送错误响应
     * 
     * @param res HTTP响应对象
     * @param requestId 请求ID
     * @param errorCode 错误码
     * @param errorMsg 错误描述
     * @param statusCode HTTP状态码，默认200
     */
    void sendErrorResponse(httplib::Response& res, 
                          const std::string& requestId, 
                          int errorCode, 
                          const std::string& errorMsg, 
                          int statusCode = 200);

    /**
     * @brief 验证会话是否有效
     * 
     * @param requestId 请求ID
     * @param sessionId 会话ID
     * @param res HTTP响应对象
     * @param userId 用户ID（通过引用参数带出）
     * @return 鉴权成功返回true，失败返回false
     *
     * @note 该方法封装了会话鉴权操作，可被文件子服务、数据库子服务、AI子服务复用
     */
    bool validateSession(const std::string& requestId,
                        const std::string& sessionId,
                        httplib::Response& res,
                        std::string& userId);

    // ==================== 健康检测接口 ====================
    
    /**
     * @brief 处理健康检测请求
     * 
     * @param req HTTP请求对象
     * @param res HTTP响应对象
     * 
     * @note 接口：GET /health
     * @note 响应：{"status": "healthy", "service": "GatewayService", "timestamp": 1706428800}
     */
    void handleHealthCheck(const httplib::Request& req, httplib::Response& res);

    // ==================== 用户子服务接口 ====================
    
    /**
     * @brief 检测用户昵称是否唯一
     * @note 接口：POST /api/user/valid/nickname
     */
    void handleValidNickname(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 检测用户邮箱是否唯一
     * @note 接口：POST /api/user/valid/email
     */
    void handleValidEmail(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 用户注册
     * @note 接口：POST /api/user/register
     */
    void handleUserRegister(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 密码登录
     * @note 接口：POST /api/user/passwd/login
     */
    void handlePasswordLogin(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取验证码
     * @note 接口：POST /api/user/code
     */
    void handleGetVerifyCode(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 验证码登录
     * @note 接口：POST /api/user/vcode/login
     */
    void handleVerifyCodeLogin(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 会话登录
     * @note 接口：POST /api/user/session/login
     */
    void handleSessionLogin(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 退出登录
     * @note 接口：POST /api/user/logout
     */
    void handleLogout(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取用户信息
     * @note 接口：GET /api/user/info?requestId={requestId}&sessionId={sessionId}&userId={userId}
     */
    void handleGetUserInfo(const httplib::Request& req, httplib::Response& res);

    // ==================== 文件子服务接口 ====================
    
    /**
     * @brief 文件上传（上传文件信息）
     * @note 接口：POST /api/file/upload/info
     */
    void handleFileUploadInfo(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取文件信息
     * @note 接口：GET /api/file/info?requestId={requestId}&sessionId={sessionId}&fileId={fileId}
     */
    void handleGetFileInfo(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 上传文件数据
     * @note 接口：POST /api/file/upload?requestId={requestId}&sessionId={sessionId}&fileId={fileId}
     * @note 请求头：Content-Type: application/octet-stream
     * @note 请求体：文件二进制数据
     */
    void handleFileUpload(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 下载文件
     * @note 接口：GET /api/file/download?requestId={requestId}&sessionId={sessionId}&fileId={fileId}
     */
    void handleFileDownload(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 删除文件
     * @note 接口：DELETE /api/file/{fileId}?requestId={requestId}&sessionId={sessionId}
     */
    void handleFileDelete(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 预览Excel文件
     * @note 接口：POST /api/file/preview
     */
    void handleFilePreview(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取用户文件列表
     * @note 接口：POST /api/file/list
     */
    void handleGetFileList(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 关联文件和聊天会话映射
     * @note 接口：POST /api/file/chat/map
     */
    void handleFileChatMap(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 上传SQLite文件
     * @note 接口：POST /api/file/sqlite/upload?requestId={requestId}&sessionId={sessionId}&filename={filename}
     * @note 请求头：Content-Type: application/octet-stream
     * @note 请求体：SQLite文件二进制数据
     */
    void handleSqliteUpload(const httplib::Request& req, httplib::Response& res);

    // ==================== 存储子服务接口 ====================
    
    /**
     * @brief 新建数据库连接
     * @note 接口：POST /api/db/connect
     */
    void handleDbConnect(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 断开数据库连接
     * @note 接口：POST /api/db/disconnect
     */
    void handleDbDisconnect(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取数据库表列表
     * @note 接口：GET /api/db/tables?requestId={requestId}&sessionId={sessionId}&dbConnectId={dbConnectId}
     */
    void handleGetDbTables(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取表数据
     * @note 接口：POST /api/db/table/data
     */
    void handleGetTableData(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取连接状态
     * @note 接口：POST /api/db/connection/status
     */
    void handleGetConnectionStatus(const httplib::Request& req, httplib::Response& res);

    // ==================== AI子服务接口 ====================
    
    /**
     * @brief 获取支持模型列表
     * @note 接口：POST /api/ai/models
     */
    void handleGetModels(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 新建聊天会话
     * @note 接口：POST /api/ai/session/create
     */
    void handleCreateChatSession(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取聊天会话列表
     * @note 接口：POST /api/ai/chatSessionLists
     */
    void handleGetChatSessionLists(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 获取历史会话
     * @note 接口：POST /api/ai/history
     */
    void handleGetHistory(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 发送消息给模型（流式响应）
     * @note 接口：POST /api/ai/sendStreamMessage
     */
    void handleAiChat(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief 删除聊天会话
     * @note 接口：POST /api/ai/delete
     */
    void handleDeleteChatSession(const httplib::Request& req, httplib::Response& res);

private:
    std::shared_ptr<biterpc::SvcChannels> _svcChannels;
    std::shared_ptr<bitesvc::SvcWatcher> _serviceWatcher;
    std::string _exePath;
};

}
