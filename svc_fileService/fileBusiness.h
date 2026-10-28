#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <cstdint>
#include <gflags/gflags.h>
#include <bite_scaffold/rpc.h>
#include "../data/fileInfoData.h"
#include "../data/worksheetData.h"
#include "common.h"
#include "../proto/protoCode/fileService.pb.h"

// 声明gflags变量（在main.cc中定义）
DECLARE_string(ai_service);
DECLARE_string(excel_service);
DECLARE_string(db_service);

namespace fileService {

class FileBusiness {
public:
    FileBusiness(std::shared_ptr<FileInfoData> fileInfoData,
                 std::shared_ptr<WorksheetData> worksheetData,
                 std::shared_ptr<biterpc::SvcChannels> svcChannels);

    // 对应上传文件信息的RPC接口：将文件信息保存或更新到MySQL数据库和Redis缓存
    std::string saveFileInfo(const FileInfo& fileInfo);
    // 对应获取文件信息的RPC接口：从MySQL数据库和Redis缓存中获取文件信息
    FileInfo getFileInfo(const std::string& fileId, const std::string& userId);
    // 对应上传Excel文件的RPC接口：将Excel文件上传到FastDFS存储系统
    std::string uploadExcelFile(const std::string& sessionId,
                                const std::string& fileId,
                                const std::string& userId,
                                const std::string& fileName,
                                const std::string& fileData);
    // 对应上传SQLite文件的RPC接口：将SQLite文件上传到FastDFS存储系统
    std::string uploadSQLiteFile(const std::string& sessionId,
                                 const std::string& fileId,
                                 const std::string& userId,
                                 const std::string& filename,
                                 const std::string& fileData);
    // 对应下载Excel文件的RPC接口：从FastDFS存储系统下载Excel文件
    std::string downloadExcelFile(const std::string& fileId, const std::string& userId);
    // 对应下载SQLite文件的RPC接口：从FastDFS存储系统下载SQLite文件
    std::string downloadSQLiteFile(const std::string& fileId);
    // 对应删除文件的RPC接口：从FastDFS存储系统删除文件--即能删除Excel文件，也能删除SQLite文件
    bool deleteFile(const std::string& fileId, const std::string& userId);
    // 删除文件信息（仅删除数据库和缓存中的文件信息，不删除FastDFS中的文件数据）
    bool deleteFileInfo(const std::string& fileId);
    // 对应获取文件列表的RPC接口：从MySQL数据库和Redis缓存中获取文件列表
    std::vector<FileInfo> getFileList(const std::string& userId);
    // 对应预览Excel文件的RPC接口：通过数据库子服务获取Excel文件对应的worksheet的数据
    chat2Data::fileService::ExcelData previewExcel(const std::string& fileId,
                                                  const std::string& userId,
                                                  int32_t pageNumber,
                                                  int32_t pageSize);
    // 对应关联文件和聊天会话的RPC接口：将文件ID和聊天会话ID关联起来，作用：当用户点击聊天会话列表项时，能跳转到智能Excel页面
    // 显示历史聊天消息 以及 Excel文件内容
    bool associateFileChatSession(const std::string& fileId,
                                 const std::string& chatSessionId,
                                 const std::string& userId);
    // 对应获取Worksheet数据库表名列表的RPC接口：从MySQL数据库和Redis缓存中获取指定文件ID对应的worksheet数据库表名列表
    std::vector<std::string> getWorksheetDBTables(const std::string& fileId);

private:
    // 辅助方法
    // 上传文件到FastDFS系统中
    std::optional<std::string> uploadToFastDFS(const std::string& fileData, const std::string& filename);
    // 从FastDFS中下载文件到本地
    std::optional<std::string> downloadFromFastDFS(const std::string& fileId);
    // 从FastDFS中删除文件
    bool deleteFromFastDFS(const std::string& fileId);
    // 通过Excel解析子服务获取Excel文件中的worksheet表列表
    std::vector<std::string> getWorksheetList(const std::string& fdfsFileId);
    // 计算worksheet对应的数据库表名
    std::string calculateTableName(const std::string& worksheetName, const std::string& fileId);
    // 通过数据库子服务将Excel数据导入数据库
    bool importExcelData2DB(const std::string& fdfsFileId,
                                   const std::string& fileId,
                                   const std::vector<std::string>& worksheets);
    // 通过数据库子服务获取Excel表数据
    chat2Data::fileService::ExcelData getExcelDataFromDB(const std::string& fileId,
                                                                const std::vector<WorksheetEntity>& worksheets,
                                                                int32_t pageNumber,
                                                                int32_t pageSize);

private:
    std::shared_ptr<FileInfoData> _fileInfoData;           // 通过该实例指针访问MySQL和Redis---tbl_fileInfo
    std::shared_ptr<WorksheetData> _worksheetData;        // 通过该实例指针访问MySQL和Redis---tbl_worksheet
    std::shared_ptr<biterpc::SvcChannels> _svcChannels;   // 通过该实例指针获取其他RPC服务节点
};

} // namespace fileService