add_library(ipc STATIC
ipc_server.c
)

target_include_directories(ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/device
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/mqtt/wrapper
)

target_link_libraries(ipc PUBLIC
    mqttwrapper
)
