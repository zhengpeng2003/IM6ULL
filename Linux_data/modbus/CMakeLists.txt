add_library(modbus_master STATIC
    modbus_master.cpp
    )

target_include_directories(modbus_master PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(modbus_master
    modbus
)
