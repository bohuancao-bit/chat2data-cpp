#pragma once

#include <string>
#include <cstddef>
#include <odb/core.hxx>
#include <odb/nullable.hxx>

// 数据库表名：tbl_chatSession
#pragma db object table("tbl_chatSession")
class ChatSessionEntity {
public:
    ChatSessionEntity() {}
    ChatSessionEntity(const std::string& chatSessionId,
                     const std::string& userId,
                     const std::string& title,
                     int64_t createTime,
                     int64_t updateTime,
                     int messageCount,
                     const std::string& modelName,
                     const std::string& fileId,
                     const std::string& sessionType,
                     const std::string& dbConnectionInfo)
        : _chatSessionId(chatSessionId), _userId(userId), _title(title),
          _createTime(createTime), _updateTime(updateTime), _messageCount(messageCount),
          _modelName(modelName), _fileId(fileId), _sessionType(sessionType),
          _dbConnectionInfo(dbConnectionInfo) {}

    unsigned long long id() const { return _id; }
    void setId(unsigned long long id) { _id = id; }

    std::string chatSessionId() const { return _chatSessionId; }
    void setChatSessionId(const std::string& chatSessionId) { _chatSessionId = chatSessionId; }

    std::string userId() const { return _userId; }
    void setUserId(const std::string& userId) { _userId = userId; }

    std::string title() const { return _title; }
    void setTitle(const std::string& title) { _title = title; }

    int64_t createTime() const { return _createTime; }
    void setCreateTime(int64_t createTime) { _createTime = createTime; }

    int64_t updateTime() const { return _updateTime; }
    void setUpdateTime(int64_t updateTime) { _updateTime = updateTime; }

    int messageCount() const { return _messageCount; }
    void setMessageCount(int messageCount) { _messageCount = messageCount; }

    std::string modelName() const { return _modelName; }
    void setModelName(const std::string& modelName) { _modelName = modelName; }

    std::string fileId() const { return _fileId.null() ? "" : *_fileId; }
    void setFileId(const std::string& fileId) { 
        if (fileId.empty()) {
            _fileId.reset();
        } else {
            _fileId = fileId;
        }
    }

    std::string sessionType() const { return _sessionType; }
    void setSessionType(const std::string& sessionType) { _sessionType = sessionType; }

    std::string dbConnectionInfo() const { return _dbConnectionInfo; }
    void setDbConnectionInfo(const std::string& dbConnectionInfo) { _dbConnectionInfo = dbConnectionInfo; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long long _id;

    #pragma db unique
    #pragma db column("chatSessionId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _chatSessionId;

    #pragma db column("userId") type("VARCHAR(32) CHARACTER SET utf8mb4")
    std::string _userId;

    #pragma db column("title") type("TEXT CHARACTER SET utf8mb4") null
    std::string _title;

    #pragma db column("createTime") type("BIGINT UNSIGNED")
    int64_t _createTime;

    #pragma db column("updateTime") type("BIGINT UNSIGNED")
    int64_t _updateTime;

    #pragma db column("messageCount") type("INT UNSIGNED")
    int _messageCount;

    #pragma db column("modelName") type("TEXT CHARACTER SET utf8mb4")
    std::string _modelName;

    #pragma db column("fileId") type("VARCHAR(32) CHARACTER SET utf8mb4") null
    odb::nullable<std::string> _fileId;

    #pragma db column("sessionType") type("VARCHAR(20) CHARACTER SET utf8mb4")
    std::string _sessionType;

    #pragma db column("dbConnectionInfo") type("TEXT CHARACTER SET utf8mb4") null
    std::string _dbConnectionInfo;
};

/*
* 在生成的sql文件中添加：
ALTER TABLE `tbl_chatSession`
  ADD CONSTRAINT `tbl_chatSession_fileId_fk`
  FOREIGN KEY (`fileId`)
  REFERENCES `tbl_fileInfo` (`fileId`)
  ON DELETE CASCADE;
*/