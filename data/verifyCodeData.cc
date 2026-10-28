#include "verifyCodeData.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <bite_scaffold/redis.h>

namespace userService {

VerifyCodeData::VerifyCodeData(std::shared_ptr<sw::redis::Redis> redis)
    : _redis(redis) {
}

bool VerifyCodeData::saveVerifyCodeToCache(const VerifyCodeInfo& verifyCodeInfo) {
    try {
        std::string key = "verifyCode:" + verifyCodeInfo._codeId;
        Json::Value codeJson;
        codeJson["codeId"] = verifyCodeInfo._codeId;
        codeJson["verifyCode"] = verifyCodeInfo._verifyCode;
        codeJson["email"] = verifyCodeInfo._email;
        codeJson["createTime"] = verifyCodeInfo._createTime;

        auto jsonStr = biteutil::JSON::serialize(codeJson);
        if (!jsonStr.has_value()) {
            WRN("Failed to serialize verify code info for cache");
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            _redis->setex(key, VERIFY_CODE_CACHE_TTL_SECONDS, jsonStr.value());
        }

        INF("Verify code saved to cache: codeId={}, email={}", verifyCodeInfo._codeId, verifyCodeInfo._email);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save verify code to cache: codeId={}, error={}", verifyCodeInfo._codeId, e.what());
        return false;
    }
}

std::optional<VerifyCodeInfo> VerifyCodeData::getVerifyCodeFromCache(const std::string& codeId) {
    try {
        std::string key = "verifyCode:" + codeId;
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("Verify code not found in cache: codeId={}", codeId);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize verify code from cache: codeId={}", codeId);
            return std::nullopt;
        }
        Json::Value codeJson = jsonOpt.value();
        VerifyCodeInfo verifyCodeInfo;
        verifyCodeInfo._codeId = jsonOpt.value()["codeId"].asString();
        verifyCodeInfo._verifyCode = jsonOpt.value()["verifyCode"].asString();
        verifyCodeInfo._email = jsonOpt.value()["email"].asString();
        verifyCodeInfo._createTime = jsonOpt.value()["createTime"].asString();

        INF("Verify code found in cache: codeId={}", codeId);
        return verifyCodeInfo;
    } catch (const std::exception& e) {
        ERR("Failed to get verify code from cache: codeId={}, error={}", codeId, e.what());
        return std::nullopt;
    }
}

bool VerifyCodeData::deleteVerifyCodeFromCache(const std::string& codeId) {
    try {
        std::string key = "verifyCode:" + codeId;
        std::lock_guard<std::mutex> lock(_redisMutex);
        auto res = _redis->del(key);
        if (res == 0) {
            INF("Verify code not found in cache: codeId={}", codeId);
            return false;
        }

        INF("Verify code deleted from cache: codeId={}", codeId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to delete verify code from cache: codeId={}, error={}", codeId, e.what());
        return false;
    }
}

} // namespace userService