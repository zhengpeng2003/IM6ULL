cmake_minimum_required(VERSION 2.8)

set(PROJECT_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/mqtt/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/mqtt/bin")

set(TARGETS "mqtt-client")

set(SUBDIRS "mqtt" "platform" "network" "common" "mqttclient" "wrapper")

set(INCDIRS 
    "mqtt" "platform/linux" "common" "common/log" 
    "network" "network/mbedtls/include" "mqttclient" "config" "wrapper"
)
set(OUTDIRS "build")

# ✅ 正确的链接顺序：从顶层到底层，被依赖的放后面
set(LIBNAMES 
    "mqttwrapper"    # 你的封装层（最顶层）
    "mqttclient"     # 客户端API
    "mqtt"           # 核心协议
    "network"        # 网络层（依赖 mbedtls, wrapper, common）
    "mbedtls"        # 加密库
    "wrapper"        # mbedtls包装
    "common"         # 公共代码（random_number在这里！）
    "salof"          # 日志系统
    "platform"       # 平台抽象（最底层）
)

# 头文件
foreach(incdir ${INCDIRS})
    include_directories(${PROJECT_ROOT_PATH}/${incdir})
endforeach()

# 子目录
foreach(subdir ${SUBDIRS})
    add_subdirectory(${PROJECT_ROOT_PATH}/${subdir})
endforeach()

link_directories(${LIBRARY_OUTPUT_PATH})
