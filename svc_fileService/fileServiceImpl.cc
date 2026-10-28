#include <exception>
#include <bite_scaffold/log.h>
#include <bite_scaffold/rpc.h>
#include "../common/errorHandler.h"
#include "../common/utils.h"
#include "fileServiceImpl.h"
#include "fileBusiness.h"
#include "common.h"


namespace fileService {

FileServiceImpl::FileServiceImpl(std::shared_ptr<FileBusiness> fileBusiness)
    : _fileBusiness(fileBusiness) {
}

FileServiceImpl::~FileServiceImpl() {
}

void FileServiceImpl::UploadFileInfo(google::protobuf::RpcController* controller,
                                    const chat2Data::fileService::UploadFileInfoRequest* request,
                                    chat2Data::fileService::UploadFileInfoResponse* response,
                                    google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string userId = request->user_id();
    const auto& fileInfoProto = request->file_info();

    // 3. 校验参数
    if (sessionId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 构建文件信息
        FileInfo fileInfo;
        fileInfo._fileId = chat2Data::Utils::generateUuid();
        fileInfo._fileName = fileInfoProto.filename();
        fileInfo._fileSize = fileInfoProto.file_size();
        fileInfo._fileExt = fileInfoProto.file_ext();
        fileInfo._userId = userId;
        fileInfo._uploadTime = std::time(nullptr);

        // 5. 保存文件信息到数据库
        std::string fileId = _fileBusiness->saveFileInfo(fileInfo);

        // 6. 返回rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->set_file_id(fileId);
        INF("UploadFileInfo success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::GetFileInfo(google::protobuf::RpcController* controller,
                                  const chat2Data::fileService::GetFileInfoRequest* request,
                                  chat2Data::fileService::GetFileInfoResponse* response,
                                  google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 从数据库中获取文件信息
        FileInfo fileInfo = _fileBusiness->getFileInfo(fileId, userId);

        // 5 构建rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        chat2Data::fileService::FileDetail* result = response->mutable_result();
        result->set_file_id(fileInfo._fileId);
        result->set_file_name(fileInfo._fileName);
        result->set_file_size(fileInfo._fileSize);
        result->set_upload_time(fileInfo._uploadTime);
        result->set_file_ext(fileInfo._fileExt);

        INF("GetFileInfo success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::UploadFile(google::protobuf::RpcController* controller,
                                 const chat2Data::fileService::UploadFileRequest* request,
                                 chat2Data::fileService::UploadFileResponse* response,
                                 google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 从数据库中获取文件信息
        FileInfo fileInfo = _fileBusiness->getFileInfo(fileId, userId);

        // 5. 从attachment中获取文件数据
        butil::IOBuf attachment;
        brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
        cntl->request_attachment().append(attachment);
        std::string fileData = cntl->request_attachment().to_string();

        // 6. 上传文件数据到FastDFS中
        // 在该方法的内部，将文件数据上传到FastDFS中之后，还需要通过excel解析子服务解析Excel文件内容
        // 解析完成之后，通过数据库子服务将解析的结果保存到数据库中
        _fileBusiness->uploadExcelFile(sessionId, fileId, userId, fileInfo._fileName, fileData);

        // 7. 返回rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        INF("UploadFile success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::DownloadFile(google::protobuf::RpcController* controller,
                                  const chat2Data::fileService::DownloadFileRequest* request,
                                  chat2Data::fileService::DownloadFileResponse* response,
                                  google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 从FastDFS中下载文件数据
        std::string fileData = _fileBusiness->downloadExcelFile(fileId, userId);

        // 5. 构造rpc响应，文件数据通过attachment返回
        brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
        cntl->response_attachment().append(fileData);

        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        INF("DownloadFile success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::DeleteFile(google::protobuf::RpcController* controller,
                                 const chat2Data::fileService::DeleteFileRequest* request,
                                 chat2Data::fileService::DeleteFileResponse* response,
                                 google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 从FastDFS中删除文件数据，并删除文件信息，以及worksheet映射表数据，以及存储在数据库中的excel的数据
        _fileBusiness->deleteFile(fileId, userId);

        // 5. 返回rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        INF("DeleteFile success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::PreviewExcel(google::protobuf::RpcController* controller,
                                  const chat2Data::fileService::PreviewExcelRequest* request,
                                  chat2Data::fileService::PreviewExcelResponse* response,
                                  google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();
    std::string userId = request->user_id();
    int32_t pageNumber = request->page_number();
    int32_t pageSize = request->page_size();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    // 4. 校验分页参数
    if (pageNumber <= 0) pageNumber = 1;
    if (pageSize <= 0) pageSize = 50;

    try {
        // 5. 从数据库中查询excel数据
        chat2Data::fileService::ExcelData excelData = _fileBusiness->previewExcel(fileId, userId, pageNumber, pageSize);

        // 6. 返回rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        chat2Data::fileService::PreviewExcelResult* result = response->mutable_result();
        result->set_file_id(fileId);
        *result->mutable_excel_data() = excelData;

        INF("PreviewExcel success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::GetFileList(google::protobuf::RpcController* controller,
                                  const chat2Data::fileService::GetFileListRequest* request,
                                  chat2Data::fileService::GetFileListResponse* response,
                                  google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 从数据库中查询文件列表
        std::vector<FileInfo> fileList = _fileBusiness->getFileList(userId);

        // 5. 构建rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        for (const auto& fileInfo : fileList) {
            chat2Data::fileService::FileListItem* item = response->mutable_result()->add_file_list();
            item->set_file_id(fileInfo._fileId);
            item->set_file_name(fileInfo._fileName);
            item->set_file_size(fileInfo._fileSize);
            item->set_upload_time(fileInfo._uploadTime);
            item->set_chat_session_id(fileInfo._chatSessionId);
        }

        INF("GetFileList success: requestId={}, userId={}, count={}", requestId, userId, fileList.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::HandleFileChatSessionMap(google::protobuf::RpcController* controller,
                                              const chat2Data::fileService::HandleFileChatSessionMapRequest* request,
                                              chat2Data::fileService::HandleFileChatSessionMapResponse* response,
                                              google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();
    std::string chatSessionId = request->chat_session_id();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty() || chatSessionId.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 关联文件和聊天会话
        _fileBusiness->associateFileChatSession(fileId, chatSessionId, userId);

        // 5. 构建rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        INF("HandleFileChatSessionMap success: requestId={}, fileId={}, chatSessionId={}", requestId, fileId, chatSessionId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::UploadSQLiteFile(google::protobuf::RpcController* controller,
                                      const chat2Data::fileService::UploadSQLiteFileRequest* request,
                                      chat2Data::fileService::UploadSQLiteFileResponse* response,
                                      google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = chat2Data::Utils::generateUuid();
    std::string filename = request->filename();
    std::string userId = request->user_id();

    // 3. 校验参数
    if (sessionId.empty() || filename.empty() || userId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 从attachment中获取文件数据
        brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
        std::string fileData = cntl->request_attachment().to_string();

        // 5. 构建FileInfo对象
        FileInfo fileInfo;
        fileInfo._fileId = fileId;
        fileInfo._fileName = filename;
        fileInfo._fileSize = fileData.size();
        fileInfo._fileExt = ".db";
        fileInfo._userId = userId;
        // sqlite数据库连接成功，新建聊天会话成功之后，前端会主动发送建立聊天会话和sqlite文件的关联
        fileInfo._chatSessionId = "";
        fileInfo._uploadTime = std::time(nullptr);

        // 6. 保存文件信息到数据库
        _fileBusiness->saveFileInfo(fileInfo);

        // 7. 上传文件数据到FastDFS
        _fileBusiness->uploadSQLiteFile(sessionId, fileId, userId, filename, fileData);

        // 8. 构建rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->set_file_id(fileId);
        INF("UploadSQLiteFile success: requestId={}, fileId={}", requestId, fileId);
    } catch (const chat2Data::Chat2DataException& e) {
        // 9. 文件数据上传失败时，删除已保存的文件信息和缓存
        _fileBusiness->deleteFileInfo(fileId);
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::GetSQLiteFile(google::protobuf::RpcController* controller,
                                   const chat2Data::fileService::GetSQLiteFileRequest* request,
                                   chat2Data::fileService::GetSQLiteFileResponse* response,
                                   google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 获取SQLite文件在FastDFS中的文件id
        std::string fdfsFileId = _fileBusiness->downloadSQLiteFile(fileId);

        // 5. 构建rpc响应---返回文件在FastDFS中的文件id
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        response->mutable_result()->set_fdfsfileid(fdfsFileId);
        INF("GetSQLiteFile success: requestId={}, fileId={}, fdfsFileId={}", requestId, fileId, fdfsFileId);
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

void FileServiceImpl::GetWorksheetDBTables(google::protobuf::RpcController* controller,
                                          const chat2Data::fileService::GetWorksheetDBTablesRequest* request,
                                          chat2Data::fileService::GetWorksheetDBTablesResponse* response,
                                          google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 解析rpc请求参数
    std::string requestId = request->request_id();
    std::string sessionId = request->session_id();
    std::string fileId = request->file_id();

    // 3. 校验参数
    if (sessionId.empty() || fileId.empty()) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        response->set_error_msg(chat2Data::error2String(chat2Data::ErrorCode::FILE_PARAM_INVALID));
        return;
    }

    try {
        // 4. 获取worksheet对应的数据库表名列表
        std::vector<std::string> worksheetTables = _fileBusiness->getWorksheetDBTables(fileId);

        // 5. 构建rpc响应
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));

        for (const auto& tableName : worksheetTables) {
            response->mutable_result()->add_worksheet_dbtables(tableName);
        }

        INF("GetWorksheetDBTables success: requestId={}, fileId={}, count={}", requestId, fileId, worksheetTables.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
    }
}

} // namespace fileService