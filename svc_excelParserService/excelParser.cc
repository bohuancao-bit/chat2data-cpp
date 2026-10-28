#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <bite_scaffold/log.h>
#include "excelParser.h"

namespace excelParserService {

ExcelParser::ExcelParser() {
}

ExcelParser::~ExcelParser() {
}

std::vector<std::string> ExcelParser::getWorksheetNames(const std::string& filePath) {
    std::vector<std::string> names;

    try {
        // 1. 打开Excel文件
        OpenXLSX::XLDocument doc;
        doc.open(filePath);
        if (!doc.isOpen()) {
            ERR("Failed to open Excel file: {}", filePath);
            return names;
        }

        // 2. 获取工作簿对象
        auto workbook = doc.workbook();

        // 3. 获取工作表名称
        names = workbook.worksheetNames();
        INF("Successfully retrieved {} worksheet names from file: {}", names.size(), filePath);

        // 4. 关闭文件
        doc.close();
    } catch (const OpenXLSX::XLException& e) {
        ERR("OpenXLSX exception while getting worksheet names: {}", e.what());
    } catch (const std::exception& e) {
        ERR("Exception while getting worksheet names: {}", e.what());
    }

    return names;
}

std::unique_ptr<WorksheetInfo> ExcelParser::parseWorksheet(const std::string& filePath, const std::string& worksheetName) {
    auto worksheetInfo = std::make_unique<WorksheetInfo>();

    try {
        // 1. 打开Excel文件
        OpenXLSX::XLDocument doc;
        doc.open(filePath);
        if (!doc.isOpen()) {
            ERR("Failed to open Excel file: {}", filePath);
            return nullptr;
        }

        // 2. 获取工作簿对象
        auto workbook = doc.workbook();

        // 3. 检查工作表是否存在
        if (!workbook.worksheetExists(worksheetName)) {
            ERR("Worksheet '{}' not found in file: {}", worksheetName, filePath);
            doc.close();
            return nullptr;
        }

        // 4. 获取工作表对象
        auto worksheet = workbook.worksheet(worksheetName);
        worksheetInfo->name = worksheetName;
        worksheetInfo->totalRows = static_cast<int>(worksheet.rowCount());
        worksheetInfo->totalCols = static_cast<int>(worksheet.columnCount());
        INF("Parsing worksheet '{}' with {} rows and {} columns",
            worksheetName, worksheetInfo->totalRows, worksheetInfo->totalCols);
        if (worksheetInfo->totalRows == 0 || worksheetInfo->totalCols == 0) {
            ERR("Worksheet '{}' is empty", worksheetName);
            doc.close();
            return nullptr;
        }

        // 5. 解析worksheet表头。表头内容为worksheet中的第一行
        std::vector<CellData> headerCells = parseHeaderRow(worksheet, 1, worksheetInfo->totalCols);

        // 6. 检测表头字段名称是否符合要求
        for (int i = 0; i < headerCells.size(); ++i) {
            ColumnData column;
            column.name = validateColumnName(headerCells[i].value, i + 1);
            worksheetInfo->columns.push_back(column);
        }

        // 7. 采样100行数据，用于推断列类型
        std::vector<std::vector<CellData>> sampledRows;
        int sampleRowCount = std::min(100, worksheetInfo->totalRows - 1);
        for (int i = 2; i <= sampleRowCount + 1; ++i) {
            sampledRows.push_back(parseDataRow(worksheet, i, worksheetInfo->totalCols));
        }

        // 从采样数据中拿到各个列的所有数据，然后推理列类型
        for (int col = 0; col < worksheetInfo->columns.size(); ++col) {
            std::vector<CellData> columnValues;
            for (const auto& row : sampledRows) {
                if (col < row.size()) {
                    columnValues.push_back(row[col]);
                }
            }
            worksheetInfo->columns[col].type = inferColumnType(columnValues);
        }

        // 8. 解析worksheet的表数据。解析时需跳过表头
        for (int i = 2; i <= worksheetInfo->totalRows; ++i) {
            RowData rowData;
            rowData.cells = parseDataRow(worksheet, i, worksheetInfo->totalCols);
            worksheetInfo->rows.push_back(std::move(rowData));
        }

        INF("Successfully parsed worksheet '{}': {} columns, {} rows",
            worksheetName, worksheetInfo->columns.size(), worksheetInfo->rows.size());

        // 9. 关闭文件
        doc.close();
    } catch (const OpenXLSX::XLException& e) {
        ERR("OpenXLSX exception while parsing worksheet: {}", e.what());
        return nullptr;
    } catch (const std::exception& e) {
        ERR("Exception while parsing worksheet: {}", e.what());
        return nullptr;
    }

    return worksheetInfo;
}

// 检测列名是否合法
std::string ExcelParser::validateColumnName(const std::string& name, int columnIndex) {
    // 1. 检测列名是否为空。空列名默认使用"column_索引"命名
    if (name.empty()) {
        return "column_" + std::to_string(columnIndex);
    }

    // 2. 检测列名是否包含无效字符。无效字符包括非字母、非数字、非下划线和非UTF-8字符
    // 特殊符号使用_替换
    std::string result;
    for (unsigned char c : name) {
        if (std::isalnum(c) || c == '_' || c >= 0x80 || c == '$') {
            result += c;
        } else {
            result += '_';
        }
    }

    // 3. 检测列名是否以数字开头。如果是，添加"col_"前缀
    if (!result.empty() && std::isdigit(result[0])) {
        result = "col_" + result;
    }

    // 4. 返回最终结果
    return result;
}

// 移除字符串首尾的空格
std::string ExcelParser::trimWhitespace(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();

    // 1. 从前往后找第一个非空白字符的位置
    while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    // 2. 从后往前找第一个非空白字符的位置
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }

    // 3. 截取字符窜中的非空白字符
    return str.substr(start, end - start);
}

// 检测字符串是否为数字
bool ExcelParser::isNumericString(const std::string& str) {
    // 1. 检测字符串是否为空
    if (str.empty()) {
        return false;
    }

    // 2. 移除字符串首尾的空格
    std::string trimmed = trimWhitespace(str);
    if (trimmed.empty()) {
        return false;
    }

    // 3. 检测字符串是否为数字
    // 3.1 检测字符串是否以正负号开头
    size_t pos = 0;
    if (trimmed[0] == '-' || trimmed[0] == '+') {
        pos = 1;
    }
    if (pos >= trimmed.length()) {
        return false;
    }

    // 3.2 检测字符串是否包含数字和小数点
    size_t dotPos = std::string::npos;
    size_t digitCount = 0;
    while (pos < trimmed.length()) {
        if (std::isdigit(static_cast<unsigned char>(trimmed[pos]))) {
            ++digitCount;
        } else if (trimmed[pos] == '.' && dotPos == std::string::npos) {  // 123.45.678
            dotPos = pos;
        } else {
            return false;
        }
        ++pos;
    }

    // 3.3 检测字符串是否包含数字
    if (digitCount == 0) {
        return false;
    }

    // 3.4 截取字符串中数值部分
    std::string afterNumber = trimmed.substr(pos);
    afterNumber = trimWhitespace(afterNumber);

    // 3.5 返回是否为数值的结果
    return afterNumber.empty();
}

// 检测字符串是否为布尔值
bool ExcelParser::isBooleanString(const std::string& str) {
    // 1. 将字符串转换为小写
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // 2. 检测字符串是否为布尔值
    if (lower == "true" || lower == "1" || lower == "t" || lower == "yes" || lower == "y") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "f" || lower == "no" || lower == "n") {
        return true;
    }

    return false;
}

// 检测字符串是否为日期类型--利用正则表达式检测
bool ExcelParser::isDateString(const std::string& str) {
    std::regex datePatterns[] = {
        std::regex(R"(\d{4}-\d{2}-\d{2})"),       // 2023-01-01
        std::regex(R"(\d{4}/\d{2}/\d{2})"),       // 2023/01/01
        std::regex(R"(\d{2}-\d{2}-\d{4})"),       // 01-01-2023
        std::regex(R"(\d{2}/\d{2}/\d{4})"),       // 01/01/2023
        std::regex(R"(\d{4}\.\d{2}\.\d{2})"),       // 2023.01.01
        std::regex(R"(\d{2}\.\d{2}\.\d{4})"),       // 01.01.2023
        std::regex(R"(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})"), // 2023-01-01 12:00:00
        std::regex(R"(\d{4}/\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2})"), // 2023/01/01 12:00:00
    };

    // 验证字符串是否符合正则表达式
    for (const auto& pattern : datePatterns) {
        if (std::regex_match(str, pattern)) {
            return true;
        }
    }

    return false;
}

// 推断列类型
std::string ExcelParser::inferColumnType(const std::vector<CellData>& cellValues) {
    // 0. 初始化类型计数器
    std::unordered_map<std::string, int> typeCount;

    for (const auto& cell : cellValues) {
        // 1. 空列值 以及 N/A值不考虑
        if (cell.value.empty() || cell.value == "N/A" || cell.value == "n/a") {
            continue;
        }

        // 2. 检测列值的类型
        std::string detectedType;
        if (cell.type == "Integer") {
            detectedType = "BIGINT";
        } else if (cell.type == "Float") {
            detectedType = "DOUBLE";
        } else if (cell.type == "Boolean") {
            detectedType = "BOOLEAN";
        } else if (cell.type == "String") {
            // 如果是字符串类型，需要进一步检测是否为数字、布尔值、日期
            if (isNumericString(cell.value)) {
                detectedType = "DOUBLE";
            } else if (isBooleanString(cell.value)) {
                detectedType = "BOOLEAN";
            } else if (isDateString(cell.value)) {
                detectedType = "DATE";
            } else {
                detectedType = "TEXT";   // 如果不是数字、布尔值、日期，认为是文本类型
            }
        } else {
            detectedType = "TEXT";
        }

        // 统计每个类型的出现次数
        ++typeCount[detectedType];
    }

    if (typeCount.empty()) {
        return "TEXT";
    }

    // 3. 找到出现次数最多的类型
    std::string maxType;
    int maxCount = 0;
    for (const auto& pair : typeCount) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            maxType = pair.first;
        }
    }

    // 4. 返回出现次数最多的类型
    return maxType;
}

// 解析指定单元格数值
CellData ExcelParser::parseCellValue(const OpenXLSX::XLCellValue& value) {
    CellData cell;

    // 1. 空单元格
    if (value.type() == OpenXLSX::XLValueType::Empty) {
        cell.type = "Empty";
        cell.value = "";
        return cell;
    }

    // 2. 布尔值
    if (value.type() == OpenXLSX::XLValueType::Boolean) {
        bool boolValue = value.get<bool>();
        cell.type = "Boolean";
        cell.value = boolValue ? "1" : "0";
        return cell;
    }

    // 3. 整数
    if (value.type() == OpenXLSX::XLValueType::Integer) {
        int64_t intValue = value.get<int64_t>();
        cell.type = "Integer";
        cell.value = std::to_string(intValue);
        return cell;
    }

    // 4. 浮点数
    if (value.type() == OpenXLSX::XLValueType::Float) {
        double floatValue = value.get<double>();
        cell.type = "Float";
        cell.value = std::to_string(floatValue);
        return cell;
    }

    try {
        // 5. 字符串
        cell.type = "String";
        cell.value = value.get<std::string>();
    } catch (...) {
        cell.type = "String";
        cell.value = "";
    }

    return cell;
}

std::vector<CellData> ExcelParser::parseHeaderRow(OpenXLSX::XLWorksheet& worksheet, int rowNumber, int columnCount) {
    std::vector<CellData> cells;
    // 逐个获取表头单元格数据并解析成CellData格式
    for (int i = 0; i < columnCount; ++i) {
        cells.push_back(parseCellValue(worksheet.cell(rowNumber, i + 1).value()));
    }

    return cells;
}

std::vector<CellData> ExcelParser::parseDataRow(OpenXLSX::XLWorksheet& worksheet, int rowNumber, int columnCount) {
    std::vector<CellData> cells;
    // 逐个获取数据行单元格数据并解析成CellData格式
    for (int i = 0; i < columnCount; ++i) {
        cells.push_back(parseCellValue(worksheet.cell(rowNumber, i + 1).value()));
    }

    return cells;
}

} // namespace excelParserService