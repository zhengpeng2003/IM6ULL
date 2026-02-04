cmake_minimum_required(VERSION 2.8)

aux_source_directory(. WRAPPER_SRCS)

# 生成 wrapper 库
add_library(mqttwrapper STATIC ${WRAPPER_SRCS})

target_include_directories(mqttwrapper 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}  # 当前目录（包含 mqtt_wrapper.h）
)
# 依赖底层的 mqttclient
target_link_libraries(mqttwrapper mqttclient)
