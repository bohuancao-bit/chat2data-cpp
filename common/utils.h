#pragma once

#include <string>
#include <vector>

namespace chat2Data {

class Utils {
public:
    /**
     * @brief  生成UUID字符串
     * @return 返回格式为xxxx-xxxxxxxx-yyyy的UUID字符串，例如：fd7c-91ceb230-0000
     */
    static std::string generateUuid();

    /**
     * @brief  对二进制数据进行Base64编码
     * @param data 二进制数据向量
     * @return 编码后的Base64字符串
     */
    static std::string base64Encode(const std::vector<char>& data);

    /**
     * @brief  对字符串进行Base64编码
     * @param input 输入字符串
     * @return 编码后的Base64字符串
     */
    static std::string base64Encode(const std::string& input);
};

} // namespace chat2Data
