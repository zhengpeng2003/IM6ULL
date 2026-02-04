add_library(ipc STATIC
ipc_server.c
)

target_include_directories(ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
