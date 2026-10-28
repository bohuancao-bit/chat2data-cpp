
#include <exception>
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>
#include "userServiceImpl.h"
#include "userBusiness.h"
#include "../common/errorHandler.h"

namespace userService {

UserServiceImpl::UserServiceImpl(std::shared_ptr<UserBusiness> userBusiness)
    : _userBusiness(userBusiness) {
}

UserServiceImpl::~UserServiceImpl() {
}

void UserServiceImpl::ValidNickname(google::protobuf::RpcController* controller,
                                    const chat2Data::userService::ValidNicknameRequest* request,
                                    chat2Data::userService::ValidNicknameResponse* response,
                                    google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string nickname = request->nickname();

    // 2. 参数校验
    if (nickname.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_NICKNAME_EMPTY));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_NICKNAME_EMPTY));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层检测昵称是否唯一
        bool isUnique = _userBusiness->isNicknameUnique(nickname);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        if (isUnique) {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        } else {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_NICKNAME_EXIST));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_NICKNAME_EXIST));
        }
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::ValidEmail(google::protobuf::RpcController* controller,
                                 const chat2Data::userService::ValidEmailRequest* request,
                                 chat2Data::userService::ValidEmailResponse* response,
                                 google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string email = request->email();

    // 2. 参数校验
    if (email.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_EMAIL_EMPTY));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_EMAIL_EMPTY));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层检测邮箱是否唯一
        bool isUnique = _userBusiness->isEmailUnique(email);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        if (isUnique) {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        } else {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_EMAIL_EXIST));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_EMAIL_EXIST));
        }
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::UserRegister(google::protobuf::RpcController* controller,
                                  const chat2Data::userService::UserRegisterRequest* request,
                                  chat2Data::userService::UserRegisterResponse* response,
                                  google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string nickname = request->nickname();
    std::string password = request->password();
    std::string email = request->email();

    // 2. 参数校验
    if (nickname.empty() || password.empty() || email.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_REGISTER_PARAM_ERROR));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_REGISTER_PARAM_ERROR));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层注册用户
        _userBusiness->registerUser(nickname, email, password);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::SessionLogin(google::protobuf::RpcController* controller,
                                   const chat2Data::userService::SessionLoginRequest* request,
                                   chat2Data::userService::SessionLoginResponse* response,
                                   google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();

    // 2. 参数校验
    if (sessionId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_SESSION_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_SESSION_INVALID));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层进行会话登录
        bool loginResult = _userBusiness->loginWithSession(sessionId);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        if (loginResult) {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        } else {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_SESSION_LOGIN_FAILED));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_SESSION_LOGIN_FAILED));
        }
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::PasswdLogin(google::protobuf::RpcController* controller,
                                  const chat2Data::userService::PasswdLoginRequest* request,
                                  chat2Data::userService::PasswdLoginResponse* response,
                                  google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string username = request->username();
    std::string password = request->password();

    // 2. 参数校验
    if (username.empty() || password.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_PASSWORD_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_PASSWORD_INVALID));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层进行密码登录
        std::string sessionId = _userBusiness->loginWithPassword(username, password);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->set_session_id(sessionId);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::GetCode(google::protobuf::RpcController* controller,
                              const chat2Data::userService::GetCodeRequest* request,
                              chat2Data::userService::GetCodeResponse* response,
                              google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string email = request->email();

    // 2. 参数校验
    if (email.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_EMAIL_EMPTY));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_EMAIL_EMPTY));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层获取验证码
        std::string codeId = _userBusiness->getVerifyCode(email);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->set_code_id(codeId);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::VcodeLogin(google::protobuf::RpcController* controller,
                                 const chat2Data::userService::VcodeLoginRequest* request,
                                 chat2Data::userService::VcodeLoginResponse* response,
                                 google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string email = request->email();
    std::string verifyCode = request->verify_code();
    std::string codeId = request->code_id();

    // 2. 参数校验
    if (email.empty() || verifyCode.empty() || codeId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_PASSWORD_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_PASSWORD_INVALID));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层进行验证码登录
        std::string sessionId = _userBusiness->loginWithVerifyCode(email, codeId, verifyCode);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->set_session_id(sessionId);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::Logout(google::protobuf::RpcController* controller,
                            const chat2Data::userService::LogoutRequest* request,
                            chat2Data::userService::LogoutResponse* response,
                            google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();

    // 2. 参数校验
    if (sessionId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_SESSION_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_SESSION_INVALID));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层退出登录
        bool logoutResult = _userBusiness->logout(sessionId);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        if (logoutResult) {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        } else {
            response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_LOGOUT_FAILED));
            response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_LOGOUT_FAILED));
        }
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::GetUserInfo(google::protobuf::RpcController* controller,
                                 const chat2Data::userService::GetUserInfoRequest* request,
                                 chat2Data::userService::GetUserInfoResponse* response,
                                 google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();

    // 2. 参数校验
    if (sessionId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_GET_USER_INFO_PARAM_ERROR));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_GET_USER_INFO_PARAM_ERROR));
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层获取用户信息
        UserInfo userInfo = _userBusiness->getUserInfo(sessionId);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        chat2Data::userService::UserInfo* resultUserInfo = response->mutable_result()->mutable_user_info();
        resultUserInfo->set_user_id(userInfo._userId);
        resultUserInfo->set_nickname(userInfo._nickname);
        resultUserInfo->set_email(userInfo._email);
        INF("Get user info: userId={}, nickname={}, email={}", userInfo._userId, userInfo._nickname, userInfo._email);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void UserServiceImpl::IsSessionValid(google::protobuf::RpcController* controller,
                                     const chat2Data::userService::IsSessionValidRequest* request,
                                     chat2Data::userService::IsSessionValidResponse* response,
                                     google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 1. 解析RPC请求中参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();

    // 2. 参数校验
    if (sessionId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::USER_SESSION_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::USER_SESSION_INVALID));
        response->set_is_valid(false);
        return;
    }

    try {
        // 3. 业务逻辑处理：调用业务逻辑层检查会话是否有效
        std::string userId;
        bool isValid = _userBusiness->isSessionValid(sessionId, userId);

        // 4. 设置RPC响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->set_is_valid(isValid);
        response->set_user_id(userId);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
        response->set_is_valid(false);
    }
}

} // namespace userService