#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <odb/mysql/database.hxx>
#include <sw/redis++/redis.h>
#include "fileInfoEntity.h"

namespace fileService {

class FileInfoData {
public:
    FileInfoData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis);
    // MySQL数据库操作接口
    // 保存或更新文件信息到MySQL
    bool saveFileInfoToDb(const FileInfoEntity& fileInfo);
    // 根据文件ID从MySQL获取文件信息
    std::optional<FileInfoEntity> getFileInfoByFileIdFromDb(const std::string& fileId);
    // 根据文件ID和用户ID从MySQL获取文件信息
    std::optional<FileInfoEntity> getFileInfoByFileIdAndUserIdFromDb(const std::string& fileId, const std::string& userId);
    // 根据文件ID从MySQL删除文件信息
    bool deleteFileInfoByFileIdFromDb(const std::string& fileId);
    // 根据用户ID从MySQL获取文件列表
    std::vector<FileInfoEntity> getFileListByUserIdFromDb(const std::string& userId);
    
    // Redis数据库操作接口
    // 保存文件信息到Redis缓存
    bool saveFileInfoToCache(const FileInfoEntity& fileInfo);
    // 根据文件ID从Redis缓存获取文件信息
    std::optional<FileInfoEntity> getFileInfoFromCacheByFileId(const std::string& fileId);
    // 删除Redis缓存中的文件信息
    void deleteFileInfoCache(const std::string& fileId);

private:
    std::shared_ptr<odb::database> _db;            // 操作Mysql数据库的实例指针
    std::shared_ptr<sw::redis::Redis> _redis;      // 操作Redis数据库的实例指针
    std::mutex _redisMutex;                        // 用于保护Redis数据库操作的互斥锁
    const static int FILE_CACHE_TTL_SECONDS = 3 * 24 * 60 * 60; // 文件缓存过期时间：3天
};

} // namespace fileService