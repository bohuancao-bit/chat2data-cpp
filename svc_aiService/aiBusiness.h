#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <thread>
#include <bite_scaffold/rpc.h>
#include <ai_chat_sdk/ChatSDK.h>
#include "chatSessionMgr.h"
#include "common.h"
#include "aimessageHandler.h"
#include "../proto/protoCode/aiService.pb.h"

namespace aiService {

class AIBusiness {
public:
    AIBusiness(std::shared_ptr<ai_chat_sdk::ChatSDK> chatSdk,
               std::shared_ptr<ChatSessionMgr> chatSessionMgr,
               std::shared_ptr<biterpc::SvcChannels> svcChannels);

    // 获取可用的模型列表
    std::vector<ModelInfo> getAvailableModels();

    // 新建聊天会话
    CreateSessionResult createSession(const std::string& userId,
                                     const std::string& model,
                                     const std::string& sessionType,
                                     const std::string& dbConnectionInfo);

    // 获取用户会话列表
    std::vector<SessionDetailInfo> getSessionList(const std::string& userId);

    // 获取指定会话元数据
    std::optional<SessionDetailInfo> getSession(const std::string& chatSessionId, const std::string& userId);

    // 获取指定会话历史消息
    std::optional<SessionHistoryResult> getSessionHistory(const std::string& chatSessionId, const std::string& userId);

    // 删除指定会话
    bool deleteSession(const std::string& chatSessionId, const std::string& userId);

    // 更新会话关联的文件
    bool updateSessionFile(const std::string& chatSessionId, const std::string& userId, const std::string& fileId);

    // 发送消息
    // context 发送消息上下文，包含所有必要的请求参数
    // progressiveAttachment 流式响应器，用于实现SSE流式传输
    void sendMessage(const SendMessageContext& context,
                     butil::intrusive_ptr<brpc::ProgressiveAttachment> progressiveAttachment);
                     
private:
    std::shared_ptr<ai_chat_sdk::ChatSDK> _chatSdk;           // ChatSDK实例
    std::shared_ptr<ChatSessionMgr> _chatSessionMgr;         // 会话管理器
    std::shared_ptr<biterpc::SvcChannels> _svcChannels;      // RPC通信
};

} // namespace aiService
