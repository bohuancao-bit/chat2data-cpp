#pragma once
#include <sstream>
#include "emailSender.h"


namespace notifyService {

// 普通邮件发送器类
class NormalEmailSender : public EmailSender {
public:
    explicit NormalEmailSender(const mail_settings& settings);
    ~NormalEmailSender() override;
    // 构建普通邮件正文
    std::stringstream buildEmailBody(const std::string& to, const std::string& subject, const std::string& content) override;
};

} // namespace notifyService