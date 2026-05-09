#1.添加cmake版本说明
cmake_minimum_required(VERSION 3.1.0)

#2.声明工程名称
project(file_server)

#3.检测并生成proto框架代码

##1.添加所需要的proto映射代码文件名称
set(proto_path ${CMAKE_CURRENT_SOURCE_DIR}/../proto)
set(proto_files file.proto base.proto)
set(test_client "file_client")
##2.检测框架代码是否生成
set(proto_srcs "")  # 初始化变量

foreach(proto_file ${proto_files})
    # 获取纯文件名（不含路径）
    string(REPLACE ".proto" ".pb.cc" proto_cc ${proto_file})
    string(REPLACE ".proto" ".pb.h" proto_h ${proto_file})
    
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${proto_cc})
        add_custom_command(
            PRE_BUILD
            COMMAND protoc
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I ${proto_path} ${proto_path}/${proto_file}
            DEPENDS ${proto_path}/${proto_file}
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${proto_cc}
            COMMENT "Generating Protobuf framework code: ${CMAKE_CURRENT_BINARY_DIR}/${proto_cc}"
        )
    endif()
    list(APPEND proto_srcs ${CMAKE_CURRENT_BINARY_DIR}/${proto_cc})
endforeach()

#4.获取源码目录下的源文件
set(src_files "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/source src_files)

#5.声明目标及依赖
add_executable(file_server ${src_files} ${proto_srcs})
#7.设置链接库
target_link_libraries(file_server 
    gflags 
    fmt 
    brpc 
    protobuf 
    leveldb 
    spdlog
    curl
    ssl 
    crypto 
    pthread
    cpprest
    etcd-cpp-api
    gtest
    /usr/lib/x86_64-linux-gnu/libjsoncpp.so.19
)

set(test_files "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/test test_files)
add_executable(${test_client} ${test_files} ${proto_srcs})

target_link_libraries(${test_client}
    gflags 
    fmt 
    brpc 
    protobuf 
    leveldb 
    spdlog
    curl
    ssl 
    crypto 
    pthread
    cpprest
    etcd-cpp-api
    gtest
    /usr/lib/x86_64-linux-gnu/libjsoncpp.so.19
)
#6.设置头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../third/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})



#8.安装
INSTALL(TARGETS file_server RUNTIME DESTINATION bin)