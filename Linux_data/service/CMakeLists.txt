add_library(service STATIC
    service.cpp
    service_threads.cpp
)

target_include_directories(service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(service device data_protocol ipc modbus_master 
)


