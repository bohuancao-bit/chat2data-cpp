#pragma once

#include <jsoncpp/json/value.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <gflags/gflags.h>
#include <ai_chat_sdk/ChatSDK.h>
#include <bite_scaffold/rpc.h>

#include "../proto/protoCode/dbService.pb.h"
#include "../proto/protoCode/fileService.pb.h"
#include "../proto/protoCode/notifyService.pb.h"
#include "../proto/protoCode/userService.pb.h"

#include "common.h"

// 声明gflags变量（在main.cc中定义）
DECLARE_string(file_service);
DECLARE_string(db_service);
DECLARE_string(user_service);
DECLARE_string(notify_service);

namespace aiService {

// 前向声明
class ChatSessionMgr;

// AI消息处理器，负责发送消息的核心业务逻辑
class AIMessageHandler {
public:
    using SendMessageCallback = std::function<void(const std::string&, bool)>;
    AIMessageHandler(std::shared_ptr<ai_chat_sdk::ChatSDK> chatSdk,
                     std::shared_ptr<ChatSessionMgr> chatSessionMgr,
                     std::shared_ptr<biterpc::SvcChannels> svcChannels);

    // 发送消息主流程
    // context: 发送消息上下文  writeChunk: 消息回调函数，用于推送消息给前端
    void sendMessage(const SendMessageContext& context, SendMessageCallback writeChunk);

private:
    // Excel场景：获取Excel文件的worksheet数据库表名列表
    std::vector<std::string> getExcelWorksheetTables(const std::string& sessionId, const std::string& fileId);

    // 数据库场景：从请求参数获取表名列表.
    // tableNames中多个表之间以逗号间隔
    std::vector<std::string> getDatabaseTables(const std::string& tableNames);

    // 构建分析提示词
    std::string buildAnalysisPrompt(const std::string& userInput, const std::string& dbType, const std::vector<TableSchemaInfo>& tables);

    // 构建总结提示词
    std::string buildSummaryPrompt(const std::string& userInput, const std::string& sqlResult);

    // 构建邮件内容生成提示词
    std::string buildEmailPrompt(const Json::Value& emailParamJson);

    // 从模型响应中提取SQL语句
    std::string extractSql(const std::string& response);

    // 规范化SQL语句
    std::string normalizeSql(const std::string& sql);

    // 执行SQL语句
    SqlExecuteResult executeSql(const std::string& sessionId, const std::string& dbConnectId, const std::string& sql);

    // 检测模型回复是否为发送邮件请求 <EMAIL_START>sendMessage<EMAIL_END>
    bool isEmailRequest(const std::string& response);

    // 发送邮件
    void sendEmail(const SendMessageContext& context, SendMessageCallback writeChunk);

    // 发送普通消息
    void sendPlainMessage(const SendMessageContext& context, SendMessageCallback writeChunk);

    // 获取数据源：表结构 和 表采样数据
    std::vector<TableSchemaInfo> getDataSourceInfo(const std::string& dbConnectId, const std::string& sessionId, const std::vector<std::string>& tableNames);

    //推送消息给前端
    void pushMessage(const std::string& content, bool done, SendMessageCallback writeChunk);

    // 更新会话活跃时间和消息数，并更新会话标题
    void updateSessionActivity(const std::string& chatSessionId, const std::string& title = "");

    // 从聊天会话的第一条助手消息中提取标题
    std::string extractTitleFromFirstAssistantMessage(const std::string& chatSessionId);

    // 发送消息给模型（流式），累积响应并实时推送
    std::string sendMessageToModel(const std::string& chatSessionId, const std::string& message, SendMessageCallback writeChunk);

    // 从模型响应中提取标签内容
    std::string extractTagContent(const std::string& response, const std::string& tagStart, const std::string& tagEnd);

    // 构建最终响应JSON（保留模型返回的taskStatus、keyFindings、summary字段）
    std::string buildFinalResponse(const std::string& summaryResponseJson, const std::string& displayType, const SqlExecuteResult& sqlResult);

    // 获取数据库类型字符串
    std::string getDatabaseTypeString(chat2Data::AiService::DataType dbType);

    // 获取数据库表列表
    std::vector<std::string> getDatabaseTables(const SendMessageContext& context);

    // 获取数据库连接Id
    std::string getDatabaseConnectId(const SendMessageContext& context);

    // 获取用户邮箱
    std::string getUserEmail(const SendMessageContext& context);

    // 获取最近两条assistant消息
    void getRecentAssistantMessages(const std::string& chatSessionId, std::string& summaryMessage, std::string& analysisMessage);

    // 构建邮件参数Json
    Json::Value buildEmailParamsJson(const std::string& analysisMessage, const std::string& summaryMessage);

    // 构建邮件内容
    void buildEmailContent(const std::string& emailResponse, std::string& subject, std::string& content);

    // 发送邮件(调用邮件通知子服务将邮件发送到用户邮箱)
    void sendEmail(const std::string& email, const std::string& subject, const std::string& content);

     // 获取总结内容
    std::string getSummary(const std::string& summaryMessage);

    // 获取图表类型
    std::string getDisplayType(const std::string& summaryMessage);

    // 构建SQL执行结果Json
    std::string buildSqlResultJson(const SqlExecuteResult& sqlResult);

    // 更新ChatSDK底层sqlite中最近一条assistant消息为最终json
    void updateLastAssistantMessage(const std::string& chatSessionId, const std::string& finalJson);

    // 删除ChatSDK底层sqlite中指定会话的最后两条消息
    void removeLastTwoMessages(const std::string& chatSessionId);

    // 获取chatDB.db文件路径
    std::string getChatDBPath();

private:
    std::shared_ptr<ai_chat_sdk::ChatSDK> _chatSdk;           // ChatSDK实例
    std::shared_ptr<ChatSessionMgr> _chatSessionMgr;         // 会话管理器
    std::shared_ptr<biterpc::SvcChannels> _svcChannels;      // RPC通信
    const static int SAMPLE_DATA_LIMIT = 5;                  // 采样数据条数
};

} // namespace aiService
