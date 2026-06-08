cmake_minimum_required(VERSION 3.16)
project(Pc_mqtt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Sql Charts Mqtt Network)

file(GLOB_RECURSE PROJECT_SOURCES
    "*.cpp"
    "*.h"
    "resources/*.qrc"
)

qt_add_executable(Pc_mqtt
    ${PROJECT_SOURCES}
)

target_link_libraries(Pc_mqtt PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Charts
    Qt6::Mqtt
    Qt6::Network
)
