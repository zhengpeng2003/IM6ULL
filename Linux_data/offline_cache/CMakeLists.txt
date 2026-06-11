add_library(offline_cache STATIC
    OfflineCacheDatabase.cpp
    OfflineCachePolicy.cpp
    OfflinePublishQueue.cpp
    OfflinePublishQueueC.cpp
)

target_include_directories(offline_cache PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(offline_cache PUBLIC
    sqlite3
    Threads::Threads
)
