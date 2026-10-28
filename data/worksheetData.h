#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <odb/mysql/database.hxx>
#include <sw/redis++/redis.h>
#include "worksheetEntity.h"

namespace fileService {

class WorksheetData {
public:
    WorksheetData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis);
    // 保存工作表映射到MySQL
    bool saveWorksheetMappingToDb(const WorksheetEntity& worksheet);
    // 根据fileId从MySQL中获取工作表映射
    std::vector<WorksheetEntity> getWorksheetMappingsByFileIdFromDb(const std::string& fileId);
    // 保存工作表映射到Redis缓存
    bool saveWorksheetMappingsToCache(const std::string& fileId, const std::vector<WorksheetEntity>& worksheets);
    // 根据fileId从Redis缓存中获取工作表映射
    std::optional<std::vector<WorksheetEntity>> getWorksheetMappingsFromCache(const std::string& fileId);
    // 根据fileId从Redis缓存中删除工作表映射
    void deleteWorksheetMappingsCache(const std::string& fileId);

private:
    std::shared_ptr<odb::database> _db;         // 操作MySQL数据库实例指针
    std::shared_ptr<sw::redis::Redis> _redis;   // 操作Redis缓存实例指针
    std::mutex _redisMutex;                     // 用于保护Redis缓存操作的互斥锁
    const static int WORKSHEET_CACHE_TTL_SECONDS = 3 * 24 * 60 * 60; // 工作表映射缓存过期时间，3天
};

} // namespace fileService