#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <curl/curl.h>

namespace notifyService {

// 邮件配置结构
struct mail_settings {
    std::string _username;     // 邮箱用户名(传递用户邮箱)
    std::string _password;     // 授权码
    std::string _url;          // 邮箱服务器地址
    std::string _from;         // 发送者邮箱号
};

class EmailSender {
public:
    explicit EmailSender(const mail_settings& settings);
    virtual ~EmailSender();
    // 构建邮件正文
    virtual std::stringstream buildEmailBody(const std::string& to,
                                const std::string& subject,
                                const std::string& content) = 0;
    // 发送邮件(内部实现发送邮件的完整的流程)
    bool sendEmail(const std::string& to, const std::string& subject, const std::string& content);

private:
    // libcurl需要的回调函数(将邮件内容(userdata)读取到libcurl指定的内存空间，即buffer)
    static size_t callback(char* buffer, size_t size, size_t nitems, void* userdata);

protected:
    mail_settings _settings;    // 邮箱配置
};

} // namespace notifyService