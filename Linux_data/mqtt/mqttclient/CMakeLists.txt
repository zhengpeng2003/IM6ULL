cmake_minimum_required(VERSION 3.10)

# 收集 mqttclient 目录下所有 .c
aux_source_directory(. DIR_SRCS)

# 直接生成 mqttclient 静态库
add_library(mqttclient STATIC ${DIR_SRCS})

# 🚀 一把梭：把 mqtt 下面所有可能用到的头文件目录全部加进来
target_include_directories(mqttclient
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}                # mqttclient.h
        ${CMAKE_CURRENT_SOURCE_DIR}/..             # mqtt/
        ${CMAKE_CURRENT_SOURCE_DIR}/../mqtt
        ${CMAKE_CURRENT_SOURCE_DIR}/../platform
        ${CMAKE_CURRENT_SOURCE_DIR}/../platform/linux
        ${CMAKE_CURRENT_SOURCE_DIR}/../network
        ${CMAKE_CURRENT_SOURCE_DIR}/../network/mbedtls/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/log
        ${CMAKE_CURRENT_SOURCE_DIR}/../config
)

# 依赖的库，直接连
target_link_libraries(mqttclient
    mqtt
    platform
    network
    common
)



