cmake_minimum_required(VERSION 3.10)
project(qt_object C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-Wall -O2")

find_package(Threads REQUIRED)

add_subdirectory(device)
add_subdirectory(ipc)
add_subdirectory(modbus)
add_subdirectory(mqtt)
add_subdirectory(sensors)
add_subdirectory(service)
add_subdirectory(data)

add_executable(qt_object main.cpp)

target_link_libraries(qt_object
    mqttwrapper
    device
    ipc
    modbus
    sensors
    service
    data_protocol
    Threads::Threads
)
