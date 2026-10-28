#include "utils.h"
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>

namespace chat2Data {

static std::atomic<uint16_t> uuidCounter{0};

std::string Utils::generateUuid() {
    // 生成一个由16位随机字符组成的字符串作为唯一ID
    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen(rd());
    thread_local static std::uniform_int_distribution<int> dis(0, 255);

    // 1. 生成6个0~255之间的随机数字(1字节-转换为16进制字符)--生成12位16进制字符
    uint8_t bytes[6];
    for (int i = 0; i < 6; ++i) {
        bytes[i] = static_cast<uint8_t>(dis(gen));
    }

    // 2. 生成计数
    uint16_t counter = uuidCounter.fetch_add(1);

    // 3. 构建uuid， 格式：038d-838e161f-0001
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(2) << static_cast<int>(bytes[0])
        << std::setw(2) << static_cast<int>(bytes[1])
        << '-'
        << std::setw(2) << static_cast<int>(bytes[2])
        << std::setw(2) << static_cast<int>(bytes[3])
        << std::setw(2) << static_cast<int>(bytes[4])
        << std::setw(2) << static_cast<int>(bytes[5])
        << '-'
        << std::setw(4) << static_cast<int>(counter);

    return oss.str();
}

static const char* BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Utils::base64Encode(const std::vector<char>& data) {
    std::string result;
    int i = 0;
    int j = 0;

    while (i < static_cast<int>(data.size())) {
        unsigned char char1 = static_cast<unsigned char>(data[i++]);
        result += BASE64_CHARS[char1 >> 2];

        if (i >= static_cast<int>(data.size())) {
            unsigned char char2 = 0;
            result += BASE64_CHARS[((char1 & 0x03) << 4) | (char2 >> 4)];
            result += "==";
            break;
        }

        unsigned char char2 = static_cast<unsigned char>(data[i++]);
        result += BASE64_CHARS[((char1 & 0x03) << 4) | (char2 >> 4)];

        if (i >= static_cast<int>(data.size())) {
            result += BASE64_CHARS[((char2 & 0x0F) << 2)];
            result += "=";
            break;
        }

        unsigned char char3 = static_cast<unsigned char>(data[i++]);
        result += BASE64_CHARS[((char2 & 0x0F) << 2) | (char3 >> 6)];
        result += BASE64_CHARS[char3 & 0x3F];
    }

    return result;
}

std::string Utils::base64Encode(const std::string& input) {
    return base64Encode(std::vector<char>(input.begin(), input.end()));
}

} // namespace chat2Data
