add_library(data_protocol STATIC
    data_packer.c
)

target_include_directories(data_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(data_protocol
    service
	  json-c
)

