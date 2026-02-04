add_library(sensors STATIC
    sensor_th.cpp
   init_sensors.cpp
   relay.cpp
)

target_include_directories(sensors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(sensors modbus_master data_protocol ipc)

