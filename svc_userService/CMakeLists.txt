# CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(UserService)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型为Release
set(CMAKE_BUILD_TYPE Release)

# 设置proto代码目录
set(PROTO_CODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/protoCode)

# 设置data目录
set(DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../data)

# 添加包含目录
include_directories(
    ${PROTO_CODE_DIR}
    ${DATA_DIR}
)

# 添加可执行文件
# 注意：proto生成的pb.cc和common静态库在父CMakeLists.txt中已处理
add_executable(${PROJECT_NAME}
    main.cc
    userServer.cc
    userServerBuilder.cc
    userServiceImpl.cc
    userBusiness.cc
    sessionManager.cc
    ../data/userData.cc
    ../data/sessionData.cc
    ../data/verifyCodeData.cc
    ../data/odb/sessionEntity-odb.cxx
    ../data/odb/userEntity-odb.cxx
)

# 添加链接库
# 公共库在父CMakeLists.txt中定义，此处只添加特有的库
target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    proto
    ${COMMON_LIBRARIES}
    odb-mysql
    odb
    mysqlclient
    redis++
    hiredis
)
