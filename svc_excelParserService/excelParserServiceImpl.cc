#include "excelParserServiceImpl.h"
#include <brpc/controller.h>
#include <bite_scaffold/log.h>
#include "../common/errorHandler.h"

namespace excelParserService {

ExcelParserServiceImpl::ExcelParserServiceImpl(std::shared_ptr<ExcelParserBusiness> business)
    : _business(business) {
    INF("ExcelParserServiceImpl initialized");
}

ExcelParserServiceImpl::~ExcelParserServiceImpl() {
    INF("ExcelParserServiceImpl destroyed");
}

void ExcelParserServiceImpl::GetWorksheets(google::protobuf::RpcController* controller,
                                          const chat2Data::excelParseService::GetWorksheetsRequest* request,
                                          chat2Data::excelParseService::GetWorksheetsResponse* response,
                                          google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 从请求中获取请求ID和FastDFS文件ID
    std::string requestId = request->request_id();
    std::string fdfsFileId = request->fdfs_file_id();

    try {
        // 3. 验证FastDFS文件ID是否为空
        if (fdfsFileId.empty()) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
        }

        // 4. 调用业务逻辑层获取worksheet表名列表
        auto worksheets = _business->getWorksheets(fdfsFileId);

        // 5. 构造rpc响应
        for (const auto& worksheet : worksheets) {
            response->add_worksheets(worksheet);
        }
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        INF("GetWorksheets success: requestId={}, fdfsFileId={}, worksheetCount={}",
            requestId, fdfsFileId, worksheets.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
        ERR("GetWorksheets failed: {}", e.what());
    }
}

void ExcelParserServiceImpl::ParseExcel(google::protobuf::RpcController* controller,
                                       const chat2Data::excelParseService::ParseExcelRequest* request,
                                       chat2Data::excelParseService::ParseExcelResponse* response,
                                       google::protobuf::Closure* done) {
    // 1. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard done_guard(done);

    // 2. 从请求中获取请求ID、文件路径和worksheet表名列表
    std::string requestId = request->request_id();
    std::string fdfsFileId = request->fdfs_file_id();
    std::vector<std::string> worksheets(request->worksheets().begin(), request->worksheets().end());

    try {
        // 3. 验证FastDFS文件ID和worksheet表名列表是否为空
        if (fdfsFileId.empty()) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
        }
        if (worksheets.empty()) {
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_DATA_INVALID);
        }

        // 4. 调用业务逻辑层解析excel文件各个worksheet表数据
        auto worksheetDataList = _business->parseExcel(fdfsFileId, worksheets);

        // 5. 构造rpc响应
        for (auto& worksheetData : worksheetDataList) {
            auto* protoWorksheet = response->add_worksheets();
            protoWorksheet->CopyFrom(worksheetData);
        }
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(chat2Data::ErrorCode::SUCCESS));
        INF("ParseExcel success: requestId={}, fdfsFileId={}, worksheetCount={}",
            requestId, fdfsFileId, worksheetDataList.size());
    } catch (const chat2Data::Chat2DataException& e) {
        response->set_request_id(requestId);
        response->set_error_code(static_cast<int32_t>(e.getErrorCode()));
        response->set_error_msg(chat2Data::error2String(e.getErrorCode()));
        ERR("ParseExcel failed: {}", e.what());
    }
}

} // namespace excelParserService