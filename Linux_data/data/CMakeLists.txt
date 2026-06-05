add_library(data_protocol STATIC
    data_telemetry.c
    data_port.cpp
    data_command.c
)

target_include_directories(data_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/service
)
target_link_libraries(data_protocol
    service
    json-c
)
