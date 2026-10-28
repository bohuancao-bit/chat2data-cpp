#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "../data/chatSessionData.h"
#include "common.h"

namespace aiService {

class ChatSessionMgr {
public:
    ChatSessionMgr(std::shared_ptr<ChatSessionData> chatSessionData);

    // 检测指定聊天会话是否属于指定用户
    bool isSessionOwnedByUser(const std::string& chatSessionId, const std::string& userId);

    // 保存或更新聊天会话
    bool saveChatSession(const ChatSessionInfo& chatSessionInfo);

    // 通过用户Id获取用户所有聊天会话（从MySQL，不走缓存）
    std::vector<ChatSessionInfo> getChatSessionsByUserId(const std::string& userId);

    // 通过聊天会话Id获取指定聊天会话元数据
    std::optional<ChatSessionInfo> getChatSessionBySessionId(const std::string& chatSessionId);

    // 通过聊天会话Id删除指定聊天会话（需校验用户权限）
    bool deleteChatSession(const std::string& chatSessionId, const std::string& userId);

    // 将文件Id关联到聊天会话
    bool associateFileId(const std::string& chatSessionId, const std::string& fileId, const std::string& userId);

private:
    std::shared_ptr<ChatSessionData> _chatSessionData;  // 数据层实例指针
};

} // namespace aiService
