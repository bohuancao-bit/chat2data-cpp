#pragma once
#include <memory>
#include <string>
#include "emailSender.h"
#include "verifyCodeEmailSender.h"
#include "normalEmailSender.h"
#include "emailWorker.h"

namespace notifyService {

class NotifyBusiness {
public:
    explicit NotifyBusiness(const mail_settings& mailSettings);
    ~NotifyBusiness();
    // 启动发送邮件线程
    void start();
    // 停止发送邮件线程
    void stop();
    // 发送验证码邮件
    void sendVerifyCodeEmail(const std::string& to, const std::string& code);
    // 发送普通邮件
    void sendNormalEmail(const std::string& to, const std::string& subject, const std::string& content);

private:
    std::unique_ptr<VerifyCodeEmailSender> _verifyCodeEmailSender;  // 验证码邮件发送器对象指针
    std::unique_ptr<NormalEmailSender> _normalEmailSender;          // 普通邮件发送器对象指针
    EmailWorker _emailWorker;                                       // 发送邮件异步线程对象
};

} // namespace notifyService