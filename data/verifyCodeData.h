#pragma once

#include <memory>
#include <string>
#include <optional>
#include <mutex>
#include <sw/redis++/redis.h>
#include "../svc_userService/common.h"

namespace userService {

class VerifyCodeData {
public:
    /**
     * @brief  构造函数，根据配置初始化Redis连接
     * @param  redis  Redis数据库连接
     */
    explicit VerifyCodeData(std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief  保存验证码信息到Redis缓存
     * @param  verifyCodeInfo  验证码信息结构体
     * @return 成功返回true，失败返回false
     */
    bool saveVerifyCodeToCache(const VerifyCodeInfo& verifyCodeInfo);

    /**
     * @brief  从Redis缓存获取验证码信息
     * @param  codeId  验证码ID
     * @return 存在返回验证码信息，不存在返回std::nullopt
     */
    std::optional<VerifyCodeInfo> getVerifyCodeFromCache(const std::string& codeId);

    /**
     * @brief  从Redis缓存删除验证码信息
     * @param  codeId  验证码ID
     * @return 成功返回true，失败返回false
     */
    bool deleteVerifyCodeFromCache(const std::string& codeId);

private:
    std::shared_ptr<sw::redis::Redis> _redis;
    std::mutex _redisMutex;
    const static int VERIFY_CODE_CACHE_TTL_SECONDS = 5 * 60;  // 验证码缓存过期时间为5分钟
};

} // namespace userService