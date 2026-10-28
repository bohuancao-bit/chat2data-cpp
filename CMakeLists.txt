# CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(Chat2Data)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型为Release，开启优化
set(CMAKE_BUILD_TYPE Release)

# 设置输出目录
# 可执行文件输出到 bin/ 目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
# 静态库输出到 bin/lib/ 目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/lib)

# 创建输出目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/lib)

# 设置临时文件目录（build目录）
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)

# 设置proto生成的C++代码目录
set(PROTO_CODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto/protoCode)
file(MAKE_DIRECTORY ${PROTO_CODE_DIR})

# 设置odb生成的代码和SQL目录
set(ODB_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/odb)
set(SQL_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/sql)
file(MAKE_DIRECTORY ${ODB_OUTPUT_DIR})
file(MAKE_DIRECTORY ${SQL_OUTPUT_DIR})

# 查找protobuf编译器
find_package(Protobuf REQUIRED)

# 查找OpenSSL库
find_package(OpenSSL REQUIRED)

# 编译common静态库
# common目录包含公共代码：errorHandler、sqlValidator、utils
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/common/errorHandler.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/common/sqlValidator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/common/utils.cc
)
add_library(common STATIC ${COMMON_SOURCES})
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

# 编译proto静态库
# 遍历proto目录下所有的.proto文件并生成C++代码
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")

set(PROTO_SOURCES "")
set(PROTO_HEADERS "")
set(PROTO_TARGETS "")

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    set(PROTO_SRC "${PROTO_CODE_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${PROTO_CODE_DIR}/${PROTO_NAME}.pb.h")
    
    list(APPEND PROTO_SOURCES ${PROTO_SRC})
    list(APPEND PROTO_HEADERS ${PROTO_HDR})
    list(APPEND PROTO_TARGETS "generate_${PROTO_NAME}_proto")
    
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
                --cpp_out=${PROTO_CODE_DIR}
                ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ code from ${PROTO_NAME}.proto"
        VERBATIM
    )
    
    add_custom_target(generate_${PROTO_NAME}_proto ALL
        DEPENDS ${PROTO_SRC} ${PROTO_HDR}
    )
endforeach()

# 创建proto静态库
add_library(proto STATIC ${PROTO_SOURCES})
target_include_directories(proto PUBLIC ${PROTO_CODE_DIR})

# 让proto库依赖于所有proto生成目标
foreach(TARGET ${PROTO_TARGETS})
    add_dependencies(proto ${TARGET})
endforeach()

# 定义所有子服务共同依赖的库
# 这些库在子服务的CMakeLists.txt中统一链接
# 注意：brpc必须在leveldb之前链接，OpenSSL必须在brpc之后再次链接
set(COMMON_LIBRARIES
    bite_scaffold
    protobuf
    jsoncpp
    redis++
    hiredis
    spdlog
    gflags
    fmt
    etcd-cpp-api
    cpprest
    pthread
    dl
)

# 查找brpc库并设置链接顺序
find_library(BRPC_LIB NAMES brpc)
if(BRPC_LIB)
    # brpc必须在leveldb之前链接，这样leveldb的符号可以被brpc解析
    list(APPEND COMMON_LIBRARIES ${BRPC_LIB} leveldb)
    # brpc依赖于OpenSSL，需要在brpc之后再链接一次OpenSSL和crypt
    list(APPEND COMMON_LIBRARIES OpenSSL::SSL OpenSSL::Crypto crypt)
endif()

# 定义公共头文件目录
set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${PROTO_CODE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/data
)

# 将公共目录添加到全局include路径
include_directories(${COMMON_INCLUDE_DIRS})

# 添加各子服务
add_subdirectory(svc_userService)
add_subdirectory(svc_aiService)
add_subdirectory(svc_dbService)
add_subdirectory(svc_fileService)
add_subdirectory(svc_gatewayService)
add_subdirectory(svc_excelParserService)
add_subdirectory(svc_notifyService)
