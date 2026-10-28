#pragma once

#include <string>
#include <cstddef>
#include <odb/core.hxx>

// 指定该类映射到数据库中的tbl_user表
#pragma db object table("tbl_user")
class UserEntity {
    public:
        UserEntity() {}
        UserEntity(const std::string& userId,
                    const std::string& nickname,
                    const std::string& email,
                    const std::string& password,
                    unsigned char status)
            : _userId(userId), _nickname(nickname), _email(email),
              _password(password), _status(status) {}

        unsigned long long id() const { return _id; }
        void setId(unsigned long long id) { _id = id; }

        std::string userId() const { return _userId; }
        void setUserId(const std::string& userId) { _userId = userId; }

        std::string nickname() const { return _nickname; }
        void setNickname(const std::string& nickname) { _nickname = nickname; }

        std::string email() const { return _email; }
        void setEmail(const std::string& email) { _email = email; }

        std::string password() const { return _password; }
        void setPassword(const std::string& password) { _password = password; }

        unsigned char status() const { return _status; }
        void setStatus(unsigned char status) { _status = status; }

    private:
        friend class odb::access;

        // 主键自增
        #pragma db id auto
        unsigned long long _id;

        #pragma db unique
        #pragma db column("userId") type("VARCHAR(32) CHARACTER SET utf8mb4")
        std::string _userId;

        #pragma db unique
        #pragma db column("nickname") type("VARCHAR(32) CHARACTER SET utf8mb4")
        std::string _nickname;

        #pragma db unique
        #pragma db column("email") type("VARCHAR(255) CHARACTER SET utf8mb4")
        std::string _email;

        #pragma db column("password") type("TEXT CHARACTER SET utf8mb4")
        std::string _password;

        #pragma db column("status") type("TINYINT")
        int _status;
};