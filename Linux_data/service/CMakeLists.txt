add_library(service STATIC
    service.cpp
    service_threads.cpp
    port_manager.cpp
)

target_include_directories(service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/data
)
target_link_libraries(service device data_protocol ipc modbus_master mqttwrapper
)
