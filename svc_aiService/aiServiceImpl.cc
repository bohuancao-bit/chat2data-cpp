#include <exception>
#include <butil/logging.h>
#include <brpc/controller.h>
#include <brpc/server.h>
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>
#include "../common/errorHandler.h"
#include "aiServiceImpl.h"
#include "aiBusiness.h"
#include "aimessageHandler.h"


namespace aiService {

AIServiceImpl::AIServiceImpl(std::shared_ptr<AIBusiness> aiBusiness)
    : _aiBusiness(aiBusiness) {
}

AIServiceImpl::~AIServiceImpl() {
}

void AIServiceImpl::GetModels(google::protobuf::RpcController* controller,
                             const chat2Data::AiService::GetModelsRequest* request,
                             chat2Data::AiService::GetModelsResponse* response,
                             google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();

    // 3. 调用业务层获取模型列表
    try {
        auto models = _aiBusiness->getAvailableModels();

        // 4. 构造rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        for (const auto& model : models) {
            auto* modelInfo = response->mutable_result()->add_models();
            modelInfo->set_name(model._name);
            modelInfo->set_desc(model._desc);
        }

        INF("GetModels success: requestId={}, modelCount={}", requestId, models.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void AIServiceImpl::CreateSession(google::protobuf::RpcController* controller,
                                 const chat2Data::AiService::CreateChatSessionRequest* request,
                                 chat2Data::AiService::CreateChatSessionResponse* response,
                                 google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string userId = request->user_id();
    std::string model = request->model();
    std::string sessionType = request->session_type();
    std::string dbConnectionInfo = request->db_connection_info();

    // 3. 校验参数
    if (userId.empty() || model.empty() || sessionType.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_PARAM_INVALID));
        return;
    }

    // 4. 调用业务层创建会话
    try {
        auto result = _aiBusiness->createSession(userId, model, sessionType, dbConnectionInfo);

        // 5. 构造rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->mutable_session()->set_chat_session_id(result._chatSessionId);
        response->mutable_result()->mutable_session()->set_model(result._model);

        INF("CreateSession success: requestId={}, chatSessionId={}", requestId, result._chatSessionId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void AIServiceImpl::GetSessions(google::protobuf::RpcController* controller,
                               const chat2Data::AiService::GetSessionsRequest* request,
                               chat2Data::AiService::GetSessionsResponse* response,
                               google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_PARAM_INVALID));
        return;
    }

    // 4. 调用业务层获取会话列表
    try {
        auto sessions = _aiBusiness->getSessionList(userId);

        // 5. 构造rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        for (const auto& session : sessions) {
            auto* sessionInfo = response->mutable_result()->add_sessioninfo();
            sessionInfo->set_id(session._id);
            sessionInfo->set_model(session._model);
            sessionInfo->set_title(session._title);
            sessionInfo->set_created_at(session._createdAt);
            sessionInfo->set_updated_at(session._updatedAt);
            sessionInfo->set_message_count(session._messageCount);
            sessionInfo->set_first_user_message_content(session._firstUserMessageContent);
            sessionInfo->set_session_type(session._sessionType);
            sessionInfo->set_db_connection_info(session._dbConnectionInfo);
        }

        INF("GetSessions success: requestId={}, sessionCount={}", requestId, sessions.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void AIServiceImpl::GetSessionHistory(google::protobuf::RpcController* controller,
                                      const chat2Data::AiService::GetSessionHistoryRequest* request,
                                      chat2Data::AiService::GetSessionHistoryResponse* response,
                                      google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string userId = request->user_id();
    std::string chatSessionId = request->chat_session_id();

    // 3. 校验参数
    if (userId.empty() || chatSessionId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_PARAM_INVALID));
        return;
    }

    // 4. 调用业务层获取历史消息
    try {
        auto result = _aiBusiness->getSessionHistory(chatSessionId, userId);

        // 5. 检查结果
        if (!result.has_value()) {
            response->set_request_id(requestId);
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_SESSION_NOT_FOUND));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_SESSION_NOT_FOUND));
            return;
        }

        // 6. 构造rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        auto& historyResult = result.value();
        if (!historyResult._fileId.empty()) {
            response->mutable_result()->set_file_id(historyResult._fileId);
        }
        response->mutable_result()->set_session_type(historyResult._sessionType);
        response->mutable_result()->set_db_connection_info(historyResult._dbConnectionInfo);

        for (const auto& msg : historyResult._messages) {
            auto* msgProto = response->mutable_result()->add_messages();
            msgProto->set_id(msg._messageId);
            msgProto->set_role(msg._role);
            msgProto->set_content(msg._content);
            msgProto->set_timestamp(msg._timestamp);
        }

        INF("GetSessionHistory success: requestId={}, chatSessionId={}, messageCount={}",
            requestId, chatSessionId, historyResult._messages.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void AIServiceImpl::DeleteSession(google::protobuf::RpcController* controller,
                                  const chat2Data::AiService::DeleteSessionRequest* request,
                                  chat2Data::AiService::DeleteSessionResponse* response,
                                  google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string userId = request->user_id();
    std::string chatSessionId = request->chat_session_id();

    // 3. 校验参数
    if (userId.empty() || chatSessionId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_PARAM_INVALID));
        return;
    }

    // 4. 调用业务层删除会话
    try {
        bool success = _aiBusiness->deleteSession(chatSessionId, userId);
        if (!success) {
            response->set_request_id(requestId);
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_SESSION_NOT_FOUND));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_SESSION_NOT_FOUND));
            return;
        }

        // 5. 构造rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        INF("DeleteSession success: requestId={}, chatSessionId={}", requestId, chatSessionId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void AIServiceImpl::UpdateSessionFile(google::protobuf::RpcController* controller,
                                      const chat2Data::AiService::UpdateSessionFileRequest* request,
                                      chat2Data::AiService::UpdateSessionFileResponse* response,
                                      google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string userId = request->user_id();
    std::string chatSessionId = request->chat_session_id();
    std::string fileId = request->file_id();

    // 3. 校验参数
    if (userId.empty() || chatSessionId.empty() || fileId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_PARAM_INVALID));
        return;
    }

    // 4. 调用业务层更新会话文件关联
    try {
        bool success = _aiBusiness->updateSessionFile(chatSessionId, userId, fileId);
        if (!success) {
            response->set_request_id(requestId);
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_SESSION_NOT_FOUND));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::AI_SESSION_NOT_FOUND));
            return;
        }

        // 5. 构造rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        INF("UpdateSessionFile success: requestId={}, chatSessionId={}, fileId={}",
            requestId, chatSessionId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void AIServiceImpl::SendMessage(google::protobuf::RpcController* controller,
                                const chat2Data::AiService::SendMessageRequest* request,
                                chat2Data::AiService::SendMessageResponse* response,
                                google::protobuf::Closure* done) {

    brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);

    // 1. 解析请求参数
    std::string requestId = request->request_id();
    std::string userId = request->user_id();
    std::string sessionId = request->session_id();
    std::string chatSessionId = request->chat_session_id();
    std::string chatType = request->chat_type();
    std::string message = request->message();
    std::string fileId = request->file_id();
    auto dbType = request->db_type();
    std::string dbConnectId = request->db_connect_id();
    std::string tableName = request->table_name();

    
    INF("SendMessage called: requestId={}, userId={}, chatSessionId={}, chatType={}",
        requestId, userId, chatSessionId, chatType);

    // 2. 验证参数合法性
    if (chatSessionId.empty() || message.empty() || sessionId.empty()) {
        WRN("SendMessage params invalid: chatSessionId or message or sessionId is empty");
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::AI_PARAM_INVALID));
        response->set_error_msg("参数不完整：chatSessionId、message和sessionId不能为空");
        done->Run();
        return;
    }
    
    // 3. 设置HTTP响应头，开启流式传输
    cntl->http_response().set_content_type("text/event-stream");
    cntl->http_response().SetHeader("Cache-Control", "no-cache");
    cntl->http_response().SetHeader("Connection", "keep-alive");
    cntl->http_response().SetHeader("Access-Control-Allow-Origin", "*");
    cntl->http_response().SetHeader("Access-Control-Allow-Headers", "*");
    response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
    response->mutable_result()->set_done(false);

    // 5. 获取ProgressiveAttachment实现流式响应
    auto progressiveAttachment = cntl->CreateProgressiveAttachment();

    done->Run();

    try{
        // 6. 构建发送消息上下文
        SendMessageContext context;
        context._requestId = requestId;
        context._sessionId = sessionId;
        context._chatSessionId = chatSessionId;
        context._userId = userId;
        context._message = message;
        context._chatType = chatType;
        context._fileId = fileId;
        context._dbType = dbType;
        context._dbConnectId = dbConnectId;
        context._tableNames = {tableName};  // 在数据库场景中，该表名需要重新解析；因为多个表名是通过逗号分隔的

        // 7. 调用AIBusiness::sendMessage，流式响应会通过progressiveAttachment逐步返回
        // 注意：这里不调用done->Run()，因为流式响应会持续进行，done会在连接关闭时自动调用
        _aiBusiness->sendMessage(context, progressiveAttachment);
    } catch (const chat2Data::Chat2DataException& e) {
        std::string errorMsg = "data: " + chat2Data::error2String(e.getErrorCode()) + "\n\n";
        progressiveAttachment->Write(errorMsg.c_str(), errorMsg.size());
        errorMsg = "data: DONE\n\n";
        progressiveAttachment->Write(errorMsg.c_str(), errorMsg.size());
    }
        
}

} // namespace aiService
