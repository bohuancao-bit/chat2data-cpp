#include "fileInfoData.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/transaction.hxx>
#include "fileInfoEntity.h"
#include "odb/fileInfoEntity-odb.hxx"

namespace fileService {

FileInfoData::FileInfoData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis)
    : _db(db), _redis(redis) {
    INF("FileInfoData initialized");
}

// 保存或更新文件信息到MySQL数据库
bool FileInfoData::saveFileInfoToDb(const FileInfoEntity& fileInfo) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询fileId对应的文件是否存在
        // 等价的SQL语句：SELECT * FROM fileInfo WHERE fileId = ?
        auto existingFile = _db->query_one<FileInfoEntity>(
            odb::query<FileInfoEntity>::fileId == fileInfo.fileId());
        
        // 3. 检查fileId的文件是否存在
        if (existingFile) {
            // 3.1. 如果存在，则更新fileId对应的文件信息 
            existingFile->setFileName(fileInfo.fileName());
            existingFile->setFileExt(fileInfo.fileExt());
            existingFile->setFileSize(fileInfo.fileSize());
            existingFile->setUploadTime(fileInfo.uploadTime());
            existingFile->setFdfsFileId(fileInfo.fdfsFileId());
            existingFile->setUserId(fileInfo.userId());
            existingFile->setChatSessionId(fileInfo.chatSessionId());
            // 3.2. 更新数据库中的fileId对应的文件信息
            // 等价的SQL语句：UPDATE fileInfo SET fileName = ?, fileExt = ?, fileSize = ?, uploadTime = ?, fdfsFileId = ?, userId = ?, chatSessionId = ? WHERE fileId = ?
            _db->update(*existingFile);
            INF("FileInfo updated in DB: fileId={}", fileInfo.fileId());
        } else {
            // 3.3. 如果不存在，则插入新的文件信息
            // 等价的SQL语句：INSERT INTO fileInfo (fileId, fileName, fileExt, fileSize, uploadTime, fdfsFileId, userId, chatSessionId)
            // VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            // 注：odb的persist函数需要非const引用，因此使用const_cast
            _db->persist(const_cast<FileInfoEntity&>(fileInfo));
            INF("FileInfo saved to DB: fileId={}", fileInfo.fileId());
        }
        // 4. 提交事务
        trans.commit();
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save FileInfo to DB: fileId={}, error={}", fileInfo.fileId(), e.what());
        return false;
    }
}

// 根据fileId从MySQL数据库中查询文件信息
std::optional<FileInfoEntity> FileInfoData::getFileInfoByFileIdFromDb(const std::string& fileId) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询fileId对应的文件是否存在
        // 等价的SQL语句：SELECT * FROM fileInfo WHERE fileId = ?
        auto result = _db->query_one<FileInfoEntity>(
            odb::query<FileInfoEntity>::fileId == fileId);
        // 3. 提交事务
        trans.commit();
        // 4. 检查查询结果是否存在，存在则返回文件信息
        if (result) {
            INF("FileInfo found in DB by fileId: fileId={}", fileId);
            return *result;
        }
        WRN("FileInfo not found in DB by fileId: fileId={}", fileId);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get FileInfo from DB by fileId: fileId={}, error={}", fileId, e.what());
        return std::nullopt;
    }
}

// 根据fileId获取文件信息，并检查该文件是否属于当前用户
std::optional<FileInfoEntity> FileInfoData::getFileInfoByFileIdAndUserIdFromDb(
    const std::string& fileId, const std::string& userId) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询fileId对应的文件是否存在
        // 等价的SQL语句：SELECT * FROM fileInfo WHERE fileId = ? AND userId = ?
        auto result = _db->query_one<FileInfoEntity>(
            (odb::query<FileInfoEntity>::fileId == fileId) &&
            (odb::query<FileInfoEntity>::userId == userId));
        // 3. 提交事务
        trans.commit();
        // 4. 检查查询结果是否存在，存在则返回文件信息
        if (result) {
            INF("FileInfo found in DB by fileId and userId: fileId={}, userId={}", fileId, userId);
            return *result;
        }
        WRN("FileInfo not found in DB by fileId and userId: fileId={}, userId={}", fileId, userId);
        return std::nullopt;
    } catch (const std::exception& e) {
        ERR("Failed to get FileInfo from DB by fileId and userId: fileId={}, userId={}, error={}",
            fileId, userId, e.what());
        return std::nullopt;
    }
}

// 根据fileId删除文件信息
bool FileInfoData::deleteFileInfoByFileIdFromDb(const std::string& fileId) {
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 删除fileId对应的文件信息
        // 等价的SQL语句：DELETE FROM fileInfo WHERE fileId = ?
        _db->erase_query<FileInfoEntity>(odb::query<FileInfoEntity>::fileId == fileId);
        // 3. 提交事务
        trans.commit();
        INF("FileInfo deleted from DB: fileId={}", fileId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to delete FileInfo from DB: fileId={}, error={}", fileId, e.what());
        return false;
    }
}

// 根据userId获取用户所有文件信息
std::vector<FileInfoEntity> FileInfoData::getFileListByUserIdFromDb(const std::string& userId) {
    std::vector<FileInfoEntity> result;
    try {
        // 1. 创建事务并开启事务
        odb::transaction trans(_db->begin());
        // 2. 在数据库中查询userId对应的文件信息
        // 等价的SQL语句：SELECT * FROM fileInfo WHERE userId = ?
        odb::result<FileInfoEntity> queryResult = _db->query<FileInfoEntity>(
            odb::query<FileInfoEntity>::userId == userId);
        // 3. 遍历查询结果，将每个文件信息添加到结果向量中
        for (const auto& file : queryResult) {
            result.push_back(file);
        }
        // 4. 提交事务
        trans.commit();
        INF("FileList found in DB by userId: userId={}, count={}", userId, result.size());
        return result;
    } catch (const std::exception& e) {
        ERR("Failed to get FileList from DB by userId: userId={}, error={}", userId, e.what());
        return result;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 保存文件信息到缓存
bool FileInfoData::saveFileInfoToCache(const FileInfoEntity& fileInfo) {
    try {
        // 1. 构建缓存键，格式为"file:fileId"
        std::string key = "file:" + fileInfo.fileId();

        // 2. 构建缓存值，缓存值以Json字符串方式保存
        Json::Value fileJson;
        fileJson["fileId"] = fileInfo.fileId();
        fileJson["fileName"] = fileInfo.fileName();
        fileJson["fileExt"] = fileInfo.fileExt();
        fileJson["fileSize"] = static_cast<Json::Int64>(fileInfo.fileSize());
        fileJson["uploadTime"] = static_cast<Json::Int64>(fileInfo.uploadTime());
        fileJson["fdfsFileId"] = fileInfo.fdfsFileId();
        fileJson["userId"] = fileInfo.userId();
        fileJson["chatSessionId"] = fileInfo.chatSessionId();

        // 3. 序列化Json对象为字符串
        auto jsonStr = biteutil::JSON::serialize(fileJson);
        if (!jsonStr.has_value()) {
            WRN("Failed to serialize FileInfo for cache: fileId={}", fileInfo.fileId());
            return false;
        }

        // 4. 将缓存值保存到Redis缓存中，过期时间为FILE_CACHE_TTL_SECONDS秒
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            _redis->setex(key, FILE_CACHE_TTL_SECONDS, jsonStr.value());
        }
        // 5. 返回结果
        INF("FileInfo saved to cache: fileId={}", fileInfo.fileId());
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save FileInfo to cache: fileId={}, error={}", fileInfo.fileId(), e.what());
        return false;
    }
}

// 根据fileId从缓存中获取文件信息
std::optional<FileInfoEntity> FileInfoData::getFileInfoFromCacheByFileId(const std::string& fileId) {
    try {
        // 1. 构建缓存键，格式为"file:fileId"
        std::string key = "file:" + fileId;
        // 2. 从Redis缓存中获取缓存值
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("FileInfo not found in cache by fileId: fileId={}", fileId);
                return std::nullopt;
            }
            jsonStr = result.value();
        }
        // 3. 反序列化Json字符串为对象对象
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize FileInfo from cache: fileId={}", fileId);
            return std::nullopt;
        }

        // 4. 从Json对象中提取文件信息
        FileInfoEntity fileInfo;
        fileInfo.setFileId(jsonOpt.value()["fileId"].asString());
        fileInfo.setFileName(jsonOpt.value()["fileName"].asString());
        fileInfo.setFileExt(jsonOpt.value()["fileExt"].asString());
        fileInfo.setFileSize(jsonOpt.value()["fileSize"].asInt64());
        fileInfo.setUploadTime(jsonOpt.value()["uploadTime"].asInt64());
        fileInfo.setFdfsFileId(jsonOpt.value()["fdfsFileId"].asString());
        fileInfo.setUserId(jsonOpt.value()["userId"].asString());
        fileInfo.setChatSessionId(jsonOpt.value()["chatSessionId"].asString());
        // 5. 返回结果
        INF("FileInfo found in cache by fileId: fileId={}", fileId);
        return fileInfo;
    } catch (const std::exception& e) {
        ERR("Failed to get FileInfo from cache by fileId: fileId={}, error={}", fileId, e.what());
        return std::nullopt;
    }
}

// 根据fileId从缓存中删除文件信息
void FileInfoData::deleteFileInfoCache(const std::string& fileId) {
    try {
        // 1. 构建缓存键，格式为"file:fileId"
        std::string key = "file:" + fileId;
        
        // 2. 从Redis缓存中删除缓存值
        std::lock_guard<std::mutex> lock(_redisMutex);
        _redis->del(key);
        
        INF("FileInfo cache deleted: fileId={}", fileId);
    } catch (const std::exception& e) {
        ERR("Failed to delete FileInfo cache: fileId={}, error={}", fileId, e.what());
    }
}

} // namespace fileService