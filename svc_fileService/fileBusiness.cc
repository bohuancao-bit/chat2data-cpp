#include <fstream>
#include <sys/stat.h>
#include <jsoncpp/json/json.h>
#include <bite_scaffold/log.h>
#include "../common/errorHandler.h"
#include "../common/utils.h"
#include "fileBusiness.h"
#include "../proto/protoCode/excelParseService.pb.h"
#include "../proto/protoCode/dbService.pb.h"
#include "../proto/protoCode/aiService.pb.h"
#include <bite_scaffold/fdfs.h>

namespace fileService {

FileBusiness::FileBusiness(std::shared_ptr<FileInfoData> fileInfoData,
                            std::shared_ptr<WorksheetData> worksheetData,
                            std::shared_ptr<biterpc::SvcChannels> svcChannels)
    : _fileInfoData(fileInfoData)
    , _worksheetData(worksheetData)
    , _svcChannels(svcChannels) {
    INF("FileBusiness initialized");
}

// 保存或更新文件信息到数据库中
std::string FileBusiness::saveFileInfo(const FileInfo& fileInfo) {
    // 1. 构建FileInfoEntity实例
    FileInfoEntity entity(fileInfo._fileId,
                          fileInfo._fileName,
                          fileInfo._fileExt,
                          fileInfo._fileSize,
                          fileInfo._uploadTime,
                          fileInfo._fdfsFileId,
                          fileInfo._userId,
                          fileInfo._chatSessionId);

    // 2. 保存或更新文件信息到数据库中
    if (!_fileInfoData->saveFileInfoToDb(entity)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_INFO_SAVE_FAILED);
    }

    // 3. 从缓存中删除旧的文件信息
    _fileInfoData->deleteFileInfoCache(fileInfo._fileId);
    INF("FileInfo saved: fileId={}, fileName={}", fileInfo._fileId, fileInfo._fileName);

    // 4. 返回结果
    return fileInfo._fileId;
}

// 获取文件信息
FileInfo FileBusiness::getFileInfo(const std::string& fileId, const std::string& userId) {
    // 1. 从缓存中获取文件信息
    auto cacheResult = _fileInfoData->getFileInfoFromCacheByFileId(fileId);
    // 2. 如果缓存中存在则直接返回
    if (cacheResult.has_value()) {
        // 2.1. 检查文件信息是否属于当前用户
        FileInfoEntity entity = cacheResult.value();
        if (entity.userId() != userId) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_PERMISSION_DENIED);
        }
        // 2.2. 返回文件信息
        FileInfo fileInfo;
        fileInfo._fileId = entity.fileId();
        fileInfo._fileName = entity.fileName();
        fileInfo._fileSize = entity.fileSize();
        fileInfo._uploadTime = entity.uploadTime();
        fileInfo._fileExt = entity.fileExt();
        fileInfo._fdfsFileId = entity.fdfsFileId();
        fileInfo._userId = entity.userId();
        fileInfo._chatSessionId = entity.chatSessionId();
        return fileInfo;
    }
    // 3. 如果缓存中不存在则从数据库中获取
    auto dbResult = _fileInfoData->getFileInfoByFileIdAndUserIdFromDb(fileId, userId);
    if (!dbResult.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_NOT_FOUND);
    }
    // 4. 从数据库中获取文件信息，并更新缓存
    FileInfoEntity entity = dbResult.value();
    _fileInfoData->saveFileInfoToCache(entity);

    // 5. 返回文件信息
    FileInfo fileInfo;
    fileInfo._fileId = entity.fileId();
    fileInfo._fileName = entity.fileName();
    fileInfo._fileSize = entity.fileSize();
    fileInfo._uploadTime = entity.uploadTime();
    fileInfo._fileExt = entity.fileExt();
    fileInfo._fdfsFileId = entity.fdfsFileId();
    fileInfo._userId = entity.userId();
    fileInfo._chatSessionId = entity.chatSessionId();
    return fileInfo;
}

std::string FileBusiness::uploadExcelFile(const std::string& sessionId,
                                         const std::string& fileId,
                                         const std::string& userId,
                                         const std::string& fileName,
                                         const std::string& fileData) {
    // 1. 上传文件到FastDFS
    auto fdfsFileIdOpt = uploadToFastDFS(fileData, fileName);
    if (!fdfsFileIdOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_UPLOAD_FASTDFS_FAILED);
    }

    // 2. 更新数据库中的fileId对应的文件信息中的fdfsFileId字段
    //    并同步缓存处理
    auto dbResult = _fileInfoData->getFileInfoByFileIdFromDb(fileId);
    if (!dbResult.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_NOT_FOUND);
    }

    FileInfoEntity entity = dbResult.value();
    entity.setFdfsFileId(fdfsFileIdOpt.value());
    if (!_fileInfoData->saveFileInfoToDb(entity)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_INFO_SAVE_FAILED);
    }
    _fileInfoData->deleteFileInfoCache(fileId);

    // 3. 通过Excel解析子服务获取worksheet的表名列表
    //    注意：提供Excel文件在FastDFS中的文件id，而不是文件路径
    auto worksheets = getWorksheetList(fdfsFileIdOpt.value());
    if (worksheets.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_EXCEL_WORKSHEETS_GET_FAILED);
    }

    // 4. 保存worksheet映射到数据库
    for (const auto& worksheetName : worksheets) {
        std::string tableName = calculateTableName(worksheetName, fileId);
        WorksheetEntity worksheetEntity(fileId, worksheetName, tableName);
        if (!_worksheetData->saveWorksheetMappingToDb(worksheetEntity)) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_WORKSHEET_SAVE_FAILED);
        }
    }

    // 5. 通过Excel解析子服务解析Excel文件数据，并通过数据库子服务将worksheet数据保存到数据库
    if (!importExcelData2DB(fdfsFileIdOpt.value(), fileId, worksheets)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_EXCEL_PARSE_FAILED);
    }

    INF("Excel file uploaded successfully: fileId={}, fdfsFileId={}", fileId, fdfsFileIdOpt.value());
    return fdfsFileIdOpt.value();
}

std::string FileBusiness::uploadSQLiteFile(const std::string& sessionId,
                                          const std::string& fileId,
                                          const std::string& userId,
                                          const std::string& filename,
                                          const std::string& fileData) {
    // 1. 将文件数据直接上传到FastDFS
    auto fdfsFileIdOpt = uploadToFastDFS(fileData, filename);
    if (!fdfsFileIdOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_SQLITE_UPLOAD_FAILED);
    }

    // 2. 更新数据库中的fileId对应的文件信息中的fdfsFileId字段
    auto dbResult = _fileInfoData->getFileInfoByFileIdFromDb(fileId);
    if (!dbResult.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_NOT_FOUND);
    }

    FileInfoEntity entity = dbResult.value();
    entity.setFdfsFileId(fdfsFileIdOpt.value());
    if (!_fileInfoData->saveFileInfoToDb(entity)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_INFO_SAVE_FAILED);
    }
    _fileInfoData->deleteFileInfoCache(fileId);

    INF("SQLite file uploaded successfully: fileId={}, fdfsFileId={}", fileId, fdfsFileIdOpt.value());
    return fdfsFileIdOpt.value();
}

std::string FileBusiness::downloadExcelFile(const std::string& fileId, const std::string& userId) {
    // 1. 从数据库中获取文件信息，并检测文件是否属于当前用户
    auto fileInfo = getFileInfo(fileId, userId);

    // 2. 从FastDFS下载文件
    auto fileDataOpt = downloadFromFastDFS(fileInfo._fdfsFileId);
    if (!fileDataOpt.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_NOT_FOUND);
    }

    // 3. 返回文件数据
    return fileDataOpt.value();
}

std::string FileBusiness::downloadSQLiteFile(const std::string& fileId) {
    // 1. 从数据库中获取文件信息
    auto dbResult = _fileInfoData->getFileInfoByFileIdFromDb(fileId);
    if (!dbResult.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_SQLITE_FILE_NOT_FOUND);
    }

    // 2. 返回文件在FastDFS中的文件id
    FileInfoEntity entity = dbResult.value();
    INF("Get SQLite file fdfsFileId: fileId={}, fdfsFileId={}", fileId, entity.fdfsFileId());
    return entity.fdfsFileId();
}

bool FileBusiness::deleteFile(const std::string& fileId, const std::string& userId) {
    // 1. 从数据库中获取文件信息，并检测文件是否属于当前用户
    auto fileInfo = getFileInfo(fileId, userId);

    // 2. 从FastDFS删除文件
    if (!deleteFromFastDFS(fileInfo._fdfsFileId)) {
        WRN("Failed to delete file from FastDFS: fileId={}", fileId);
    }

    // 3. 如果是Excel文件，删除对应的数据库表
    if (fileInfo._fileExt == "xlsx") {
        auto worksheets = _worksheetData->getWorksheetMappingsByFileIdFromDb(fileId);
        if (!worksheets.empty()) {
            auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
            if (dbChannel) {
                std::vector<std::string> tableNames;
                for (const auto& worksheet : worksheets) {
                    tableNames.push_back(worksheet.tableName());
                }

                chat2Data::DatabaseService::DropTableExcelRequest request;
                request.set_request_id(chat2Data::Utils::generateUuid());
                request.set_session_id("");
                request.set_db_connect_id("excel_default");
                for (const auto& tableName : tableNames) {
                    request.add_table_names(tableName);
                }

                chat2Data::DatabaseService::DropTableExcelResponse response;
                brpc::Controller controller;
                chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());
                dbStub.DropTableExcel(&controller, &request, &response, nullptr);

                if (controller.Failed() || response.error_code() != 0) {
                    WRN("Failed to drop Excel tables: fileId={}, error_code={}, error_msg={}",
                        fileId, response.error_code(), response.error_msg());
                } else {
                    INF("Excel tables dropped: fileId={}, tables={}", fileId, tableNames.size());
                }
            }
        }
    }

    // 4. 从数据库和缓存中删除文件信息
    if (!_fileInfoData->deleteFileInfoByFileIdFromDb(fileId)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_DELETE_FAILED);
    }
    _fileInfoData->deleteFileInfoCache(fileId);

    // 5. worksheet的缓存也删除，数据库中的worksheet的信息会级联删除
    _worksheetData->deleteWorksheetMappingsCache(fileId);

    INF("File deleted successfully: fileId={}", fileId);
    return true;
}

bool FileBusiness::deleteFileInfo(const std::string& fileId) {
    // 1. 从数据库中删除文件信息
    if (!_fileInfoData->deleteFileInfoByFileIdFromDb(fileId)) {
        WRN("Failed to delete file info from database: fileId={}", fileId);
        return false;
    }

    // 2. 从缓存中删除文件信息
    _fileInfoData->deleteFileInfoCache(fileId);

    INF("File info deleted successfully: fileId={}", fileId);
    return true;
}

// 获取指定用户的文件表列表
std::vector<FileInfo> FileBusiness::getFileList(const std::string& userId) {
    // 1. 从MySQL中获取指定用户的文件信息
    auto dbResults = _fileInfoData->getFileListByUserIdFromDb(userId);

    // 2. 转换为FileInfo对象
    std::vector<FileInfo> fileList;
    for (const auto& entity : dbResults) {
        FileInfo fileInfo;
        fileInfo._fileId = entity.fileId();
        fileInfo._fileName = entity.fileName();
        fileInfo._fileSize = entity.fileSize();
        fileInfo._uploadTime = entity.uploadTime();
        fileInfo._fileExt = entity.fileExt();
        fileInfo._fdfsFileId = entity.fdfsFileId();
        fileInfo._userId = entity.userId();
        fileInfo._chatSessionId = entity.chatSessionId();
        fileList.push_back(fileInfo);
    }

    // 3. 返回文件列表
    return fileList;
}

chat2Data::fileService::ExcelData FileBusiness::previewExcel(const std::string& fileId,
                                                            const std::string& userId,
                                                            int32_t pageNumber,
                                                            int32_t pageSize) {
    // 1. 从数据库中获取文件信息，并检测文件是否属于当前用户
    auto fileInfo = getFileInfo(fileId, userId);

    // 2. 从缓存或数据库中获取worksheet信息
    auto cacheResult = _worksheetData->getWorksheetMappingsFromCache(fileId);
    std::vector<WorksheetEntity> worksheets;
    if (!cacheResult.has_value()) {
        worksheets = _worksheetData->getWorksheetMappingsByFileIdFromDb(fileId);
        if (!worksheets.empty()) {
            _worksheetData->saveWorksheetMappingsToCache(fileId, worksheets);
        }
    } else {
        worksheets = cacheResult.value();
    }

    if (worksheets.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_EXCEL_PARSE_FAILED);
    }

    // 3. 通过数据库子服务获取worksheet对应的表数据
    chat2Data::fileService::ExcelData excelData = getExcelDataFromDB(fileId, worksheets, pageNumber, pageSize);
    return excelData;
}

bool FileBusiness::associateFileChatSession(const std::string& fileId,
                                           const std::string& chatSessionId,
                                           const std::string& userId) {
    // 1. 从数据库中获取文件信息
    auto dbResult = _fileInfoData->getFileInfoByFileIdAndUserIdFromDb(fileId, userId);
    if (!dbResult.has_value()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_NOT_FOUND);
    }

    // 2. 更新文件信息中的聊天会话ID
    FileInfoEntity entity = dbResult.value();
    entity.setChatSessionId(chatSessionId);

    // 3. 保存更新后的文件信息到数据库
    if (!_fileInfoData->saveFileInfoToDb(entity)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_INFO_SAVE_FAILED);
    }
    _fileInfoData->deleteFileInfoCache(fileId);

    // 4. 通过AI子服务，更新聊天会话中的文件Id
    // 4.1 获取AI子服务的channel
    auto aiChannel = _svcChannels->getNode(FLAGS_ai_service);
    if (!aiChannel) {
        ERR("Failed to get AIService channel");
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::FILE_STORAGE_SERVICE_FAILED);
    }

    // 4.2 构建请求
    chat2Data::AiService::UpdateSessionFileRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_user_id(userId);
    request.set_chat_session_id(chatSessionId);
    request.set_file_id(fileId);

    // 4.3 创建AI子服务的RPC客户端
    chat2Data::AiService::UpdateSessionFileResponse response;
    brpc::Controller controller;
    chat2Data::AiService::AIService_Stub stub(aiChannel.get());

    // 4.4 发起rpc调用
    stub.UpdateSessionFile(&controller, &request, &response, nullptr);

    // 4.5 检查rpc调用结果
    if (controller.Failed()) {
        ERR("UpdateSessionFile RPC failed: {}", controller.ErrorText());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::AI_UPDATE_SESSION_FILE_FAILED);
    }
    if (response.error_code() != 0) {
        ERR("UpdateSessionFile failed: errorCode={}, errorMsg={}", response.error_code(), response.error_msg());
        throw chat2Data::Chat2DataException(static_cast<chat2Data::ErrorCode>(response.error_code()));
    }

    // 4.6 返回成功
    INF("File and chat session associated: fileId={}, chatSessionId={}", fileId, chatSessionId);
    return true;
}

std::vector<std::string> FileBusiness::getWorksheetDBTables(const std::string& fileId) {
    // 1. 从缓存中获取worksheet映射
    auto cacheResult = _worksheetData->getWorksheetMappingsFromCache(fileId);
    if (cacheResult.has_value()) {
        std::vector<std::string> tableNames;
        for (const auto& worksheet : cacheResult.value()) {
            tableNames.push_back(worksheet.tableName());
        }
        INF("Got worksheet tables from cache: fileId={}, count={}", fileId, tableNames.size());
        return tableNames;
    }

    // 2. 缓存中没有，从数据库中获取worksheet映射
    auto dbResult = _worksheetData->getWorksheetMappingsByFileIdFromDb(fileId);
    if (!dbResult.empty()) {
        // 3. 将数据库查询结果存入缓存
        _worksheetData->saveWorksheetMappingsToCache(fileId, dbResult);

        // 4. 提取表名列表
        std::vector<std::string> tableNames;
        for (const auto& worksheet : dbResult) {
            tableNames.push_back(worksheet.tableName());
        }
        INF("Got worksheet tables from db: fileId={}, count={}", fileId, tableNames.size());
        return tableNames;
    }

    // 5. 没有找到任何worksheet映射，返回空列表
    WRN("No worksheet tables found: fileId={}", fileId);
    return {};
}

// 上传文件到FastDFS---通过buffer来上传的
std::optional<std::string> FileBusiness::uploadToFastDFS(const std::string& fileData, const std::string& filename) {
    // 1. 上传文件到FastDFS
    auto result = bitefdfs::FDFSClient::upload_from_buff(fileData);
    if (!result.has_value()) {
        ERR("Failed to upload file to FastDFS: filename={}", filename);
        return std::nullopt;
    }
    INF("File uploaded to FastDFS: filename={}, fileId={}", filename, result.value());

    // 2. 返回上传后的文件ID
    return result.value();
}

std::optional<std::string> FileBusiness::downloadFromFastDFS(const std::string& fileId) {
    std::string fileData;
    if (!bitefdfs::FDFSClient::download_to_buff(fileId, fileData)) {
        ERR("Failed to download file from FastDFS: fileId={}", fileId);
        return std::nullopt;
    }
    INF("File downloaded from FastDFS: fileId={}, size={}", fileId, fileData.size());
    return fileData;
}

// 从FastDFS删除文件---通过文件ID来删除的
bool FileBusiness::deleteFromFastDFS(const std::string& fileId) {
    // 1. 从FastDFS删除文件
    if (!bitefdfs::FDFSClient::remove(fileId)) {
        WRN("Failed to delete file from FastDFS: fileId={}", fileId);
        return false;
    }
    // 2. 返回删除成功
    INF("File deleted from FastDFS: fileId={}", fileId);
    return true;
}

std::vector<std::string> FileBusiness::getWorksheetList(const std::string& fdfsFileId) {
    // 1. 获取Excel解析子服务的通信通道
    auto channel = _svcChannels->getNode(FLAGS_excel_service);
    if (!channel) {
        ERR("ExcelParserService not available");
        return {};
    }

    // 2. 构建请求
    chat2Data::excelParseService::GetWorksheetsRequest request;
    request.set_request_id(chat2Data::Utils::generateUuid());
    request.set_fdfs_file_id(fdfsFileId);

    // 3. 创建RPC客户端
    brpc::Controller controller;
    chat2Data::excelParseService::ExcelParserService_Stub stub(channel.get());

    // 4. 发起获取worksheet列表的请求
    chat2Data::excelParseService::GetWorksheetsResponse response;
    stub.GetWorksheets(&controller, &request, &response, nullptr);

    // 5. 检查响应是否成功
    if (controller.Failed() || response.error_code() != 0) {
        ERR("Failed to get worksheets from ExcelParserService: error_code={}, error_msg={}",
            response.error_code(), response.error_msg());
        return {};
    }

    // 6. 提取worksheet列表
    std::vector<std::string> worksheets;
    for (const auto& worksheet : response.worksheets()) {
        worksheets.push_back(worksheet);
    }

    INF("Got {} worksheets from ExcelParserService", worksheets.size());
    return worksheets;
}

// 根据worksheet的名称来构造数据库表名
// 将不符合字母、数字、下划线、中文条件的字符替换为_
std::string FileBusiness::calculateTableName(const std::string& worksheetName, const std::string& fileId) {
    std::string sanitizedName;
    for (char c : worksheetName) {
        if (std::isalnum(c) || c == '_' || static_cast<unsigned char>(c) > 127) {
            sanitizedName += c;
        } else {
            sanitizedName += '_';
        }
    }
    return sanitizedName + "_" + fileId;
}

bool FileBusiness::importExcelData2DB(const std::string& fdfsFileId, const std::string& fileId, const std::vector<std::string>& worksheets) {
    // 1. 解析Excel文件数据
    // 1.1 获取Excel解析子服务的通信通道
    auto excelChannel = _svcChannels->getNode(FLAGS_excel_service);
    if (!excelChannel) {
        ERR("ExcelParserService not available when importing Excel data to database");
        return false;
    }

    // 1.2 构建请求
    chat2Data::excelParseService::ParseExcelRequest parseRequest;
    parseRequest.set_request_id(chat2Data::Utils::generateUuid());
    parseRequest.set_fdfs_file_id(fdfsFileId);
    for (const auto& worksheet : worksheets) {
        parseRequest.add_worksheets(worksheet);
    }

    // 1.3 创建excel解析子服务的客户端
    brpc::Controller parseController;
    chat2Data::excelParseService::ParseExcelResponse parseResponse;
    chat2Data::excelParseService::ExcelParserService_Stub excelStub(excelChannel.get());

    // 1.4 发起解析Excel文件数据的请求
    excelStub.ParseExcel(&parseController, &parseRequest, &parseResponse, nullptr);

    // 1.5 检查响应是否成功
    if (parseController.Failed() || parseResponse.error_code() != 0) {
        ERR("Failed to parse Excel from ExcelParserService: error_code={}, error_msg={}",
            parseResponse.error_code(), parseResponse.error_msg());
        return false;
    }

    // 2. 导入解析后的数据到数据库
    // 2.1 获取数据库子服务的通信通道
    auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("DatabaseService not available when importing Excel data");
        return false;
    }

    // 2.2 构建rpc请求
    for (const auto& worksheetData : parseResponse.worksheets()) {
        std::string tableName = calculateTableName(worksheetData.name(), fileId);

        chat2Data::DatabaseService::ImportExcelDataRequest importRequest;
        importRequest.set_request_id(chat2Data::Utils::generateUuid());
        importRequest.set_session_id("");
        importRequest.set_db_connect_id("excel_default");
        importRequest.set_table_name(tableName);
        *importRequest.mutable_worksheet_data() = worksheetData;

        // 2.3 创建数据库子服务的客户端
        chat2Data::DatabaseService::ImportExcelDataResponse importResponse;
        brpc::Controller importController;
        chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());

        // 2.4 发起导入Excel数据到数据库的请求
        dbStub.ImportExcelData(&importController, &importRequest, &importResponse, nullptr);

        // 2.5 检查响应是否成功
        if (importController.Failed() || importResponse.error_code() != 0) {
            WRN("Failed to import worksheet {} to database: error_code={}, error_msg={}",
                worksheetData.name(), importResponse.error_code(), importResponse.error_msg());
            continue;
        }

        INF("Successfully imported worksheet {} to database table {}: {} rows",
            worksheetData.name(), tableName, importResponse.result().imported_rows());
    }

    return true;
}

chat2Data::fileService::ExcelData FileBusiness::getExcelDataFromDB(const std::string& fileId,
                                                                           const std::vector<WorksheetEntity>& worksheets,
                                                                           int32_t pageNumber,
                                                                           int32_t pageSize) {
    chat2Data::fileService::ExcelData excelData;

        auto dbChannel = _svcChannels->getNode(FLAGS_db_service);
    if (!dbChannel) {
        ERR("DatabaseService not available when getting Excel data");
        return excelData;
    }

    for (const auto& worksheet : worksheets) {
        chat2Data::fileService::Sheet sheet;
        sheet.set_name(worksheet.worksheetName());

        chat2Data::DatabaseService::GetTableDataRequest request;
        request.set_request_id(chat2Data::Utils::generateUuid());
        request.set_session_id("");
        request.set_db_connect_id("excel_default");
        request.set_table_name(worksheet.tableName());
        request.set_page_number(pageNumber);
        request.set_page_size(pageSize);
        request.set_force_original(true);   // 此处获取原表

        chat2Data::DatabaseService::GetTableDataResponse response;
        brpc::Controller controller;
        chat2Data::DatabaseService::DatabaseService_Stub dbStub(dbChannel.get());
        dbStub.GetTableData(&controller, &request, &response, nullptr);

        if (controller.Failed() || response.error_code() != 0) {
            WRN("Failed to get table data for worksheet {}: error_code={}, error_msg={}",
                worksheet.worksheetName(), response.error_code(), response.error_msg());
            continue;
        }

        sheet.set_total_rows(response.result().table_schema().table_data().total_rows());
        sheet.set_col_count(response.result().table_schema().column_info_size());
        sheet.set_current_page(pageNumber);
        sheet.set_total_pages((response.result().table_schema().table_data().total_rows() + pageSize - 1) / pageSize);
        sheet.set_page_size(pageSize);

        for (const auto& columnInfo : response.result().table_schema().column_info()) {
            sheet.add_columns(columnInfo.name());
        }

        for (const auto& row : response.result().table_schema().table_data().rows()) {
            chat2Data::fileService::Row dataRow;
            for (const auto& cell : row.cells()) {
                dataRow.add_cells(cell);
            }
            *sheet.add_data() = std::move(dataRow);
        }

        *excelData.add_sheets() = std::move(sheet);
    }

    return excelData;
}

} // namespace fileService