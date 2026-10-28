#pragma once

#include <string>
#include <cstddef>
#include <odb/core.hxx>

// 数据库表名：tbl_fileInfo
#pragma db object table("tbl_fileInfo")
class FileInfoEntity {
public:
    FileInfoEntity() {}
    FileInfoEntity(const std::string& fileId,
                   const std::string& fileName,
                   const std::string& fileExt,
                   int64_t fileSize,
                   int64_t uploadTime,
                   const std::string& fdfsFileId,
                   const std::string& userId,
                   const std::string& chatSessionId)
        : _fileId(fileId), _fileName(fileName), _fileExt(fileExt),
          _fileSize(fileSize), _uploadTime(uploadTime),
          _fdfsFileId(fdfsFileId), _userId(userId), _chatSessionId(chatSessionId) {}

    unsigned long long id() const { return _id; }
    void setId(unsigned long long id) { _id = id; }

    std::string fileId() const { return _fileId; }
    void setFileId(const std::string& fileId) { _fileId = fileId; }

    std::string fileName() const { return _fileName; }
    void setFileName(const std::string& fileName) { _fileName = fileName; }

    std::string fileExt() const { return _fileExt; }
    void setFileExt(const std::string& fileExt) { _fileExt = fileExt; }

    int64_t fileSize() const { return _fileSize; }
    void setFileSize(int64_t fileSize) { _fileSize = fileSize; }

    int64_t uploadTime() const { return _uploadTime; }
    void setUploadTime(int64_t uploadTime) { _uploadTime = uploadTime; }

    std::string fdfsFileId() const { return _fdfsFileId; }
    void setFdfsFileId(const std::string& fdfsFileId) { _fdfsFileId = fdfsFileId; }

    std::string userId() const { return _userId; }
    void setUserId(const std::string& userId) { _userId = userId; }

    std::string chatSessionId() const { return _chatSessionId; }
    void setChatSessionId(const std::string& chatSessionId) { _chatSessionId = chatSessionId; }

private:
    // 友元类：odb::access能够直接访问私有成员变量
    friend class odb::access;

    // 主键：id
    #pragma db id auto
    unsigned long long _id;

    // 唯一索引：fileId
    #pragma db unique
    #pragma db column("fileId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _fileId;

    #pragma db column("fileName") type("VARCHAR(64) CHARACTER SET utf8mb4")
    std::string _fileName;

    #pragma db column("fileExt") type("VARCHAR(8) CHARACTER SET utf8mb4")
    std::string _fileExt;

    #pragma db column("fileSize") type("BIGINT UNSIGNED")
    int64_t _fileSize;

    #pragma db column("uploadTime") type("BIGINT UNSIGNED")
    int64_t _uploadTime;

    #pragma db column("fdfsFileId") type("VARCHAR(64) CHARACTER SET utf8mb4") null
    std::string _fdfsFileId;

    #pragma db column("userId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _userId;

    #pragma db column("chatSessionId") type("VARCHAR(32) CHARACTER SET utf8mb4") null
    std::string _chatSessionId;
};