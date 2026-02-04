cmake_minimum_required(VERSION 3.10)
project(qt_object C CXX)
set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "-Wall -O2 -lpthread")

add_subdirectory(device)
add_subdirectory(ipc)
add_subdirectory(modbus)
add_subdirectory(mqtt)
add_subdirectory(sensors)
add_subdirectory(service)
add_subdirectory(data)

add_executable(qt_object main.cpp)

# ✅ 正确的链接顺序（从上层到底层）
set(MQTT_LIBS 
    mqttwrapper    # 你的封装层
    mqttclient     # 客户端 API
    mqtt           # 核心协议
    network        # 网络层
    mbedtls        # 加密库
    wrapper        # mbedtls 包装
    common         # 公共代码（random_number 在这里！）
    salof          # 日志系统
    platform       # 平台抽象
)

foreach(lib ${MQTT_LIBS})
    target_link_libraries(qt_object ${lib})
endforeach()

target_link_libraries(qt_object
    device
    ipc
    modbus
    sensors
    service
    pthread
    data_protocol
    mqttwrapper
)


