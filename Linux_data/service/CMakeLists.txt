add_library(service STATIC
    service_threads.cpp
)

target_include_directories(service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ipc
    ${CMAKE_SOURCE_DIR}/mqtt/wrapper
)
target_link_libraries(service ipc mqttwrapper
)
