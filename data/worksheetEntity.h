#pragma once

#include <string>
#include <cstddef>
#include <odb/core.hxx>

// 数据库表名：tbl_worksheet
#pragma db object table("tbl_worksheet")
class WorksheetEntity {
public:
    WorksheetEntity() {}
    WorksheetEntity(const std::string& fileId,
                    const std::string& worksheetName,
                    const std::string& tableName)
        : _fileId(fileId), _worksheetName(worksheetName), _tableName(tableName) {}

    unsigned long long id() const { return _id; }
    void setId(unsigned long long id) { _id = id; }

    std::string fileId() const { return _fileId; }
    void setFileId(const std::string& fileId) { _fileId = fileId; }

    std::string worksheetName() const { return _worksheetName; }
    void setWorksheetName(const std::string& worksheetName) { _worksheetName = worksheetName; }

    std::string tableName() const { return _tableName; }
    void setTableName(const std::string& tableName) { _tableName = tableName; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long long _id;

    #pragma db column("fileId") type("VARCHAR(32) CHARACTER SET utf8mb4") index
    std::string _fileId;

    #pragma db column("worksheetName") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _worksheetName;

    #pragma db column("tableName") type("VARCHAR(64) CHARACTER SET utf8mb4")
    std::string _tableName;
};


/*
* 在生成的sql文件中添加：
ALTER TABLE `tbl_worksheet`
  ADD CONSTRAINT `tbl_worksheet_fileId_fk`
  FOREIGN KEY (`fileId`)
  REFERENCES `tbl_fileInfo` (`fileId`)
  ON DELETE CASCADE;
*/