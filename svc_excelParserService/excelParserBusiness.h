#pragma once
#include <memory>
#include <string>
#include <vector>
#include "excelParser.h"
#include "../proto/protoCode/excelParseService.pb.h"

namespace excelParserService {

class ExcelParserBusiness {
public:
    ExcelParserBusiness();
    ~ExcelParserBusiness();
    // 获取Excel文件中的所有工作表名称（通过FastDFS文件ID）
    std::vector<std::string> getWorksheets(const std::string& fdfsFileId);
    // 解析Excel文件中的指定工作表数据（通过FastDFS文件ID）
    std::vector<chat2Data::excelParseService::WorksheetData> parseExcel(
        const std::string& fdfsFileId,
        const std::vector<std::string>& worksheets);

private:
    // 将WorksheetInfo转换为Proto格式的WorksheetData
    chat2Data::excelParseService::WorksheetData convertToProtoWorksheetData(
        const std::unique_ptr<WorksheetInfo>& worksheetInfo);

private:
    std::unique_ptr<ExcelParser> _excelParser;    // excel解析器实例
};

} // namespace excelParserService