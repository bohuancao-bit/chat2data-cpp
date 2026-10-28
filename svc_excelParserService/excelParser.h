#pragma once
#include <string>
#include <vector>
#include <memory>
#include <OpenXLSX.hpp>
#include "../proto/protoCode/excelParseService.pb.h"

namespace excelParserService {
// 单元格结构
struct CellData {
    std::string value;           // 单元格数值
    std::string type;            // 单元格类型
};

// 列类型结构
struct ColumnData {
    std::string name;            // 列名
    std::string type;            // 列类型
};

// worksheet的行数据结构
struct RowData {
    std::vector<CellData> cells; // 保存行数据--即worksheet中一行数据，包含多个单元格
};

// worksheet信息结构
struct WorksheetInfo {
    std::string name;                    // worksheet名称
    std::vector<ColumnData> columns;     // 列类型信息
    std::vector<RowData> rows;           // 行数据
    int totalRows = 0;                  // 总行数
    int totalCols = 0;                  // 总列数
};

class ExcelParser {
public:
    ExcelParser();
    ~ExcelParser();
    // 获取excel文件中所有worksheet名称
    std::vector<std::string> getWorksheetNames(const std::string& filePath);
    // 解析指定worksheet数据
    std::unique_ptr<WorksheetInfo> parseWorksheet(const std::string& filePath, const std::string& worksheetName);

private:
    // 验证列名是否符合要求
    std::string validateColumnName(const std::string& name, int columnIndex);
    // 移除字符串首尾空白字符
    std::string trimWhitespace(const std::string& str);
    // 检测单元格数值是否为数值类型
    bool isNumericString(const std::string& str);
    // 检测单元格数值是否为布尔类型
    bool isBooleanString(const std::string& str);
    // 检测单元格数值是否为日期类型
    bool isDateString(const std::string& str);
    // 检测表头类类型
    std::string inferColumnType(const std::vector<CellData>& cellValues);
    // 解析指定单元格
    CellData parseCellValue(const OpenXLSX::XLCellValue& value);
    // 解析worksheet表头
    std::vector<CellData> parseHeaderRow(OpenXLSX::XLWorksheet& worksheet, int rowNumber, int columnCount);
    // 解析worksheet数据行
    std::vector<CellData> parseDataRow(OpenXLSX::XLWorksheet& worksheet, int rowNumber, int columnCount);
};

} // namespace excelParserService