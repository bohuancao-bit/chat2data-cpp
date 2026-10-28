#include <bite_scaffold/log.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <sstream>
#include <bite_scaffold/util.h>
#include <sqlite3.h>
#include <unistd.h>
#include <limits.h>
#include <cstdlib>
#include "aimessageHandler.h"
#include "chatSessionMgr.h"
#include "promptTemplate.h"
#include "../common/errorHandler.h"
#include "../common/utils.h"
#include "userPrompt.h"
#include "../common/sqlValidator.h"

namespace aiService {

AIMessageHandler::AIMessageHandler(std::shared_ptr<ai_chat_sdk::ChatSDK> chatSdk,
                                   std::shared_ptr<ChatSessionMgr> chatSessionMgr,
                                   std::shared_ptr<biterpc::SvcChannels> svcChannels)
    : _chatSdk(chatSdk)
    , _chatSessionMgr(chatSessionMgr)
    , _svcChannels(svcChannels) {
    INF("AIMessageHandler initialized");
}

// 发送消息主流程
void AIMessageHandler::sendMessage(const SendMessageContext& context, SendMessageCallback writeChunk) {
    try {
        // 1. 根据chatType判断消息场景
        if (context._chatType == "plain") {
            // 普通消息场景
            INF("==========普通聊天场景===========");
            sendPlainMessage(context, writeChunk);
            return;
        }

        // 2. 获取表名列表
        INF("==========获取数据库表列表===========");
        std::vector<std::string> tableNames = getDatabaseTables(context);
        if(tableNames.empty()) {
            WRN("获取数据库表失败，没有发现数据库表");
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
        }
        INF("获取数据库表成功，总共涉及到{}个表", tableNames.size());

        
        // 3. 获取数据库类型字符串
        std::string dbTypeStr = getDatabaseTypeString(context._dbType);
        INF("数据库类型：{}", dbTypeStr);

        // 4. 获取表结构信息
        // 获取数据库连接Id
        INF("==========获取数据源信息===========");
        std::string dbConnectId = getDatabaseConnectId(context);
        std::vector<TableSchemaInfo> tables = getDataSourceInfo(dbConnectId, context._sessionId, tableNames);
        INF("获取数据源信息成功，共获取到{}个表", tables.size());

        // 5. 构建分析提示词
        INF("==========构建分析提示词===========");
        std::string analysisPrompt = buildAnalysisPrompt(context._message, dbTypeStr, tables);
        INF("分析提示词：{}", analysisPrompt);

        // 6. 发送分析消息给模型（流式）
        INF("==========发送分析阶段消息===========");
        std::string analysisResponse = sendMessageToModel(context._chatSessionId, analysisPrompt, writeChunk);
        INF("==========分析阶段消息处理完成===========");

        // 7. 检测邮件请求
        if (isEmailRequest(analysisResponse)) {
            INF("==========模型分析结果为发送邮件请求===========");
            // 删除最后两条消息（邮件提示词和工具调用相关），避免污染会话历史
            removeLastTwoMessages(context._chatSessionId);
            sendEmail(context, writeChunk);
            return;
        }

        // 8. 从模型回复中提取SQL语句
        INF("==========从模型回复中提取SQL语句===========");
        std::string sql = extractSql(analysisResponse);
        if (sql.empty()) {
            WRN("SQL语句为空");
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
        }
        INF("提取到的SQL语句：{}", sql);

        // 9. 规范化SQL
        INF("==========规范化SQL语句===========");
        sql = normalizeSql(sql);
        INF("规范化后的SQL语句：{}", sql);

        // 10. 执行SQL
        INF("==========开始执行SQL语句===========");
        SqlExecuteResult sqlResult = executeSql(context._sessionId, dbConnectId, sql);
        INF("==========执行SQL语句完成===========");

        // 11. 构建总结提示词. 注意：将sql执行结果转换为json字符串
        INF("==========构建总结提示词===========");
        std::string summaryPrompt = buildSummaryPrompt(context._message, buildSqlResultJson(sqlResult));
        INF("总结提示词：{}", summaryPrompt);

        // 12. 发送总结消息给模型（流式）
        INF("==========发送总结阶段消息===========");
        std::string summaryResponse = sendMessageToModel(context._chatSessionId, summaryPrompt, writeChunk);
        INF("==========总结阶段消息处理完成===========");

        // 13. 提取总结内容和图表类型
        INF("==========构建最终响应===========");
        std::string summaryContent = getSummary(summaryResponse);
        if (summaryContent.empty()) {
            summaryContent = summaryResponse;
        }

        std::string displayType = getDisplayType(summaryResponse);

        // 14. 构建最终响应（传入完整的summaryResponse以保留taskStatus、keyFindings字段）
        INF("总结内容：{}", summaryContent);
        INF("图表类型：{}", displayType);
        std::string finalResponse = buildFinalResponse(summaryResponse, displayType, sqlResult);
        INF("最终响应：{}", finalResponse);

        // 15. 发送最终响应
        INF("==========发送最终响应===========");
        pushMessage("<CHART_DATA>" + finalResponse + "</CHART_DATA>",false, writeChunk);
        pushMessage("",true, writeChunk);
        INF("==========发送最终响应完成===========");
        INF("最终响应结果：{}", finalResponse);

        // 16. 更新会话活跃时间和消息数，并更新会话标题
        std::string title = extractTitleFromFirstAssistantMessage(context._chatSessionId);
        updateSessionActivity(context._chatSessionId, title);

        // 17. 更新ChatSDK底层sqlite中最近一条assistant消息更新为最终json
        updateLastAssistantMessage(context._chatSessionId, finalResponse);

    } catch (const chat2Data::Chat2DataException& e) {
        ERR("Chat2DataException in sendMessage: code={}, msg={}", 
            static_cast<int>(e.getErrorCode()), e.what());
        std::string errorMsg = "error: " + std::string(e.what());
        pushMessage(errorMsg, true, writeChunk);
    } catch (const std::exception& e) {
        ERR("std::exception in sendMessage: msg={}", e.what());
        std::string errorMsg = "error: " + std::string(e.what());
        pushMessage(errorMsg, true, writeChunk);
    }
}

std::vector<std::string> AIMessageHandler::getExcelWorksheetTables(const std::string& sessionId, const std::string& fileId) {
    std::vector<std::string> result;

    // 通过文件子服务获取Excel文件的worksheet数据库表名列表
    auto channel = _svcChannels->getNode(FLAGS_file_service);
    if (!channel) {
        ERR("Failed to get FileService channel");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 构建RPC请求
    chat2Data::fileService::GetWorksheetDBTablesRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_session_id(sessionId);
    request.set_file_id(fileId);

    // 发起RPC调用
    chat2Data::fileService::GetWorksheetDBTablesResponse response;
    brpc::Controller controller;
    chat2Data::fileService::FileService_Stub stub(channel.get());
    stub.GetWorksheetDBTables(&controller, &request, &response, nullptr);

    // 检测RPC调用是否成功
    if (controller.Failed()) {
        ERR("GetWorksheetDBTables RPC failed: {}", controller.ErrorText());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }
    if (response.error_code() != 0) {
        ERR("GetWorksheetDBTables failed: errorCode={}, errorMsg={}",
            response.error_code(), response.error_msg());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 获取结果
    for (const auto& tableName : response.result().worksheet_dbtables()) {
        result.push_back(tableName);
    }

    INF("GetExcelWorksheetTables success, fileId={}, tableCount={}", fileId, result.size());
    return result;
}

std::vector<std::string> AIMessageHandler::getDatabaseTables(const std::string& tableNames) {
    // 1. 检测tableNames是否为空
    std::vector<std::string> result;
    if (tableNames.empty()) {
        return result;
    }

    // 2. 解析tableNames，提取表名列表
    std::stringstream ss(tableNames);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::string AIMessageHandler::buildAnalysisPrompt(const std::string& userInput,
                                                 const std::string& dbType,
                                                 const std::vector<TableSchemaInfo>& tables) {
    // 分析提示词模板
    PromptTemplate prompt(ANALYSIS_PROMPT);
    
    // 构建表结构信息
    std::string tableSchema;
    std::string tableName;
    std::string dataExample;
    for (const auto& table : tables) {
        tableSchema += table._schema + "\n\n";
        tableName += table._tableName + "\n\n";
        dataExample += table._sampleData + "\n\n";
    }

    // 设置占位符和实际值的映射
    prompt.setPlaceholder("DataBase", dbType);
    prompt.setPlaceholder("table_schema", tableSchema);
    prompt.setPlaceholder("table_name", tableName);
    prompt.setPlaceholder("data_example", dataExample);
    prompt.setPlaceholder("user_input", userInput);

    // 构建实际提示词
    return prompt.build();
}

// 构建总结提示词
std::string AIMessageHandler::buildSummaryPrompt(const std::string& userInput, const std::string& sqlResult) {
    // 总结提示词模板
    PromptTemplate prompt(SUMMARY_PROMPT);
    prompt.setPlaceholder("user_input", userInput);
    prompt.setPlaceholder("result_json", sqlResult);

    // 构建实际提示词
    return prompt.build();
}

// 构建邮件提示词
std::string AIMessageHandler::buildEmailPrompt(const Json::Value& emailParamJson) {
    // 1. 从json对象中提取标题
    std::string title("消息标题");
    if(emailParamJson.isMember("question")) {
        title = emailParamJson["question"].asString();
    }

    // 2. 对emailParamJson进行序列化
    std::string emailParamStr = biteutil::JSON::serialize(emailParamJson).value();
    
    // 3. 构建邮件内容生成提示词
    PromptTemplate prompt(EMAIL_PROMPT);
    prompt.setPlaceholder("user_input", title);
    prompt.setPlaceholder("email_param", emailParamStr);

    // 4. 返回邮件内容生成提示词
    return prompt.build();
}

std::string AIMessageHandler::extractSql(const std::string& response) {
    return extractTagContent(response, "<SQL_START>", "<SQL_END>");
}

std::string AIMessageHandler::normalizeSql(const std::string& sql) {
    // 使用SQLValidator规范化SQL
    // 1. 移除注释
    // 2. 移除首尾空白字符
    std::string normalizedSql = chat2Data::SQLValidator::normalize(sql);

    // sql校验中包含：
    // 1. 检查是否包含危险关键词
    // 2. 检查是否包含多个语句
    // 3. 检测是否为支持的SQL类型
    if(chat2Data::SQLValidator::validate(normalizedSql)) {
        return normalizedSql;
    }else{
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }
}

SqlExecuteResult AIMessageHandler::executeSql(const std::string& sessionId, const std::string& dbConnectId, const std::string& sql) {
    SqlExecuteResult result;
    result._success = false;

    // 通过数据库子服务执行SQL
    auto channel = _svcChannels->getNode(FLAGS_db_service);
    if (!channel) {
        ERR("Failed to get DatabaseService channel");
        result._errorMsg = "Database service unavailable";
        return result;
    }

    // 构建RPC请求
    chat2Data::DatabaseService::ExecuteSQLRequest rpcRequest;
    rpcRequest.set_request_id(chat2Data::Utils::generateUuid());
    rpcRequest.set_session_id(sessionId);
    rpcRequest.set_db_connect_id(dbConnectId);
    rpcRequest.set_sql(sql);

    // 创建RPC客户端
    chat2Data::DatabaseService::DatabaseService_Stub stub(channel.get());

    // 发起RPC调用
    chat2Data::DatabaseService::ExecuteSQLResponse rpcResponse;
    brpc::Controller controller;
    stub.ExecuteSQL(&controller, &rpcRequest, &rpcResponse, nullptr);

    // 检测RPC调用是否成功
    if (controller.Failed()) {
        ERR("ExecuteSQL RPC failed: {}", controller.ErrorText());
        result._errorMsg = controller.ErrorText();
        return result;
    }
    if (rpcResponse.error_code() != 0) {
        ERR("ExecuteSQL failed: errorCode={}, errorMsg={}",
            rpcResponse.error_code(), rpcResponse.error_msg());
        result._errorMsg = rpcResponse.error_msg();
        return result;
    }

    // 解析SQL执行结果
    result._success = true;
    result._isQuery = rpcResponse.is_query();
    result._affectedRows = rpcResponse.affected_rows();
    for (const auto& col : rpcResponse.columns()) {
        result._columns.push_back(col);
    }
    for (const auto& colType : rpcResponse.column_types()) {
        result._columnTypes.push_back(colType);
    }
    for (const auto& row : rpcResponse.rows()) {
        std::vector<std::string> rowData;
        for (const auto& cell : row.cells()) {
            rowData.push_back(cell);
        }
        result._rows.push_back(rowData);
    }

    INF("SQL执行成功, dbConnectId={}, columnsCount={}, columnTypesCount={}, rowsCount={}",
        dbConnectId, result._columns.size(), result._columnTypes.size(), result._rows.size());
    return result;
}

bool AIMessageHandler::isEmailRequest(const std::string& response) {
    std::string emailStartTag = "<EMAIL_START>";

    // 找起始标签
    size_t start = response.find(emailStartTag);
    if (start == std::string::npos) {
        return false;
    }
    // 找结束标签
    size_t end = response.find("<EMAIL_END>", start + emailStartTag.size());
    if (end == std::string::npos) {
        return false;
    }

    return start < end;
}

void AIMessageHandler::sendEmail(const SendMessageContext& context, SendMessageCallback writeChunk) {
    try {
        // 1. 获取用户邮箱
        INF("开始获取用户邮箱");
        std::string userEmail = getUserEmail(context);
        if (userEmail.empty()) {
            ERR("用户邮箱为空");
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
        }
        INF("用户邮箱获取成功: {}", userEmail);

        // 2. 获取最近两条assistant消息
        INF("开始获取最近两条assistant消息");
        std::string summaryMessage;
        std::string analysisMessage;
        getRecentAssistantMessages(context._chatSessionId, summaryMessage, analysisMessage);
        INF("最近两条assistant消息获取成功: {}, {}", analysisMessage, summaryMessage);

        // 3. 构建邮件参数Json
        INF("开始构建生成邮件内容提示词");
        Json::Value emailParamJson = buildEmailParamsJson(analysisMessage, summaryMessage);

        // 4. 构建邮件生成提示词
        std::string emailPrompt = buildEmailPrompt(emailParamJson);
        INF("生成邮件内容提示词构建成功: {}", emailPrompt);

        // 5. 发送提示词给模型，生成邮件内容
        INF("开始发送生成邮件内容提示词给模型");
        // 注意：邮件内容不需要在前端页面进行展示，即模型生成的邮件内容的流式数据不需要writeChunk给前端
        std::string emailResponse = sendMessageToModel(context._chatSessionId, emailPrompt, nullptr);
        INF("生成邮件内容模型回复: {}", emailResponse);
        // 邮件内容不需要放在聊天会话的消息列表中
        if(!emailResponse.empty()){
            removeLastTwoMessages(context._chatSessionId);
        }

        // 6. 从模型回复中提取邮件主题和内容
        INF("开始从模型回复中提取邮件主题和内容");
        std::string subject("邮件主题");
        std::string content("邮件内容");
        buildEmailContent(emailResponse, subject, content);
        INF("邮件主题: {}, 邮件内容: {}", subject, content);

        // 7. 通过邮件通知子服务发送邮件
        INF("开始发送邮件...");
        sendEmail(userEmail, subject, content);
        INF("邮件发送成功");

        // 8. 给前端发送成功消息
        pushMessage("<EMAIL_START>邮件发送成功，请注意查收<EMAIL_END>", false, writeChunk);
        pushMessage("",true, writeChunk);
        INF("发送邮件成功");
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("发送邮件失败: code={}, msg={}", static_cast<int>(e.getErrorCode()), e.what());
        pushMessage("<EMAIL_START>邮件发送失败：" + std::string(e.what()) + "<EMAIL_END>", true, writeChunk);
    } catch (const std::exception& e) {
        ERR("发送邮件失败: {}", e.what());
        pushMessage("<EMAIL_START>邮件发送失败：" + std::string(e.what()) + "<EMAIL_END>", true, writeChunk);
    }
}

void AIMessageHandler::sendPlainMessage(const SendMessageContext& context, SendMessageCallback writeChunk) {
    try {
        // 发送消息给模型
        std::string response = sendMessageToModel(context._chatSessionId, context._message, writeChunk);

        // 更新会话活跃时间和消息数，并更新会话标题
        std::string title = extractTitleFromFirstAssistantMessage(context._chatSessionId);
        updateSessionActivity(context._chatSessionId, title);

    } catch (const std::exception& e) {
        ERR("sendPlainMessage failed: {}", e.what());
        std::string errorMsg = "error: " + std::string(e.what());
        pushMessage(errorMsg, true, writeChunk);
    }
}

std::vector<TableSchemaInfo> AIMessageHandler::getDataSourceInfo(const std::string& dbConnectId,
                                                                  const std::string& sessionId,
                                                                  const std::vector<std::string>& tableNames) {
    std::vector<TableSchemaInfo> result;

    // 通过数据库子服务获取表结构和采样数据
    auto channel = _svcChannels->getNode(FLAGS_db_service);
    if (!channel) {
        ERR("Failed to get DatabaseService channel");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    chat2Data::DatabaseService::DatabaseService_Stub stub(channel.get());

    for (const auto& tableName : tableNames) {
        TableSchemaInfo info;
        info._tableName = tableName;

        // 1. 获取表结构
        {
            INF("开始获取{}表结构", tableName);
            // 1.1 构建RPC请求
            chat2Data::DatabaseService::GetTableStructRequest request;
            request.set_request_id(chat2Data::Utils::generateUuid());
            request.set_session_id(sessionId);
            request.set_db_connect_id(dbConnectId);
            request.set_table_name(tableName);

            // 1.2 发送RPC请求
            chat2Data::DatabaseService::GetTableStructResponse response;
            brpc::Controller controller;
            stub.GetTableStruct(&controller, &request, &response, nullptr);

            // 1.3 检测RPC调用是否成功
            if (controller.Failed()) {
                ERR("GetTableStruct RPC failed: {}", controller.ErrorText());
                throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
            }
            if (response.error_code() != 0) {
                ERR("GetTableStruct failed: errorCode={}, errorMsg={}",
                    response.error_code(), response.error_msg());
                throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
            }

            // 1.4 解析RPC响应
            info._schema = response.table_struct();
            INF("获取{}表结构成功，表结构为：{}", tableName, info._schema);
        }

        // 2. 获取采样数据，限制条数为5
        {
            INF("开始获取{}表采样数据", tableName);
            // 2.1 构建RPC请求
            chat2Data::DatabaseService::GetSampleDataRequest request;
            request.set_request_id(chat2Data::Utils::generateUuid());
            request.set_session_id(sessionId);
            request.set_db_connect_id(dbConnectId);
            request.set_table_name(tableName);
            request.set_limit(SAMPLE_DATA_LIMIT);

            // 2.2 发送RPC请求
            chat2Data::DatabaseService::GetSampleDataResponse response;
            brpc::Controller controller;
            stub.GetSampleData(&controller, &request, &response, nullptr);

            // 2.3 检测RPC调用是否成功
            if (controller.Failed()) {
                ERR("GetSampleData RPC failed: {}", controller.ErrorText());
                throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
            }
            if (response.error_code() != 0) {
                ERR("GetSampleData failed: errorCode={}, errorMsg={}",
                    response.error_code(), response.error_msg());
                throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
            }
            // 2.4 解析RPC响应
            info._sampleData = response.sample_data();
            INF("获取{}表采样数据成功，采样数据为：{}", tableName, info._sampleData);
        }

        result.push_back(info);
    }

    return result;
}

void AIMessageHandler::pushMessage(const std::string& content, bool done,SendMessageCallback writeChunk) {
    if (writeChunk) {
        writeChunk(content, done);
    }
}

void AIMessageHandler::updateSessionActivity(const std::string& chatSessionId, const std::string& title) {
    // 更新会话的最近活跃时间和消息数
    auto sessionInfoOpt = _chatSessionMgr->getChatSessionBySessionId(chatSessionId);
    if (!sessionInfoOpt.has_value()) {
        ERR("Session not found, chatSessionId={}", chatSessionId);
        return;
    }

    ChatSessionInfo sessionInfo = sessionInfoOpt.value();
    sessionInfo._updateTime = std::chrono::system_clock::now().time_since_epoch().count();  // 更新最近活跃时间
    sessionInfo._messageCount += 2;  // 消息数加2：用户消息+模型回复

    // 如果传入了标题且会话当前没有标题，则更新会话标题
    if (!title.empty() && sessionInfo._title.empty()) {
        sessionInfo._title = title;
        INF("Update session title, chatSessionId={}, title={}", chatSessionId, title);
    }

    if (!_chatSessionMgr->saveChatSession(sessionInfo)) {
        ERR("Failed to save session, chatSessionId={}", chatSessionId);
        return;
    }

    INF("UpdateSessionActivity success, chatSessionId={}, messageCount={}", chatSessionId, sessionInfo._messageCount);
}

std::string AIMessageHandler::extractTitleFromFirstAssistantMessage(const std::string& chatSessionId) {
    // 从ChatSDK获取会话的所有历史消息
    std::vector<ai_chat_sdk::Message> messages = _chatSdk->getSession(chatSessionId)->_messages;

    // 遍历历史消息，找到第一条assistant消息
    for (const auto& msg : messages) {
        if (msg._role == "assistant") {
            // 从助手消息中提取标题
            std::string title = extractTagContent(msg._content, "<TITLE_START>", "<TITLE_END>");
            if (!title.empty()) {
                INF("Extract title from first assistant message, chatSessionId={}, title={}", chatSessionId, title);
                return title;
            }else{
                // 如果获取失败，直接使用第一条用户消息的前20个字作为标题
                return messages[0]._content.substr(0, 20);
            }
        }
    }

    WRN("No title found in first assistant message, chatSessionId={}", chatSessionId);
    return "";
}

std::string AIMessageHandler::sendMessageToModel(const std::string& chatSessionId, const std::string& message, SendMessageCallback writeChunk) {
    std::string fullResponse;

    // 使用ChatSDK发送流式消息
    // 回调函数：
    // 1. 接收模型返回的流式消息, 并累加到fullResponse中
    // 2. 调用writeChunk回调函数，将消息推送给客户端
    _chatSdk->sendMessageStream(chatSessionId, message,
        [&fullResponse, writeChunk](const std::string& chunk, bool done) {
            fullResponse += chunk;
            if (writeChunk) {
                writeChunk(chunk, false);
            }
            return true;
        });

    return fullResponse;
}

std::string AIMessageHandler::extractTagContent(const std::string& response, const std::string& tagStart, const std::string& tagEnd) {
    // 1. 查找起始标签
    size_t startPos = response.find(tagStart);
    if (startPos == std::string::npos) {
        return "";
    }

    // 2. 查找结束标签
    startPos += tagStart.length();
    size_t endPos = response.find(tagEnd, startPos);
    if (endPos == std::string::npos) {
        return "";
    }

    // 3. 提取内容
    return response.substr(startPos, endPos - startPos);
}

std::string AIMessageHandler::buildFinalResponse(const std::string& summaryResponseJson, const std::string& displayType, const SqlExecuteResult& sqlResult) {
    //Json::Value responseJson;

    // 解析模型返回的完整JSON，提取taskStatus、keyFindings、summary字段
    Json::Value responseJson = biteutil::JSON::unserialize(summaryResponseJson).value();
    #if 0
    // 保留taskStatus字段
    if (summaryJson.isObject() && summaryJson.isMember("taskStatus")) {
        responseJson["taskStatus"] = summaryJson["taskStatus"];
    }
    
    // 保留keyFindings字段
    if (summaryJson.isObject() && summaryJson.isMember("keyFindings")) {
        responseJson["keyFindings"] = summaryJson["keyFindings"];
    }
    
    // 保留summary字段
    if (summaryJson.isObject() && summaryJson.isMember("summary")) {
        responseJson["summary"] = summaryJson["summary"];
    }
    
    // chartType字段（前端chart.js期望此字段名）
    if (summaryJson.isObject() && summaryJson.isMember("chartType")) {
        responseJson["chartType"] = summaryJson["chartType"];
    }
    
    // chartConfig字段（前端需要用于图表配置）
    if (summaryJson.isObject() && summaryJson.isMember("chartConfig")) {
        responseJson["chartConfig"] = summaryJson["chartConfig"];
    }
    
    // displayType字段
    //responseJson["displayType"] = displayType;
#endif

    // data字段
    Json::Value dataJson;
    if (sqlResult._success) {
        // 填充列名称
        Json::Value columnsJson(Json::arrayValue);
        for (const auto& col : sqlResult._columns) {
            columnsJson.append(col);
        }
        dataJson["columns"] = columnsJson;

        // 填充列类型
        Json::Value columnTypesJson(Json::arrayValue);
        for (const auto& colType : sqlResult._columnTypes) {
            columnTypesJson.append(colType);
        }
        dataJson["columnTypes"] = columnTypesJson;

        // 填充数据行
        Json::Value rowsJson(Json::arrayValue);
        for (const auto& row : sqlResult._rows) {
            Json::Value rowJson(Json::arrayValue);
            for (const auto& cell : row) {
                rowJson.append(cell);
            }
            rowsJson.append(rowJson);
        }
        dataJson["rows"] = rowsJson;
    }
    responseJson["data"] = dataJson;

    // 序列化JSON字符串（单行格式，避免SSE传输时被换行符分割）
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, responseJson);
}

std::string AIMessageHandler::getDatabaseTypeString(chat2Data::AiService::DataType dbType) {
    switch (dbType) {
        case chat2Data::AiService::DataType::EXCEL:
            return "Excel";
        case chat2Data::AiService::DataType::MYSQL:
            return "MySQL";
        case chat2Data::AiService::DataType::SQLITE:
            return "SQLite";
        default:
            return "Unknown";
    }
}

std::vector<std::string> AIMessageHandler::getDatabaseTables(const SendMessageContext& context){
    std::vector<std::string> tableNames;
    if (context._chatType == "excel") {
        tableNames = getExcelWorksheetTables(context._sessionId, context._fileId);
    } else if (context._chatType == "database") {
        tableNames = getDatabaseTables(context._tableNames[0]);
    } else {
        WRN("Unknown chat type: {}", context._chatType);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_PARAM_INVALID);
    }

    return tableNames;
}

std::string AIMessageHandler::getDatabaseConnectId(const SendMessageContext& context) {
    if (context._chatType == "excel") {
        return "excel_default";
    }else if (context._chatType == "database") {
        return context._dbConnectId;
    }else{
        WRN("Unknown chat type: {}", context._chatType);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_PARAM_INVALID);
    }
}

std::string AIMessageHandler::getUserEmail(const SendMessageContext& context) { 
    // 1.1 获取用户子服务服务信道
    auto userChannel = _svcChannels->getNode(FLAGS_user_service);
    if (!userChannel) {
        ERR("Failed to get UserService channel");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 1.2 构建RPC请求
    chat2Data::userService::GetUserInfoRequest userRequest;
    userRequest.set_request_id(chat2Data::Utils::generateUuid());
    userRequest.set_session_id(context._sessionId);
    userRequest.set_user_id(context._userId);

    // 1.3 创建RPC客户端
    chat2Data::userService::UserService_Stub userStub(userChannel.get());

    // 1.4 发起RPC调用
    chat2Data::userService::GetUserInfoResponse userResponse;
    brpc::Controller userController;
    userStub.GetUserInfo(&userController, &userRequest, &userResponse, nullptr);

    // 1.5 检测RPC调用是否成功
    if (userController.Failed()) {
        ERR("GetUserInfo RPC failed: {}", userController.ErrorText());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }
    if (userResponse.error_code() != 0) {
        ERR("GetUserInfo failed: errorCode={}, errorMsg={}", userResponse.error_code(), userResponse.error_msg());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 1.6 解析用户邮箱
    std::string userEmail = userResponse.result().user_info().email();
    if (userEmail.empty()) {
        ERR("User email is empty, userId={}", context._userId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    return userEmail;
}

void AIMessageHandler::getRecentAssistantMessages(const std::string& chatSessionId, std::string& summaryMessage, std::string& analysisMessage) {

    // 通过chatsdk获取chatSessionId对应的会话
    auto session = _chatSdk->getSession(chatSessionId);
    if (!session) {
        ERR("Session not found, sessionId={}", chatSessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 查找最后两条assistant消息
    int assistantCount = 0;
    auto messages = session->_messages;   // 获取该会话中的所有历史消息 std::vector<Message>
    // 通过反向迭代器遍历，获取最近两次assistant消息
    for (auto it = messages.rbegin(); it != messages.rend() && assistantCount < 2; ++it) {
        if ((*it)._role == "assistant") {
            if (assistantCount == 0) {
                summaryMessage = (*it)._content;
            } else {
                analysisMessage = (*it)._content;
            }
            assistantCount++;
        }
    }

    if (analysisMessage.empty() || summaryMessage.empty()) {
        ERR("Failed to find assistant messages, assistantCount={}", assistantCount);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }
}

std::string AIMessageHandler::getSummary(const std::string& summaryMessage) {
    Json::Value summaryJson = biteutil::JSON::unserialize(summaryMessage).value();
    std::string summaryContent;
    if(summaryJson.isObject() && summaryJson.isMember("summary")){
        summaryContent = summaryJson["summary"].asString();
    }else{
        ERR("Failed to parse summary message, summaryMessage={}", summaryMessage);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    return summaryContent;
}

Json::Value AIMessageHandler::buildEmailParamsJson(const std::string& analysisMessage, const std::string& summaryMessage) {
    // 1. 从analysisMessage中提取 标题 和 分析内容
    std::string title = extractTagContent(analysisMessage, "<TITLE_START>", "<TITLE_END>");
    if (title.empty()) {
        title = "数据分析报告";
    }
    std::string analysisContent = extractTagContent(analysisMessage, "<ANALYSIS_START>", "<ANALYSIS_END>");
    if (analysisContent.empty()) {
        analysisContent = analysisMessage;
    }

    // 2. 从summaryMessage中提取总结内容
    // 注意：summaryMessage是json字符串
    std::string summaryContent = getSummary(summaryMessage);
    
    // 3. 构建邮件参数的json对象
    Json::Value emailParam;
    emailParam["question"] = title;
    emailParam["analysis"] = analysisContent;
    emailParam["summary"] = summaryContent;

    // 4. 返回邮件参数的json对象
    return emailParam;
}

void AIMessageHandler::buildEmailContent(const std::string& emailResponse, std::string& subject, std::string& content) {
    // 1. 将emailResponse转换为json对象
    Json::Value emailJson = biteutil::JSON::unserialize(emailResponse).value();
    if(!emailJson.isObject()){
        ERR("Failed to parse email response, emailResponse={}", emailResponse);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 2. 从json对象中提取邮件内容
    if (emailJson.isMember("subject")) {
        subject = emailJson["subject"].asString();
    }

    if (emailJson.isMember("content")) {
        content = emailJson["content"].asString();
    }
}

void AIMessageHandler::sendEmail(const std::string& email, const std::string& subject, const std::string& content) {
    // 1. 获取邮件通知子服务的channel
    auto notifyChannel = _svcChannels->getNode(FLAGS_notify_service);
    if (!notifyChannel) {
        ERR("Failed to get NotifyService channel");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 2. 构建邮件请求
    chat2Data::notifyService::SendEmailRequest emailRequest;
    emailRequest.set_request_id(chat2Data::Utils::generateUuid());
    emailRequest.set_to_email(email);
    emailRequest.set_subject(subject);
    emailRequest.set_content(content);

    // 3. 创建RPC客户端
    chat2Data::notifyService::NotifyService_Stub notifyStub(notifyChannel.get());

    // 4. 调用RPC方法发送邮件
    chat2Data::notifyService::SendEmailResponse emailResponse2;
    brpc::Controller notifyController;
    notifyStub.SendEmail(&notifyController, &emailRequest, &emailResponse2, nullptr);

    // 5. 检查RPC调用是否成功
    if (notifyController.Failed()) {
        ERR("SendEmail RPC failed: {}", notifyController.ErrorText());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    if (emailResponse2.error_code() != 0) {
        ERR("SendEmail failed: errorCode={}, errorMsg={}",
            emailResponse2.error_code(), emailResponse2.error_msg());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }
}

std::string AIMessageHandler::buildSqlResultJson(const SqlExecuteResult& sqlResult) {
    Json::Value resultJson;
    if (sqlResult._success) {
        // 1. 提取列名称
        Json::Value columnsJson(Json::arrayValue);
        for (const auto& col : sqlResult._columns) {
            columnsJson.append(col);
        }
        resultJson["columns"] = columnsJson;

        // 2. 提取行数据
        Json::Value rowsJson(Json::arrayValue);
        for (const auto& row : sqlResult._rows) {
            Json::Value rowJson(Json::arrayValue);
            for (const auto& cell : row) {
                rowJson.append(cell);
            }
            rowsJson.append(rowJson);
        }
        resultJson["rows"] = rowsJson;

       
    } else {
        resultJson["error"] = sqlResult._errorMsg;
    }

    // 3. 转换为json字符串
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, resultJson);
}

std::string AIMessageHandler::getDisplayType(const std::string& summaryMessage) {
    // 1. 将summaryMessage转换为json对象
    Json::Value summaryJson = biteutil::JSON::unserialize(summaryMessage).value();
    if(!summaryJson.isObject()){
        ERR("Failed to parse summary message, summaryMessage={}", summaryMessage);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SEND_MESSAGE_FAILED);
    }

    // 2. 从json对象中提取图表类型
    if (summaryJson.isMember("chartType")) {
        return summaryJson["chartType"].asString();
    }

    return "";
}

// 获取chatDB.db文件路径
std::string AIMessageHandler::getChatDBPath() {
    // 1. 获取可执行程序所在目录
    char exePath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
    if (count == -1) {
        ERR("Failed to get executable path");
        return "";
    }
    std::string exeDir(exePath, count);
    size_t lastSlash = exeDir.find_last_of('/');
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }

    // 2. 构建数据库文件路径
    std::string dbPath = exeDir + "/chatDB.db";
    INF("Database path: {}", dbPath);
    return dbPath;
}

void AIMessageHandler::updateLastAssistantMessage(const std::string& chatSessionId, const std::string& finalJson) {
    // 1. 获取数据库路径
    std::string dbPath = getChatDBPath();
    if (dbPath.empty()) {
        ERR("Failed to get database path");
        return;
    }

    // 2. 打开数据库连接
    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        ERR("Failed to open database: {}", sqlite3_errmsg(db));
        if (db) {
            sqlite3_close(db);
        }
        return;
    }

    // 3. 构建更新SQL语句：更新指定session的最后一条assistant消息
    // 使用子查询找到指定session中timestamp最大的assistant消息
    std::string updateSQL = R"(
        UPDATE messages
        SET content = ?
        WHERE message_id = (
            SELECT message_id FROM messages
            WHERE session_id = ? AND role = 'assistant'
            ORDER BY timestamp DESC
            LIMIT 1
        );
    )";

    // 4. 准备并执行SQL语句
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, updateSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        ERR("Failed to prepare update statement: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // 绑定参数
    sqlite3_bind_text(stmt, 1, finalJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chatSessionId.c_str(), -1, SQLITE_TRANSIENT);

    // 执行SQL
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        ERR("Failed to execute update statement: {}", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return;
    }

    // 释放资源
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    INF("Update last assistant message success, chatSessionId={}", chatSessionId);
}

void AIMessageHandler::removeLastTwoMessages(const std::string& chatSessionId) {
    // 1. 获取数据库路径
    std::string dbPath = getChatDBPath();
    if (dbPath.empty()) {
        ERR("Failed to get database path");
        return;
    }

    // 2. 打开数据库连接
    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        ERR("Failed to open database: {}", sqlite3_errmsg(db));
        if (db) {
            sqlite3_close(db);
        }
        return;
    }

    // 3. 构建删除SQL语句：删除指定session中最后两条消息（按timestamp降序）
    std::string deleteSQL = R"(
        DELETE FROM messages
        WHERE session_id = ? AND rowid IN (
            SELECT rowid FROM messages
            WHERE session_id = ?
            ORDER BY timestamp DESC
            LIMIT 2
        );
    )";

    // 4. 准备并执行SQL语句
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, deleteSQL.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        ERR("Failed to prepare delete statement: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // 绑定参数
    sqlite3_bind_text(stmt, 1, chatSessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chatSessionId.c_str(), -1, SQLITE_TRANSIENT);

    // 执行SQL
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        ERR("Failed to execute delete statement: {}", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return;
    }

    // 释放资源
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    INF("Remove last two messages success, chatSessionId={}", chatSessionId);
}

} // namespace aiService
