# CMake最小版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(userServiceProto)

# 设置C++标准为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找protoc编译器
find_package(Protobuf REQUIRED)

# 创建输出目录用于存放生成的C++代码文件
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protoCode)
file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

# 自动获取当前目录下所有的.proto文件
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

# 存储所有生成的源文件和头文件
set(PROTO_SOURCES "")
set(PROTO_HEADERS "")
set(PROTO_TARGETS "")

# 遍历所有proto文件并生成代码
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    set(PROTO_SRC "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.h")

    list(APPEND PROTO_SOURCES ${PROTO_SRC})
    list(APPEND PROTO_HEADERS ${PROTO_HDR})
    list(APPEND PROTO_TARGETS "generate_${PROTO_NAME}_proto")

    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                -I ${CMAKE_CURRENT_SOURCE_DIR}
                --cpp_out=${PROTO_OUTPUT_DIR}
                ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ code from ${PROTO_NAME}.proto"
        VERBATIM
    )

    add_custom_target(generate_${PROTO_NAME}_proto ALL
        DEPENDS ${PROTO_SRC} ${PROTO_HDR}
    )
endforeach()

# 添加protoCode目录到include路径
include_directories(${PROTO_OUTPUT_DIR})