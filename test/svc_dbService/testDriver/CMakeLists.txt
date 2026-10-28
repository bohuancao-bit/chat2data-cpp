cmake_minimum_required(VERSION 3.10)
project(dbDriverTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)
set(DB_DRIVER_DIR ${PROJECT_ROOT_DIR}/svc_dbService/dbDriver)

add_executable(${PROJECT_NAME}
    databaseSchemaTest.cc
    sqlValidatorTest.cc
    databaseFactoryTest.cc
    mysqlDatabaseTest.cc
    sqliteDatabaseTest.cc
    ${DB_DRIVER_DIR}/databaseSchema.cc
    ${PROJECT_ROOT_DIR}/common/sqlValidator.cc
    ${DB_DRIVER_DIR}/databaseFactory.cc
    ${DB_DRIVER_DIR}/mysqlDatabase.cc
    ${DB_DRIVER_DIR}/sqliteDatabase.cc
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${DB_DRIVER_DIR}
    ${GTEST_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${PROJECT_ROOT_DIR}/libSource/cpp-microservice-scaffold/source
    ${PROJECT_ROOT_DIR}/common
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    mysqlclient
    sqlite3
    gtest
    jsoncpp
    pthread
    fmt
    bite_scaffold
)
