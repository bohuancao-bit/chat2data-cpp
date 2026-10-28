#pragma once

#include <string>
#include "emailSender.h"

namespace notifyService {

struct EmailTask {
    std::string _toEmail;         // 收件人邮箱
    std::string _subject;         // 邮件主题
    std::string _content;         // 邮件内容
    EmailSender* _emailSender;    // 邮件发送器对象指针
};

} // namespace notifyService