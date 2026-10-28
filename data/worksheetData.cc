#include "worksheetData.h"
#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <odb/transaction.hxx>
#include "worksheetEntity.h"
#include "odb/worksheetEntity-odb.hxx"

namespace fileService {

WorksheetData::WorksheetData(std::shared_ptr<odb::database> db, std::shared_ptr<sw::redis::Redis> redis)
    : _db(db), _redis(redis) {
    INF("WorksheetData initialized");
}

// 保存工作表映射到MySQL
bool WorksheetData::saveWorksheetMappingToDb(const WorksheetEntity& worksheet) {
    try {
        // 1. 创建并开启事务
        odb::transaction trans(_db->begin());
        // 2. 保存工作表映射到MySQL
        // 注：odb的persist函数需要非const引用，因此使用const_cast
        _db->persist(const_cast<WorksheetEntity&>(worksheet));
        // 3. 提交事务
        trans.commit();
        INF("WorksheetMapping saved to DB: fileId={}, worksheetName={}, tableName={}",
            worksheet.fileId(), worksheet.worksheetName(), worksheet.tableName());
        // 4. 返回结果
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save WorksheetMapping to DB: fileId={}, error={}",
            worksheet.fileId(), e.what());
        return false;
    }
}

// 根据fileId从MySQL中获取工作表映射
std::vector<WorksheetEntity> WorksheetData::getWorksheetMappingsByFileIdFromDb(const std::string& fileId) {
    std::vector<WorksheetEntity> result;
    try {
        // 1. 创建并开启事务
        odb::transaction trans(_db->begin());
        
        // 2. 根据fileId查询工作表映射
        // 等价的SQL语句：SELECT * FROM tbl_worksheet WHERE fileId = :fileId
        odb::result<WorksheetEntity> queryResult = _db->query<WorksheetEntity>(
            odb::query<WorksheetEntity>::fileId == fileId);
        
        // 3. 遍历查询结果，将每个工作表映射添加到结果向量中
        for (const auto& worksheet : queryResult) {
            result.push_back(worksheet);
        }

        // 4. 提交事务
        trans.commit();

        // 5. 返回结果
        INF("WorksheetMappings found in DB by fileId: fileId={}, count={}", fileId, result.size());
        return result;
    } catch (const std::exception& e) {
        ERR("Failed to get WorksheetMappings from DB by fileId: fileId={}, error={}",
            fileId, e.what());
        return result;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 保存工作表映射到Redis缓存
bool WorksheetData::saveWorksheetMappingsToCache(const std::string& fileId,
                                                   const std::vector<WorksheetEntity>& worksheets) {
    try {
        // 1. 构建Redis缓存键
        std::string key = "worksheet:" + fileId;

        // 2. worksheet信息以JSON子服务格式保存
        // 创建worksheet信息对应的JSON对象
        Json::Value rootJson;
        rootJson["fileId"] = fileId;
        Json::Value worksheetArray(Json::arrayValue);
        for (const auto& worksheet : worksheets) {
            Json::Value worksheetJson;
            worksheetJson["fileId"] = worksheet.fileId();
            worksheetJson["worksheetName"] = worksheet.worksheetName();
            worksheetJson["tableName"] = worksheet.tableName();
            worksheetArray.append(worksheetJson);
        }
        rootJson["worksheets"] = worksheetArray;

        // 3. 序列化JSON对象
        auto jsonStr = biteutil::JSON::serialize(rootJson);
        if (!jsonStr.has_value()) {
            WRN("Failed to serialize WorksheetMappings for cache: fileId={}", fileId);
            return false;
        }

        // 4. 将键值对保存到Redis缓存
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            _redis->setex(key, WORKSHEET_CACHE_TTL_SECONDS, jsonStr.value());
        }
        
        // 5. 返回结果
        INF("WorksheetMappings saved to cache: fileId={}", fileId);
        return true;
    } catch (const std::exception& e) {
        ERR("Failed to save WorksheetMappings to cache: fileId={}, error={}", fileId, e.what());
        return false;
    }
}

// 根据fileId从Redis缓存中获取工作表映射
std::optional<std::vector<WorksheetEntity>> WorksheetData::getWorksheetMappingsFromCache(
    const std::string& fileId) {
    try {
        // 1. 构建Redis缓存键
        std::string key = "worksheet:" + fileId;
        
        // 2. 从Redis缓存中获取JSON字符串
        std::string jsonStr;
        {
            std::lock_guard<std::mutex> lock(_redisMutex);
            auto result = _redis->get(key);
            if (!result.has_value() || result.value().empty()) {
                INF("WorksheetMappings not found in cache by fileId: fileId={}", fileId);
                return std::nullopt;
            }
            jsonStr = result.value();
        }

        // 3. 反序列化JSON字符串
        auto jsonOpt = biteutil::JSON::unserialize(jsonStr);
        if (!jsonOpt.has_value()) {
            WRN("Failed to unserialize WorksheetMappings from cache: fileId={}", fileId);
            return std::nullopt;
        }

        // 4. 从JSON对象中提取工作表映射
        std::vector<WorksheetEntity> worksheets;
        const auto& worksheetArray = jsonOpt.value()["worksheets"];
        for (const auto& worksheetJson : worksheetArray) {
            WorksheetEntity worksheet;
            worksheet.setFileId(worksheetJson["fileId"].asString());
            worksheet.setWorksheetName(worksheetJson["worksheetName"].asString());
            worksheet.setTableName(worksheetJson["tableName"].asString());
            worksheets.push_back(worksheet);
        }

        // 5. 返回结果
        INF("WorksheetMappings found in cache by fileId: fileId={}, count={}", fileId, worksheets.size());
        return worksheets;
    } catch (const std::exception& e) {
        ERR("Failed to get WorksheetMappings from cache by fileId: fileId={}, error={}",
            fileId, e.what());
        return std::nullopt;
    }
}

// 根据fileId从Redis缓存中删除工作表映射
void WorksheetData::deleteWorksheetMappingsCache(const std::string& fileId) {
    try {
        // 1. 构建Redis缓存键
        std::string key = "worksheet:" + fileId;
        
        // 2. 删除Redis缓存键
        std::lock_guard<std::mutex> lock(_redisMutex);
        _redis->del(key);
        
        // 3. 返回结果
        INF("WorksheetMappings cache deleted: fileId={}", fileId);
    } catch (const std::exception& e) {
        ERR("Failed to delete WorksheetMappings cache: fileId={}, error={}", fileId, e.what());
    }
}

} // namespace fileService