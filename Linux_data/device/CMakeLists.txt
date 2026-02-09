add_library(device STATIC
    device_info.cpp
   
)

target_include_directories(device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(device data_protocol ipc
)

