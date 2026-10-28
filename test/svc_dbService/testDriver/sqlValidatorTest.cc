#include <gtest/gtest.h>
#include <string>
#include "sqlValidator.h"

using namespace databaseService;

class SqlValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(SqlValidatorTest, T066_Trim_LeftSpaces) {
    // trim函数能去除左侧空格
    std::string result = SQLValidator::trim("  SELECT * FROM users");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T067_Trim_RightSpaces) {
    // trim函数能去除右侧空格
    std::string result = SQLValidator::trim("SELECT * FROM users  ");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T068_Trim_BothSpaces) {
    // trim函数能同时去除两侧空格
    std::string result = SQLValidator::trim("  SELECT * FROM users  ");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T069_Trim_TabChars) {
    // trim函数能去除Tab字符
    std::string result = SQLValidator::trim("\tSELECT * FROM users\t");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T070_Trim_NewlineChars) {
    // trim函数能去除换行符
    std::string result = SQLValidator::trim("\nSELECT * FROM users\n");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T071_Trim_AllSpaces) {
    // trim函数处理全空格字符串返回空字符串
    std::string result = SQLValidator::trim("     ");
    EXPECT_EQ(result, "");
}

TEST_F(SqlValidatorTest, T072_Trim_EmptyString) {
    // trim函数处理空字符串返回空字符串
    std::string result = SQLValidator::trim("");
    EXPECT_EQ(result, "");
}

TEST_F(SqlValidatorTest, T073_RemoveComments_SingleLine) {
    // removeComments函数能移除单行注释
    std::string result = SQLValidator::removeComments("SELECT * FROM users -- 这是注释");
    EXPECT_EQ(result, "SELECT * FROM users ");
}

TEST_F(SqlValidatorTest, T074_RemoveComments_MultiLine) {
    // removeComments函数能移除多行注释
    std::string result = SQLValidator::removeComments("SELECT /* comment */ * FROM users");
    EXPECT_EQ(result, "SELECT  * FROM users");
}

TEST_F(SqlValidatorTest, T075_RemoveComments_StringContent) {
    // removeComments函数不会移除字符串内的注释符号
    std::string result = SQLValidator::removeComments("SELECT * FROM users WHERE name='-- test'");
    EXPECT_EQ(result, "SELECT * FROM users WHERE name='-- test'");
}

TEST_F(SqlValidatorTest, T076_RemoveComments_EscapedQuote) {
    // removeComments函数能正确处理转义引号
    std::string result = SQLValidator::removeComments("SELECT * FROM users WHERE name='it''s'");
    EXPECT_EQ(result, "SELECT * FROM users WHERE name='it''s'");
}

TEST_F(SqlValidatorTest, T077_RemoveComments_MultipleSingleLine) {
    // removeComments函数能处理多个单行注释
    std::string result = SQLValidator::removeComments("SELECT * -- comment1\nFROM -- comment2\nusers");
    EXPECT_EQ(result, "SELECT * \nFROM \nusers");
}

TEST_F(SqlValidatorTest, T078_RemoveComments_NestedComments) {
    // removeComments函数处理嵌套注释（部分支持）
    std::string result = SQLValidator::removeComments("SELECT * FROM users /* outer /* inner */ outer */");
    EXPECT_EQ(result, "SELECT * FROM users  outer */");
}

TEST_F(SqlValidatorTest, T079_RemoveComments_NoComment) {
    // removeComments函数处理无注释的SQL
    std::string result = SQLValidator::removeComments("SELECT * FROM users");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T080_Normalize_CommentAndSpaces) {
    // normalize函数能同时去除注释和多余空格
    std::string result = SQLValidator::normalize("  SELECT * FROM users -- comment  ");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T081_Normalize_MultiLineComment) {
    // normalize函数能处理多行注释
    std::string result = SQLValidator::normalize("  /* comment */ SELECT * FROM users  ");
    EXPECT_EQ(result, "SELECT * FROM users");
}

TEST_F(SqlValidatorTest, T082_Validate_ValidSelect) {
    // validate函数能验证有效的SELECT语句
    bool result = SQLValidator::validate("SELECT * FROM users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T083_Validate_ValidInsert) {
    // validate函数能验证有效的INSERT语句
    bool result = SQLValidator::validate("INSERT INTO users(name) VALUES('test')");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T084_Validate_ValidUpdate) {
    // validate函数能验证有效的UPDATE语句
    bool result = SQLValidator::validate("UPDATE users SET name='test' WHERE id=1");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T085_Validate_ValidDelete) {
    // validate函数能验证有效的DELETE语句
    bool result = SQLValidator::validate("DELETE FROM users WHERE id=1");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T086_Validate_EmptyString) {
    // validate函数对空字符串返回false
    bool result = SQLValidator::validate("");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T087_Validate_AllSpaces) {
    // validate函数对全空格字符串返回false
    bool result = SQLValidator::validate("     ");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T088_Validate_ContainsDrop) {
    // validate函数对包含DROP的语句返回false
    bool result = SQLValidator::validate("DROP TABLE users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T089_Validate_ContainsDangerousKeywords) {
    // validate函数对包含危险关键字的语句返回false
    bool result = SQLValidator::validate("SELECT * FROM users; DELETE FROM users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T090_Validate_MultipleStatements) {
    // validate函数对多条语句返回false
    bool result = SQLValidator::validate("SELECT * FROM users; SELECT * FROM orders");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T091_GetSQLType_Select) {
    // getSQLType函数能识别SELECT语句
    SqlType type = SQLValidator::getSQLType("SELECT * FROM users");
    EXPECT_EQ(type, SqlType::SELECT);
}

TEST_F(SqlValidatorTest, T092_GetSQLType_Insert) {
    // getSQLType函数能识别INSERT语句
    SqlType type = SQLValidator::getSQLType("INSERT INTO users VALUES(1)");
    EXPECT_EQ(type, SqlType::INSERT);
}

TEST_F(SqlValidatorTest, T093_GetSQLType_Update) {
    // getSQLType函数能识别UPDATE语句
    SqlType type = SQLValidator::getSQLType("UPDATE users SET name='test'");
    EXPECT_EQ(type, SqlType::UPDATE);
}

TEST_F(SqlValidatorTest, T094_GetSQLType_Delete) {
    // getSQLType函数能识别DELETE语句
    SqlType type = SQLValidator::getSQLType("DELETE FROM users WHERE id=1");
    EXPECT_EQ(type, SqlType::DELETE);
}

TEST_F(SqlValidatorTest, T095_GetSQLType_Replace) {
    // getSQLType函数能识别REPLACE语句
    SqlType type = SQLValidator::getSQLType("REPLACE INTO users VALUES(1)");
    EXPECT_EQ(type, SqlType::REPLACE);
}

TEST_F(SqlValidatorTest, T096_GetSQLType_Truncate) {
    // getSQLType函数能识别TRUNCATE语句
    SqlType type = SQLValidator::getSQLType("TRUNCATE TABLE users");
    EXPECT_EQ(type, SqlType::TRUNCATE);
}

TEST_F(SqlValidatorTest, T097_GetSQLType_Create) {
    // getSQLType函数能识别CREATE语句
    SqlType type = SQLValidator::getSQLType("CREATE TABLE users(id INT)");
    EXPECT_EQ(type, SqlType::CREATE);
}

TEST_F(SqlValidatorTest, T098_GetSQLType_Drop) {
    // getSQLType函数能识别DROP语句
    SqlType type = SQLValidator::getSQLType("DROP TABLE users");
    EXPECT_EQ(type, SqlType::DROP);
}

TEST_F(SqlValidatorTest, T099_GetSQLType_Alter) {
    // getSQLType函数能识别ALTER语句
    SqlType type = SQLValidator::getSQLType("ALTER TABLE users ADD name VARCHAR(100)");
    EXPECT_EQ(type, SqlType::ALTER);
}

TEST_F(SqlValidatorTest, T100_GetSQLType_Show) {
    // getSQLType函数能识别SHOW语句
    SqlType type = SQLValidator::getSQLType("SHOW TABLES");
    EXPECT_EQ(type, SqlType::SHOW);
}

TEST_F(SqlValidatorTest, T101_GetSQLType_Desc) {
    // getSQLType函数能识别DESC语句
    SqlType type = SQLValidator::getSQLType("DESC users");
    EXPECT_EQ(type, SqlType::DESC);
}

TEST_F(SqlValidatorTest, T102_GetSQLType_Unknown) {
    // getSQLType函数对未知类型返回UNKNOWN
    SqlType type = SQLValidator::getSQLType("UNKNOWN SQL");
    EXPECT_EQ(type, SqlType::UNKNOWN);
}

TEST_F(SqlValidatorTest, T103_IsReadOnly_Select) {
    // isReadOnly函数对SELECT语句返回true
    bool result = SQLValidator::isReadOnly("SELECT * FROM users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T104_IsReadOnly_Show) {
    // isReadOnly函数对SHOW语句返回true
    bool result = SQLValidator::isReadOnly("SHOW TABLES");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T105_IsReadOnly_Desc) {
    // isReadOnly函数对DESC语句返回true
    bool result = SQLValidator::isReadOnly("DESC users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T106_IsReadOnly_Insert) {
    // isReadOnly函数对INSERT语句返回false
    bool result = SQLValidator::isReadOnly("INSERT INTO users VALUES(1)");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T107_IsReadOnly_Update) {
    // isReadOnly函数对UPDATE语句返回false
    bool result = SQLValidator::isReadOnly("UPDATE users SET name='test'");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T108_IsReadOnly_Delete) {
    // isReadOnly函数对DELETE语句返回false
    bool result = SQLValidator::isReadOnly("DELETE FROM users WHERE id=1");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T109_IsModifySQL_Select) {
    // isModifySQL函数对SELECT语句返回false
    bool result = SQLValidator::isModifySQL("SELECT * FROM users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T110_IsModifySQL_Insert) {
    // isModifySQL函数对INSERT语句返回true
    bool result = SQLValidator::isModifySQL("INSERT INTO users VALUES(1)");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T111_IsModifySQL_Update) {
    // isModifySQL函数对UPDATE语句返回true
    bool result = SQLValidator::isModifySQL("UPDATE users SET name='test'");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T112_IsModifySQL_Delete) {
    // isModifySQL函数对DELETE语句返回true
    bool result = SQLValidator::isModifySQL("DELETE FROM users WHERE id=1");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T113_IsModifySQL_Unknown) {
    // isModifySQL函数对未知类型返回false
    bool result = SQLValidator::isModifySQL("UNKNOWN SQL");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T114_ContainsDangerousKeywords_NoDanger) {
    // containsDangerousKeywords函数对安全SQL返回false
    bool result = SQLValidator::containsDangerousKeywords("SELECT * FROM users WHERE id=1");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T115_ContainsDangerousKeywords_DropDatabase) {
    // containsDangerousKeywords函数能检测DROP DATABASE
    bool result = SQLValidator::containsDangerousKeywords("DROP DATABASE testdb");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T116_ContainsDangerousKeywords_DropTable) {
    // containsDangerousKeywords函数能检测DROP TABLE
    bool result = SQLValidator::containsDangerousKeywords("DROP TABLE users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T117_ContainsDangerousKeywords_Truncate) {
    // containsDangerousKeywords函数能检测TRUNCATE
    bool result = SQLValidator::containsDangerousKeywords("TRUNCATE TABLE users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T118_ContainsDangerousKeywords_Exec) {
    // containsDangerousKeywords函数能检测EXEC
    bool result = SQLValidator::containsDangerousKeywords("EXEC sp_name");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T119_ContainsDangerousKeywords_UnionAllSelect) {
    // containsDangerousKeywords函数能检测UNION ALL SELECT
    bool result = SQLValidator::containsDangerousKeywords("SELECT * FROM users UNION ALL SELECT * FROM orders");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T120_ContainsDangerousKeywords_Or11) {
    // containsDangerousKeywords函数能检测OR 1=1注入
    bool result = SQLValidator::containsDangerousKeywords("SELECT * FROM users WHERE id=1 OR 1=1");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T121_ContainsDangerousKeywords_Sleep) {
    // containsDangerousKeywords函数能检测SLEEP注入
    bool result = SQLValidator::containsDangerousKeywords("SELECT * FROM users; SELECT SLEEP(5)");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T122_ContainsDangerousKeywords_IntoOutfile) {
    // containsDangerousKeywords函数能检测INTO OUTFILE
    bool result = SQLValidator::containsDangerousKeywords("SELECT * FROM users INTO OUTFILE '/tmp/test.txt'");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T123_ContainsDangerousKeywords_CaseInsensitive) {
    // containsDangerousKeywords函数大小写不敏感
    bool result = SQLValidator::containsDangerousKeywords("drop table users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T124_HasMultipleStatements_Single) {
    // hasMultipleStatements函数对单条语句返回false
    bool result = SQLValidator::hasMultipleStatements("SELECT * FROM users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T125_HasMultipleStatements_Multiple) {
    // hasMultipleStatements函数对多条语句返回true
    bool result = SQLValidator::hasMultipleStatements("SELECT * FROM users; SELECT * FROM orders");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T126_HasMultipleStatements_SemicolonInString) {
    // hasMultipleStatements函数不会将字符串内的分号当作语句分隔符
    bool result = SQLValidator::hasMultipleStatements("SELECT 'a;b' FROM users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T127_HasMultipleStatements_EscapedQuoteWithSemicolon) {
    // hasMultipleStatements函数能正确处理转义引号中的分号
    bool result = SQLValidator::hasMultipleStatements("SELECT 'it''s' FROM users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T128_HasMultipleStatements_EmptyString) {
    // hasMultipleStatements函数对空字符串返回false
    bool result = SQLValidator::hasMultipleStatements("");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T129_HasMultipleStatements_OnlySemicolon) {
    // hasMultipleStatements函数对仅有分号返回false
    bool result = SQLValidator::hasMultipleStatements(";");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T130_HasMultipleStatements_SemicolonNoContent) {
    // hasMultipleStatements函数对只有空语句返回false
    bool result = SQLValidator::hasMultipleStatements("SELECT * FROM users; ; SELECT * FROM orders");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T131_ExtractTableName_InsertInto) {
    // extractTableName函数能从INSERT INTO提取表名
    std::string result = SQLValidator::extractTableName("INSERT INTO users(name) VALUES('test')");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T132_ExtractTableName_Update) {
    // extractTableName函数能从UPDATE提取表名
    std::string result = SQLValidator::extractTableName("UPDATE users SET name='test'");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T133_ExtractTableName_DeleteFrom) {
    // extractTableName函数能从DELETE FROM提取表名
    std::string result = SQLValidator::extractTableName("DELETE FROM users WHERE id=1");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T134_ExtractTableName_TruncateTable) {
    // extractTableName函数能从TRUNCATE TABLE提取表名
    std::string result = SQLValidator::extractTableName("TRUNCATE TABLE users");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T135_ExtractTableName_ReplaceInto) {
    // extractTableName函数能从REPLACE INTO提取表名
    std::string result = SQLValidator::extractTableName("REPLACE INTO users VALUES(1)");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T136_ExtractTableName_AlterTable) {
    // extractTableName函数能从ALTER TABLE提取表名
    std::string result = SQLValidator::extractTableName("ALTER TABLE users ADD name VARCHAR(100)");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T137_ExtractTableName_CreateTable) {
    // extractTableName函数能从CREATE TABLE提取表名
    std::string result = SQLValidator::extractTableName("CREATE TABLE users(id INT)");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T138_ExtractTableName_DropTable) {
    // extractTableName函数能从DROP TABLE提取表名
    std::string result = SQLValidator::extractTableName("DROP TABLE users");
    EXPECT_EQ(result, "users");
}

TEST_F(SqlValidatorTest, T139_ExtractTableName_QuotedTableName) {
    // extractTableName函数对SELECT语句返回空字符串（不支持查询类SQL）
    std::string result = SQLValidator::extractTableName("SELECT * FROM `users`");
    EXPECT_EQ(result, "");
}

TEST_F(SqlValidatorTest, T140_ExtractTableName_SelectNonModify) {
    // extractTableName函数对非修改型SELECT返回空字符串
    std::string result = SQLValidator::extractTableName("SELECT * FROM users");
    EXPECT_EQ(result, "");
}

TEST_F(SqlValidatorTest, T141_ExtractTableName_EmptyString) {
    // extractTableName函数对空字符串返回空字符串
    std::string result = SQLValidator::extractTableName("");
    EXPECT_EQ(result, "");
}

TEST_F(SqlValidatorTest, T142_IsValidTableName_Normal) {
    // isValidTableName函数对正常表名返回true
    bool result = SQLValidator::isValidTableName("users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T143_IsValidTableName_WithUnderscore) {
    // isValidTableName函数对带下划线的表名返回true
    bool result = SQLValidator::isValidTableName("user_accounts");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T144_IsValidTableName_WithDot) {
    // isValidTableName函数对带点的表名返回true
    bool result = SQLValidator::isValidTableName("db.users");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T145_IsValidTableName_WithHyphen) {
    // isValidTableName函数对带连字符的表名返回true（设计允许）
    bool result = SQLValidator::isValidTableName("user-accounts");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T146_IsValidTableName_ChineseName) {
    // isValidTableName函数对中文表名返回true（设计允许）
    bool result = SQLValidator::isValidTableName("用户表");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T147_IsValidTableName_LetterPrefix) {
    // isValidTableName函数对字母开头的表名返回true
    bool result = SQLValidator::isValidTableName("users123");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T148_IsValidTableName_Empty) {
    // isValidTableName函数对空字符串返回false
    bool result = SQLValidator::isValidTableName("");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T149_IsValidTableName_DigitPrefix) {
    // isValidTableName函数对数字开头的表名返回false
    bool result = SQLValidator::isValidTableName("123users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T150_IsValidTableName_ContainsSemicolon) {
    // isValidTableName函数对包含分号的表名返回false
    bool result = SQLValidator::isValidTableName("users; DROP TABLE users");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T151_IsValidTableName_ContainsQuote) {
    // isValidTableName函数对包含引号的表名返回false
    bool result = SQLValidator::isValidTableName("users'name");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T152_IsValidTableName_ContainsSpace) {
    // isValidTableName函数对包含空格的表名返回false
    bool result = SQLValidator::isValidTableName("user name");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T153_IsValidTableName_ContainsAsterisk) {
    // isValidTableName函数对包含星号的表名返回false
    bool result = SQLValidator::isValidTableName("users*");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T154_IsValidTableName_ContainsBackslash) {
    // isValidTableName函数对包含反斜杠的表名返回false
    bool result = SQLValidator::isValidTableName("users\\");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T155_IsValidTableName_ContainsSlash) {
    // isValidTableName函数对包含斜杠的表名返回false
    bool result = SQLValidator::isValidTableName("users/");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T156_IsValidTableName_ConsecutiveDots) {
    // isValidTableName函数对包含连续点的表名返回false
    bool result = SQLValidator::isValidTableName("users..data");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T157_IsValidTableName_ConsecutiveHyphens) {
    // isValidTableName函数对包含连续连字符的表名返回false
    bool result = SQLValidator::isValidTableName("user--name");
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T158_IsValidTableName_TooLong) {
    // isValidTableName函数对过长表名返回false
    std::string longName(65, 'a');
    bool result = SQLValidator::isValidTableName(longName);
    EXPECT_FALSE(result);
}

TEST_F(SqlValidatorTest, T159_IsValidColumnName_Valid) {
    // isValidColumnName函数对有效列名返回true
    bool result = SQLValidator::isValidColumnName("user_name");
    EXPECT_TRUE(result);
}

TEST_F(SqlValidatorTest, T160_IsValidColumnName_Invalid) {
    // isValidColumnName函数对包含空格的列名返回false
    bool result = SQLValidator::isValidColumnName("user name");
    EXPECT_FALSE(result);
}
