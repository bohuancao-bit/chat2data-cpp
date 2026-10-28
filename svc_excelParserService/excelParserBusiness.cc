// #include <bite_scaffold/fdfs.h>
// #ifdef byte
// #undef byte
// #endif

#include <bite_scaffold/log.h>
#include "../common/errorHandler.h"
#include "excelParserBusiness.h"
#include <bite_scaffold/fdfs.h>

namespace excelParserService {

ExcelParserBusiness::ExcelParserBusiness()
    : _excelParser(std::make_unique<ExcelParser>()) {
    INF("ExcelParserBusiness initialized");
}

ExcelParserBusiness::~ExcelParserBusiness() {
    INF("ExcelParserBusiness destroyed");
}

std::vector<std::string> ExcelParserBusiness::getWorksheets(const std::string& fdfsFileId) {
    // 1. 检查FastDFS文件ID是否为空
    if (fdfsFileId.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
    }

    // 2. 从FastDFS下载文件到本地临时文件
    std::string tempFilePath = "/tmp/excel_temp_" + std::to_string(time(nullptr)) + ".xlsx";
    if (!bitefdfs::FDFSClient::download_to_file(fdfsFileId, tempFilePath)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_OPEN_FAILED);
    }

    // 3. 通过文件路径调用ExcelParser器获取所有工作表名称
    auto worksheets = _excelParser->getWorksheetNames(tempFilePath);
    if (worksheets.empty()) {
        std::remove(tempFilePath.c_str());
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_OPEN_FAILED);
    }

    // 4. 删除临时文件
    std::remove(tempFilePath.c_str());

    // 5. 返回所有工作表名称
    INF("ExcelParserBusiness::getWorksheets found {} worksheets", worksheets.size());
    return worksheets;
}

std::vector<chat2Data::excelParseService::WorksheetData> ExcelParserBusiness::parseExcel(
    const std::string& fdfsFileId,
    const std::vector<std::string>& worksheets) {
    // 1. 检查FastDFS文件ID是否为空
    if (fdfsFileId.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_PATH_INVALID);
    }

    // 2. 检查工作表名称是否为空
    if (worksheets.empty()) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_DATA_INVALID);
    }

    // 3. 从FastDFS下载文件到本地临时文件
    std::string tempFilePath = "/tmp/excel_temp_" + std::to_string(time(nullptr)) + ".xlsx";
    if (!bitefdfs::FDFSClient::download_to_file(fdfsFileId, tempFilePath)) {
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_FILE_OPEN_FAILED);
    }

    // 4. 调用ExcelParser器解析每个工作表数据
    std::vector<chat2Data::excelParseService::WorksheetData> result;
    for (const auto& worksheetName : worksheets) {
        // 4.1 调用ExcelParser器解析worksheetName工作表数据
        auto worksheetInfo = _excelParser->parseWorksheet(tempFilePath, worksheetName);
        if (!worksheetInfo) {
            std::remove(tempFilePath.c_str());
            throw chat2Data::Chat2DataException(chat2Data::ErrorCode::EXCEL_PARSE_FAILED);
        }

        // 4.2 将WorksheetInfo转换为Proto格式的WorksheetData
        result.push_back(convertToProtoWorksheetData(worksheetInfo));
    }

    // 5. 删除临时文件
    std::remove(tempFilePath.c_str());

    INF("ExcelParserBusiness::parseExcel parsed {} worksheets", result.size());
    return result;
}

chat2Data::excelParseService::WorksheetData ExcelParserBusiness::convertToProtoWorksheetData(
    const std::unique_ptr<WorksheetInfo>& worksheetInfo) {

    chat2Data::excelParseService::WorksheetData protoData;

    // 1. 设置worksheet名称以及总行数和总列数
    protoData.set_name(worksheetInfo->name);
    protoData.set_total_rows(worksheetInfo->totalRows);
    protoData.set_total_cols(worksheetInfo->totalCols);

    // 2. 设置worksheet列信息
    for (const auto& column : worksheetInfo->columns) {
        auto* protoColumn = protoData.add_columns();
        protoColumn->set_name(column.name);
        protoColumn->set_type(column.type);
    }

    // 3. 设置worksheet行数据
    for (const auto& row : worksheetInfo->rows) {
        auto* protoRow = protoData.add_rows();
        for (const auto& cell : row.cells) {
            auto* protoCell = protoRow->add_cells();
            protoCell->set_value(cell.value);
            protoCell->set_type(cell.type);
        }
    }

    // 4. 返回Proto格式的WorksheetData
    return protoData;
}

} // namespace excelParserService