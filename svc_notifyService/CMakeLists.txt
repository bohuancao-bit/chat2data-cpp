# cmake的最小版本
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(NotifyService)

# 设置C++标准为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型为Release
set(CMAKE_BUILD_TYPE Release)

# 设置proto生成的代码目录
set(PROTO_CODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../proto/protoCode)

# 添加包含目录
include_directories(${PROTO_CODE_DIR})

# 添加要编译的可执行文件
# 注意：proto生成的pb.cc和common静态库在父CMakeLists.txt中已处理
add_executable(${PROJECT_NAME}
    main.cc
    notifyServer.cc
    notifyServiceImpl.cc
    notifyBusiness.cc
    emailWorker.cc
    emailSender.cc
    verifyCodeEmailSender.cc
    normalEmailSender.cc
)

# 添加链接库
# 公共库在父CMakeLists.txt中定义，此处只添加特有的库
target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    proto
    ${COMMON_LIBRARIES}
    curl
)
