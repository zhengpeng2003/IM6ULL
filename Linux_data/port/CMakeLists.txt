add_library(port_manager OBJECT
    port_manager.cpp
)

target_include_directories(port_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/sensors
    ${CMAKE_SOURCE_DIR}/modbus
    ${CMAKE_SOURCE_DIR}/ipc
    ${CMAKE_SOURCE_DIR}/mqtt/wrapper
    ${CMAKE_SOURCE_DIR}/offline_cache
)
