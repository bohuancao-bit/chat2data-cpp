#include "verifyCodeEmailSender.h"

namespace notifyService {

VerifyCodeEmailSender::VerifyCodeEmailSender(const mail_settings& settings)
    : EmailSender(settings) {
}

VerifyCodeEmailSender::~VerifyCodeEmailSender() {
}

std::stringstream VerifyCodeEmailSender::buildEmailBody(const std::string& to, const std::string& subject, const std::string& content) {
    std::stringstream ss;
    ss << "Subject: 验证码\r\n";
    ss << "Content-Type: text/html\r\n";
    ss << "\r\n";
    ss << "<html><body><p>你的验证码: <b>" << content << "</b></p>"
       << "<p>验证码将在5分钟后失效.</p></body></html>\r\n";
    return ss;
}

} // namespace notifyService