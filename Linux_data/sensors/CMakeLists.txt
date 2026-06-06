add_library(sensors STATIC
    sensor_th.cpp
    relay.cpp
)

target_include_directories(sensors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/modbus
)
target_link_libraries(sensors modbus_master data_protocol)
