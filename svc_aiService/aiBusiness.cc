#include "aiBusiness.h"
#include "../common/errorHandler.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>

namespace aiService {

AIBusiness::AIBusiness(std::shared_ptr<ai_chat_sdk::ChatSDK> chatSdk,
                       std::shared_ptr<ChatSessionMgr> chatSessionMgr,
                       std::shared_ptr<biterpc::SvcChannels> svcChannels)
    : _chatSdk(chatSdk)
    , _chatSessionMgr(chatSessionMgr)
    , _svcChannels(svcChannels) {
    INF("AIBusiness initialized");
}

std::vector<ModelInfo> AIBusiness::getAvailableModels() {
    std::vector<ModelInfo> result;
    // 1. 从ChatSDK获取可用模型
    auto models = _chatSdk->getAvailableModels();

    // 2. 转换为ModelInfo结构
    for (const auto& model : models) {
        ModelInfo info;
        info._name = model._modelName;
        info._desc = model._modelDesc;
        result.push_back(info);
    }
    INF("Get available models: count={}", result.size());
    return result;
}

CreateSessionResult AIBusiness::createSession(const std::string& userId,
                                              const std::string& model,
                                              const std::string& sessionType,
                                              const std::string& dbConnectionInfo) {
    // 1. 调用ChatSDK创建会话
    std::string chatSessionId = _chatSdk->createSession(model);
    if (chatSessionId.empty()) {
        ERR("Failed to create session in ChatSDK: userId={}, model={}", userId, model);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SESSION_CREATE_ERROR);
    }

    // 2. 获取当前时间戳
    int64_t now = static_cast<int64_t>(time(nullptr));

    // 3. 构建会话信息
    ChatSessionInfo chatSessionInfo;
    chatSessionInfo._chatSessionId = chatSessionId;
    chatSessionInfo._userId = userId;
    chatSessionInfo._title = "";
    chatSessionInfo._createTime = now;
    chatSessionInfo._updateTime = now;
    chatSessionInfo._messageCount = 0;
    chatSessionInfo._modelName = model;
    chatSessionInfo._fileId = "";
    chatSessionInfo._sessionType = sessionType;
    chatSessionInfo._dbConnectionInfo = dbConnectionInfo;

    // 4. 保存会话信息到MySQL和Redis
    bool success = _chatSessionMgr->saveChatSession(chatSessionInfo);
    if (!success) {
        ERR("Failed to save chat session: chatSessionId={}", chatSessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_SESSION_SAVE_ERROR);
    }

    // 5. 返回结果
    CreateSessionResult result;
    result._chatSessionId = chatSessionId;
    result._model = model;
    INF("Session created: chatSessionId={}, userId={}, model={}", chatSessionId, userId, model);
    return result;
}

std::vector<SessionDetailInfo> AIBusiness::getSessionList(const std::string& userId) {
    std::vector<SessionDetailInfo> result;

    // 1. 获取用户所有会话
    auto chatSessions = _chatSessionMgr->getChatSessionsByUserId(userId);

    for (const auto& chatSession : chatSessions) {
        SessionDetailInfo detail;
        detail._id = chatSession._chatSessionId;
        detail._model = chatSession._modelName;
        detail._title = chatSession._title;
        detail._createdAt = chatSession._createTime;
        detail._updatedAt = chatSession._updateTime;
        detail._messageCount = chatSession._messageCount;
        detail._sessionType = chatSession._sessionType;
        detail._dbConnectionInfo = chatSession._dbConnectionInfo;
        result.push_back(detail);
    }

    // 2. 返回结果
    INF("Get session list: userId={}, count={}", userId, result.size());
    return result;
}

std::optional<SessionDetailInfo> AIBusiness::getSession(const std::string& chatSessionId, const std::string& userId) {
    // 1. 检查会话是否属于该用户
    if (!_chatSessionMgr->isSessionOwnedByUser(chatSessionId, userId)) {
        WRN("ChatSession not owned by user: chatSessionId={}, userId={}", chatSessionId, userId);
        return std::nullopt;
    }

    // 2. 获取会话信息
    auto sessionOpt = _chatSessionMgr->getChatSessionBySessionId(chatSessionId);
    if (!sessionOpt.has_value()) {
        WRN("ChatSession not found: chatSessionId={}", chatSessionId);
        return std::nullopt;
    }

    const auto& chatSession = sessionOpt.value();
    SessionDetailInfo detail;
    detail._id = chatSession._chatSessionId;
    detail._model = chatSession._modelName;
    detail._title = chatSession._title;
    detail._createdAt = chatSession._createTime;
    detail._updatedAt = chatSession._updateTime;
    detail._messageCount = chatSession._messageCount;
    detail._sessionType = chatSession._sessionType;
    detail._dbConnectionInfo = chatSession._dbConnectionInfo;

    INF("Get session: chatSessionId={}, userId={}", chatSessionId, userId);
    return detail;
}

std::optional<SessionHistoryResult> AIBusiness::getSessionHistory(const std::string& chatSessionId, const std::string& userId) {
    // 1. 检查会话是否属于该用户
    if (!_chatSessionMgr->isSessionOwnedByUser(chatSessionId, userId)) {
        WRN("ChatSession not owned by user: chatSessionId={}, userId={}", chatSessionId, userId);
        return std::nullopt;
    }

    // 2. 获取会话信息
    auto sessionOpt = _chatSessionMgr->getChatSessionBySessionId(chatSessionId);
    if (!sessionOpt.has_value()) {
        WRN("ChatSession not found: chatSessionId={}", chatSessionId);
        return std::nullopt;
    }

    // 3. 获取ChatSDK中的历史消息
    auto chatSession = _chatSdk->getSession(chatSessionId);
    if (!chatSession) {
        WRN("ChatSession not found in ChatSDK: chatSessionId={}", chatSessionId);
        return std::nullopt;
    }

    // 4. 构建结果
    SessionHistoryResult result;
    const auto& session = sessionOpt.value();
    result._sessionType = session._sessionType;
    result._dbConnectionInfo = session._dbConnectionInfo;
    if (!session._fileId.empty()) {
        result._fileId = session._fileId;
    }

    for (const auto& msg : chatSession->_messages) {
        HistoryMessageInfo msgInfo;
        msgInfo._messageId = msg._messageId;
        msgInfo._role = msg._role;
        msgInfo._content = msg._content;
        msgInfo._timestamp = msg._timestamp;
        result._messages.push_back(msgInfo);
    }

    INF("Get session history: sessionId={}, messageCount={}", chatSessionId, result._messages.size());
    return result;
}

bool AIBusiness::deleteSession(const std::string& chatSessionId, const std::string& userId) {
    // 1. 删除ChatSDK中的会话
    bool deleted = _chatSdk->deleteSession(chatSessionId);
    if (!deleted) {
        WRN("Failed to delete session from ChatSDK: chatSessionId={}", chatSessionId);
    }

    // 2. 删除MySQL和Redis中的会话元数据
    bool success = _chatSessionMgr->deleteChatSession(chatSessionId, userId);
    if (!success) {
        ERR("Failed to delete chat session: chatSessionId={}", chatSessionId);
        return false;
    }

    INF("ChatSession deleted: chatSessionId={}, userId={}", chatSessionId, userId);
    return true;
}

bool AIBusiness::updateSessionFile(const std::string& chatSessionId, const std::string& userId, const std::string& fileId) {
    // 更新会话关联的文件
    bool success = _chatSessionMgr->associateFileId(chatSessionId, fileId, userId);
    if (!success) {
        ERR("Failed to update session file: chatSessionId={}, fileId={}", chatSessionId, fileId);
        return false;
    }

    INF("ChatSession file updated: chatSessionId={}, fileId={}", chatSessionId, fileId);
    return true;
}

void AIBusiness::sendMessage(const SendMessageContext& context,
                            butil::intrusive_ptr<brpc::ProgressiveAttachment> progressiveAttachment) {
    // 检测当前会话是否属于当前用户
    if (!_chatSessionMgr->isSessionOwnedByUser(context._chatSessionId, context._userId)) {
        ERR("ChatSession not owned by user: chatSessionId={}, userId={}", context._chatSessionId, context._userId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_CHATSESSION_NOT_OWNED_BY_USER);
    }

    // 开启独立线程处理消息发送，避免RPC调用长时间阻塞
    std::thread([this, context, progressiveAttachment]() {
        try {
            // 定义writeChunk回调，用于将模型回复分块推送给客户端
            // 注意：客户端自己会组织SSE格式，这里直接发送原始数据块
            auto writeChunk = [progressiveAttachment](const std::string& chunk, bool done) {
                if (chunk.empty() && !done) {
                    return;
                }

                // 直接发送数据，客户端会自己组织成SSE格式
                // 数据块以换行分隔，最后一块以[DONE]结束标记
                std::string data;
                if (done) {
                    data = "data: [DONE]\n\n";
                } else {
                    data = "data: " + chunk + "\n\n";
                }
                progressiveAttachment->Write(data.c_str(), data.size());
            };

            // 创建AIMessageHandler并调用sendMessage完成业务逻辑
            AIMessageHandler handler(_chatSdk, _chatSessionMgr, _svcChannels);
            handler.sendMessage(context, writeChunk);
        } catch (const std::exception& e) {
            ERR("Exception in sendMessage thread: {}", e.what());
            // 发送错误信息给客户端
            std::string errorData = "error: " + std::string(e.what()) + "\n[DONE]";
            progressiveAttachment->Write(errorData.c_str(), errorData.size());
        }
    }).detach();
}

} // namespace aiService
