#pragma once

#include <string>
#include <memory>
#include <ctime>
#include <gflags/gflags.h>
#include <bite_scaffold/rpc.h>
#include "../data/userData.h"
#include "../data/verifyCodeData.h"
#include "sessionManager.h"

// 声明gflags变量（在main.cc中定义）
DECLARE_string(notify_service);
DECLARE_string(db_service);


namespace userService {

class UserBusiness {
public:
    UserBusiness(std::shared_ptr<SessionManager> sessionManager,
                 std::shared_ptr<VerifyCodeData> verifyCodeData,
                 std::shared_ptr<UserData> userData,
                 biterpc::SvcChannels::ptr svcChannels);

    // 检查昵称是否唯一
    bool isNicknameUnique(const std::string& nickname);
    // 检查邮箱是否唯一
    bool isEmailUnique(const std::string& email);
    // 注册用户
    std::string registerUser(const std::string& nickname,
                            const std::string& email,
                            const std::string& password);
    // 昵称(邮箱)+密码登录
    std::string loginWithPassword(const std::string& username,
                                  const std::string& password);
    // 生成验证码
    std::string getVerifyCode(const std::string& email);
    // 验证码登录
    std::string loginWithVerifyCode(const std::string& email,
                                   const std::string& codeId,
                                   const std::string& verifyCode);
    // 会话登录
    bool loginWithSession(const std::string& sessionId);
    // 退出登录
    bool logout(const std::string& sessionId);
    // 获取用户信息
    UserInfo getUserInfo(const std::string& sessionId);

    // 检查会话是否有效
    bool isSessionValid(const std::string& sessionId, std::string& userId);

private:
    // 根据用户ID获取用户信息
    std::optional<UserInfo> getUserByUserId(const std::string& userId);
    // 根据昵称获取用户信息
    std::optional<UserInfo> getUserByNickname(const std::string& nickname);
    // 根据邮箱获取用户信息
    std::optional<UserInfo> getUserByEmail(const std::string& email);
    // 加密密码
    std::string encryptPassword(const std::string& password);
    // 验证密码
    bool verifyPassword(const std::string& password, const std::string& encrypted);
    // 删除用户创建的所有数据库连接
    void deleteUserAllConns(const std::string& userId);

private:
    std::shared_ptr<SessionManager> _sessionManager;      // 会话管理器指针
    std::shared_ptr<VerifyCodeData> _verifyCodeData;      // 验证码数据指针
    std::shared_ptr<UserData> _userData;                  // 用户数据指针
    biterpc::SvcChannels::ptr _svcChannels;   // 服务通道指针
};

} // namespace userService