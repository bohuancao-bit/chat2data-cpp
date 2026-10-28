#include <random>
#include <crypt.h>
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>

#include "userBusiness.h"
#include "../proto/protoCode/notifyService.pb.h"
#include "../proto/protoCode/dbService.pb.h"
#include "../common/utils.h"
#include "../common/errorHandler.h"



namespace userService {

UserBusiness::UserBusiness(std::shared_ptr<SessionManager> sessionManager,
                          std::shared_ptr<VerifyCodeData> verifyCodeData,
                          std::shared_ptr<UserData> userData,
                          biterpc::SvcChannels::ptr svcChannels)
    : _sessionManager(sessionManager)
    , _verifyCodeData(verifyCodeData)
    , _userData(userData)
    , _svcChannels(svcChannels) {
}

// 检测用户昵称是否唯一
bool UserBusiness::isNicknameUnique(const std::string& nickname) {
    return !_userData->existNicknameInDb(nickname);
}
// 检测用户邮箱是否唯一
bool UserBusiness::isEmailUnique(const std::string& email) {
    return !_userData->existEmailInDb(email);
}
// 注册用户
std::string UserBusiness::registerUser(const std::string& nickname,
                                       const std::string& email,
                                       const std::string& password) {
    // 1. 创建UserInfo对象
    UserInfo user;
    user._userId = chat2Data::Utils::generateUuid();
    user._nickname = nickname;
    user._email = email;
    user._password = encryptPassword(password);
    user._status = UserStatus::Offline;
    // 2. 保存用户信息到数据库
    bool saveResult = _userData->saveUserToDb(user);
    if (!saveResult) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SAVE_USER_INFO_FAILED);
    }

    INF("User registered: userId={}, nickname={}, email={}", user._userId, user._nickname, user._email);
    return user._userId;
}
// 昵称(密码)登录
std::string UserBusiness::loginWithPassword(const std::string& username,
                                           const std::string& password) {
    // 1. 用户通过昵称登录
    std::optional<UserInfo> userOpt = getUserByNickname(username);

    // 2. 用户通过邮箱登录
    if (!userOpt.has_value()) {
        userOpt = getUserByEmail(username);
    }

    if (!userOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 3. 提取用户信息
    UserInfo user = userOpt.value();
    // 4. 密码校验
    if (!verifyPassword(password, user._password)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_LOGIN_FAILED);
    }

    // 5. 更新用户状态
    user._status = UserStatus::Online;
    // 6. 更新MySQL和Redis
    _userData->saveUserToDb(user);
    _userData->deleteUserCache(user._userId, user._nickname, user._email);

    // 7. 新建会话并返回
    std::string sessionId = _sessionManager->createSession(user._userId);

    INF("User logged in with password: userId={}, sessionId={}", user._userId, sessionId);
    return sessionId;
}

std::string UserBusiness::getVerifyCode(const std::string& email) {
    // 1. 创建验证码信息
    VerifyCodeInfo codeInfo;
    codeInfo._codeId = chat2Data::Utils::generateUuid();
    codeInfo._email = email;
    codeInfo._verifyCode = biteutil::Random::code(6, biteutil::DIGIT);
    auto now = std::time(nullptr);
    codeInfo._createTime = std::to_string(now);

    // 2. 保存验证码信息到缓存
    bool saveResult = _verifyCodeData->saveVerifyCodeToCache(codeInfo);
    if (!saveResult) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SAVE_VERIFY_CODE_FAILED);
    }

    // 3. 获取NotifyService的channel
    auto channel = _svcChannels->getNode(FLAGS_notify_service);
    if (!channel) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }

    // 4. 创建发送验证码的rpc请求
    chat2Data::notifyService::SendVerifyCodeRequest request;
    request.set_request_id(codeInfo._codeId);
    request.set_email(email);
    request.set_code(codeInfo._verifyCode);

    // 5. 创建通知子服务的rpc客户端
    chat2Data::notifyService::SendVerifyCodeResponse response;
    brpc::Controller controller;
    chat2Data::notifyService::NotifyService_Stub stub(channel.get());

    // 6. 发起发送验证码的rpc调用
    stub.SendVerifyCode(&controller, &request, &response, nullptr);

    // 7. 检测rpc调用是否成功
    if (controller.Failed()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
    if (response.error_code() != 0) {
        throw chat2Data::Chat2DataException(static_cast<chat2Data::ErrorCode>(response.error_code()));
    }

    INF("Verification code generated: codeId={}, email={}, code={}", codeInfo._codeId, email, codeInfo._verifyCode);
    return codeInfo._codeId;
}

// 验证码登录
std::string UserBusiness::loginWithVerifyCode(const std::string& email,
                                              const std::string& codeId,
                                              const std::string& verifyCode) {
    // 1. 从Redis中获取验证码
    auto codeInfoOpt = _verifyCodeData->getVerifyCodeFromCache(codeId);
    if (!codeInfoOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_VERIFY_CODE_EXPIRED);
    }

    // 2. 提取验证码信息
    const VerifyCodeInfo& codeInfo = codeInfoOpt.value();

    // 3. 验证码信息校验
    if (codeInfo._email != email || 
        codeInfo._codeId != codeId ||
        codeInfo._verifyCode != verifyCode) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_VERIFY_CODE_ERROR);
    }
    // 4. 通过用户邮箱获取用户信息
    std::optional<UserInfo> userOpt = getUserByEmail(email);

    if (!userOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 5. 提取用户信息结构
    UserInfo user = userOpt.value();
    // 6. 修改用户状态
    user._status = UserStatus::Online;
    // 7. 更新数据库
    _userData->saveUserToDb(user);
    _userData->deleteUserCache(user._userId, user._nickname, user._email);

    // 8. 新建会话
    std::string sessionId = _sessionManager->createSession(user._userId);
    // 9. 删除Redis中已使用的验证码
    _verifyCodeData->deleteVerifyCodeFromCache(codeId);

    INF("User logged in with verify code: userId={}, sessionId={}", user._userId, sessionId);
    return sessionId;
}
// 会话登录
bool UserBusiness::loginWithSession(const std::string& sessionId) {
    // 1. 通过会话Id获取会话信息
    std::string userId = _sessionManager->getUserIdBySessionId(sessionId);

    // 2. 通过会话Id获取用户信息
    std::optional<UserInfo> userOpt = getUserByUserId(userId);

    if (!userOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 3. 从结果中提取用户信息
    UserInfo user = userOpt.value();
    // 4. 更新用户状态为登录
    user._status = UserStatus::Online;
    // 5. 更新MySQL和Redis
    _userData->saveUserToDb(user);
    _userData->deleteUserCache(user._userId, user._nickname, user._email);

    INF("User logged in with session: userId={}, sessionId={}", user._userId, sessionId);
    return true;
}
// 退出登录
bool UserBusiness::logout(const std::string& sessionId) {
    // 1. 通过会话Id获取用户Id
    std::string userId = _sessionManager->getUserIdBySessionId(sessionId);

    // 2. 通过用户Id获取用户信息
    std::optional<UserInfo> userOpt = getUserByUserId(userId);

    if (!userOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 3. 从查询结果中提取用户信息结构
    UserInfo user = userOpt.value();
    // 4. 修改用户状态为离线
    user._status = UserStatus::Offline;
    // 5. 更新MySQL和Redis
    _userData->saveUserToDb(user);
    _userData->deleteUserCache(user._userId, user._nickname, user._email);

    // 6. 删除会话
    _sessionManager->deleteSession(sessionId);

    // 7. 删除用户创建的所有数据库连接
    deleteUserAllConns(user._userId);

    INF("User logged out: userId={}, sessionId={}", user._userId, sessionId);
    return true;
}

void UserBusiness::deleteUserAllConns(const std::string& userId) {
    // 1. 获取数据库服务通道
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        WRN("Failed to get DatabaseService channel when deleting user all connections, userId: {}", userId);
        return;
    }

    // 2. 构造RPC请求
    std::string requestId = chat2Data::Utils::generateUuid();
    INF("Request ID: {}", requestId);
    chat2Data::DatabaseService::DeleteUserAllConnRequest request;
    request.set_request_id(requestId);
    request.set_user_id(userId);

    // 3. 创建RPC客户端
    chat2Data::DatabaseService::DeleteUserAllConnResponse response;
    brpc::Controller controller;
    chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());

    // 4. 发送RPC请求
    dbStub.DeleteUserAllConn(&controller, &request, &response, nullptr);

    // 5. 检测RPC请求是否成功执行
    if (controller.Failed()) {
        WRN("DeleteUserAllConn RPC failed: {}, userId: {}", controller.ErrorText(), userId);
        return;
    }
    if (response.error_code() != 0) {
        WRN("DeleteUserAllConn failed with error: {}, userId: {}", response.error_msg(), userId);
        return;
    }

    INF("Successfully deleted all database connections for user: userId={}", userId);
}

UserInfo UserBusiness::getUserInfo(const std::string& sessionId) {
    // 1. 通过会话id获取用户id
    std::string userId = _sessionManager->getUserIdBySessionId(sessionId);

    INF("Session userId: {}", userId);
    if (userId.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_INVALID);
    }

    // 2. 通过用户id获取用户信息
    std::optional<UserInfo> userOpt = getUserByUserId(userId);

    if (!userOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }

    // 3. 结果返回
    INF("Get user info: userId={}", userId);
    return userOpt.value();
}

// 检查会话是否有效
bool UserBusiness::isSessionValid(const std::string& sessionId, std::string& userId) {
    // 1. 通过会话id获取userId
    userId = _sessionManager->getUserIdBySessionId(sessionId);

    // 2. 通过userId获取用户信息
    std::optional<UserInfo> userOpt = getUserByUserId(userId);
    if (!userOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_NOT_FOUND);
    }
    // 3. 检查用户状态是否为在线
    return userOpt.value()._status == UserStatus::Online;
}

std::string UserBusiness::encryptPassword(const std::string& password) {
    // 1. 生成随机盐值
    std::random_device rd;
    thread_local static std::mt19937 gen(rd());
    thread_local static std::uniform_int_distribution<int> dis(0, 255);
    unsigned char saltBytes[16];
    for (int i = 0; i < 16; ++i) {
        saltBytes[i] = static_cast<unsigned char>(dis(gen));
    }

    // 2. 对盐值进行Base64编码
    std::string saltBase64 = chat2Data::Utils::base64Encode(
        std::vector<char>(reinterpret_cast<char*>(saltBytes), reinterpret_cast<char*>(saltBytes + 16)));

    // 3. 在盐值上拼接$2b$10$
    std::string saltSetting = "$2b$10$" + saltBase64.substr(0, 22);

    // 4. 对密码进行加密
    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char* hashed = crypt_r(password.c_str(), saltSetting.c_str(), &data);
    if (hashed == nullptr || hashed[0] == '*') {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_PASSWORD_ENCRYPT_ERROR);
    }

    return std::string(hashed);
}

bool UserBusiness::verifyPassword(const std::string& password, const std::string& encrypted) {
    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    // 使用用户输入的密码 + 数据库中加密之后的哈希字符串来重新加密
    char* hashed = crypt_r(password.c_str(), encrypted.c_str(), &data);
    if (hashed == nullptr || hashed[0] == '*') {
        return false;
    }

    // 使用数据库中存储的密码加密之后的哈希字符串直接和重新加密后的字符串进行比较，判断哈希值是否一致
    return strcmp(hashed, encrypted.c_str()) == 0;
}

std::optional<UserInfo> UserBusiness::getUserByUserId(const std::string& userId) {
    std::optional<UserInfo> userOpt = _userData->getUserFromCacheByUserId(userId);
    if (!userOpt.has_value()) {
        userOpt = _userData->getUserByUserIdFromDb(userId);
        if (userOpt.has_value()) {
            _userData->saveUserToCache(userOpt.value());
        }
    }
    return userOpt;
}

std::optional<UserInfo> UserBusiness::getUserByNickname(const std::string& nickname) {
    std::optional<UserInfo> userOpt = _userData->getUserFromCacheByNickname(nickname);
    if (!userOpt.has_value()) {
        userOpt = _userData->getUserByNicknameFromDb(nickname);
        if (userOpt.has_value()) {
            _userData->saveUserToCache(userOpt.value());
        }
    }
    return userOpt;
}

std::optional<UserInfo> UserBusiness::getUserByEmail(const std::string& email) {
    std::optional<UserInfo> userOpt = _userData->getUserFromCacheByEmail(email);
    if (!userOpt.has_value()) {
        userOpt = _userData->getUserByEmailFromDb(email);
        if (userOpt.has_value()) {
            _userData->saveUserToCache(userOpt.value());
        }
    }
    return userOpt;
}

} // namespace userService