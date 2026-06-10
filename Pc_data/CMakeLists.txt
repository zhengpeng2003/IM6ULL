cmake_minimum_required(VERSION 3.16)

project(Pc_data LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 你的 vcpkg 安装路径
set(VCPKG_ROOT "S:/cppsoft/vcpkg")
set(VCPKG_TARGET "x64-windows")

add_executable(Pc_data
    main.cpp

    ipc/IpcServer.cpp
    ipc/IpcServer.hpp

    model/DeviceData.hpp
    model/ModelConverter.cpp
    model/ModelConverter.hpp
    model/PointConfig.hpp
    model/PointConfigPackParser.cpp
    model/PointConfigPackParser.hpp
    model/SiteContext.hpp
    model/TelemetryPack.hpp
    model/TelemetryPackParser.cpp
    model/TelemetryPackParser.hpp
    model/TelemetryPoint.hpp

    service/PcDataService.cpp
    service/PcDataService.hpp

    storage/DatabaseSchema.cpp
    storage/DatabaseSchema.hpp
    storage/PcDatabase.cpp
    storage/PcDatabase.hpp

    third_party/sqlite/sqlite3.c
    third_party/sqlite/sqlite3.h
    mqtt/MqttClient.cpp mqtt/MqttClient.hpp
)

target_include_directories(Pc_data PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/mqtt
    ${CMAKE_CURRENT_SOURCE_DIR}/model
    ${CMAKE_CURRENT_SOURCE_DIR}/service
    ${CMAKE_CURRENT_SOURCE_DIR}/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite

    ${VCPKG_ROOT}/installed/${VCPKG_TARGET}/include
)

target_link_directories(Pc_data PRIVATE
    ${VCPKG_ROOT}/installed/${VCPKG_TARGET}/lib
)

target_link_libraries(Pc_data PRIVATE
    paho-mqtt3c # 同步客户端库
    paho-mqtt3a  #异步客户端库
)

include(GNUInstallDirs)

install(TARGETS Pc_data
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
