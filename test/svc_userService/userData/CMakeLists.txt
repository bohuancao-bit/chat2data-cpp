# cmake版本最低要求为3.10.0
cmake_minimum_required(VERSION 3.10)
# 项目名称
project(userDataTest)
# C++标准为17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 项目根目录
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)
# 添加需要编译的源文件
add_executable(${PROJECT_NAME}
    userDataTest.cc
    ${PROJECT_ROOT_DIR}/data/userData.cc
    ${PROJECT_ROOT_DIR}/data/odb/userEntity-odb.cxx
    ${PROJECT_ROOT_DIR}/data/odb/sessionEntity-odb.cxx
)
# 添加需要包含的头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT_DIR}/data
    ${PROJECT_ROOT_DIR}/data/odb
    ${PROJECT_ROOT_DIR}/svc_userService
    ${GTEST_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${PROJECT_ROOT_DIR}/libSource/cpp-microservice-scaffold/source
)
# 添加需要链接的库
target_link_libraries(${PROJECT_NAME} PRIVATE
    odb-mysql
    odb
    mysqlclient
    bite_scaffold
    jsoncpp
    gtest
    redis++
    hiredis
    fmt
    pthread
)