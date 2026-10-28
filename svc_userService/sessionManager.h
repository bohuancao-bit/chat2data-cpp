#pragma once

#include <string>
#include <memory>
#include "../data/sessionData.h"

namespace userService {

class SessionManager {
public:
    explicit SessionManager(SessionData* sessionData);

    // 新建会话
    std::string createSession(const std::string& userId);
    // 删除会话
    bool deleteSession(const std::string& sessionId);
    // 根据会话id获取用户id
    std::string getUserIdBySessionId(const std::string& sessionId);

private:
    SessionData* _sessionData;   // 操作数据库中会话信息的指针
};

} // namespace userService