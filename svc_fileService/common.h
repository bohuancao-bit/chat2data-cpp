#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace fileService {

struct FileInfo {
    std::string _fileId;          // 文件ID
    std::string _fileName;        // 文件名
    int64_t _fileSize;            // 文件大小
    int64_t _uploadTime;          // 上传时间
    std::string _fileExt;         // 文件扩展名
    std::string _fdfsFileId;      // FastDFS文件ID
    std::string _userId;          // 用户ID
    std::string _chatSessionId;   // 聊天会话ID
};

struct MysqlConfig {
    std::string _host;                 // MySQL主机地址
    std::string _user;                 // MySQL用户名
    std::string _passwd;               // MySQL密码
    std::string _db;                   // MySQL数据库名
    unsigned int _port;               // MySQL端口号
    std::string _cset;                 // MySQL字符集
    unsigned int _connectionPoolSize; // MySQL连接池大小
};

struct RedisConfig {
    std::string _host;                 // Redis主机地址
    int _port;                         // Redis端口号
    std::string _passwd;               // Redis密码
    int _db;                           // Redis数据库索引
    size_t _connectionPoolSize;        // Redis连接池大小
};

struct FastDFSConfig {
    std::vector<std::string> _trackers; // FastDFS tracker服务器地址
    int _connectTimeout;               // FastDFS连接超时时间
    int _networkTimeout;               // FastDFS网络超时时间
};

struct RegisterCenterConfig {
    std::string _etcdAddr;             // ETCD地址
    std::string _serviceName;          // 服务名称
    std::string _serviceAddr;          // 服务地址
};

} // namespace fileService