add_library(data_protocol STATIC
    alarm_config.c
    data_telemetry.c
    data_command.c
    data_ack.c
    data_publish.c
    $<TARGET_OBJECTS:port_manager>
)

target_include_directories(data_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/service
    ${CMAKE_SOURCE_DIR}/port
    ${CMAKE_SOURCE_DIR}/ipc
    ${CMAKE_SOURCE_DIR}/mqtt/wrapper
    ${CMAKE_SOURCE_DIR}/offline_cache
)
target_link_libraries(data_protocol
    json-c
    modbus_master
    sensors
    ipc
    mqttwrapper
    offline_cache
)
