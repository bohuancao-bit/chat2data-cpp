#include <bite_scaffold/log.h>
#include "emailSender.h"

namespace notifyService {

EmailSender::EmailSender(const mail_settings& settings) 
    : _settings(settings) {
    
}

EmailSender::~EmailSender() {
}

bool EmailSender::sendEmail(const std::string& to,
                            const std::string& subject,
                            const std::string& content) {
    // 1. 初始化curl的操作句柄
    auto curl = curl_easy_init();
    if (curl == nullptr) {
        ERR("构造CURL操作句柄失败!");
        return false;
    }

    // 2. 设置超时参数
    // 设置连接超时参数(单位:秒)
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    // 设置总超时参数(单位:秒)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // 3. 设置SSL的认证参数---不认证
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // 4. 设置邮箱服务器地址
    auto ret = curl_easy_setopt(curl, CURLOPT_URL, _settings._url.c_str());
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_URL参数失败: {}", curl_easy_strerror(ret));
        curl_easy_cleanup(curl);
        return false;
    }

    // 5. 设置邮箱用户名，注意邮箱用户名即使邮箱号
    ret = curl_easy_setopt(curl, CURLOPT_USERNAME, _settings._username.c_str());
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_USERNAME参数失败: {}", curl_easy_strerror(ret));
        curl_easy_cleanup(curl);
        return false;
    }

    // 6. 设置邮箱的授权码
    ret = curl_easy_setopt(curl, CURLOPT_PASSWORD, _settings._password.c_str());
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_PASSWORD参数失败: {}", curl_easy_strerror(ret));
        curl_easy_cleanup(curl);
        return false;
    }

    // 7. 设置发送者的邮箱号
    ret = curl_easy_setopt(curl, CURLOPT_MAIL_FROM, _settings._from.c_str());
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_MAIL_FROM参数失败: {}", curl_easy_strerror(ret));
        curl_easy_cleanup(curl);
        return false;
    }

    // 8. 设置接收者的邮箱号(验证码和普通邮件都是发送给单个人的，接受者的邮箱号只设置一个即可)
    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, to.c_str());
    ret = curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_MAIL_RCPT参数失败: {}", curl_easy_strerror(ret));
        curl_easy_cleanup(curl);
        return false;
    }

    // 9. 构建邮件正文
    std::stringstream ss = buildEmailBody(to, subject, content);
    if (ss.str().empty()) {
        ERR("构建邮件正文失败!");
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        return false;
    }

    // 10. 设置邮件正文
    ret = curl_easy_setopt(curl, CURLOPT_READDATA, &ss);
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_READDATA参数失败: {}", curl_easy_strerror(ret));
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        return false;
    }

    // 11. 设置邮件正文的回调函数
    ret = curl_easy_setopt(curl, CURLOPT_READFUNCTION, &EmailSender::callback);
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_READFUNCTION参数失败: {}", curl_easy_strerror(ret));
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        return false;
    }

    // 12. 设置上传模式
    ret = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    if (ret != CURLE_OK) {
        ERR("设置CURL的CURLOPT_UPLOAD参数失败: {}", curl_easy_strerror(ret));
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        return false;
    }

    // 13. 执行请求
    ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        ERR("请求邮件服务器失败: {}", curl_easy_strerror(ret));
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        return false;
    }

    // 14. 释放资源，释放接受者链表 以及 libcurl的实例
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    INF("发送邮件成功: {}-{}", to, subject);
    return true;
}

size_t EmailSender::callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    // 将用户邮件内容读取到libcurl指定的buffer空间中
    auto ss = static_cast<std::stringstream*>(userdata);
    ss->read(buffer, size * nitems);
    return ss->gcount();
}

} // namespace notifyService