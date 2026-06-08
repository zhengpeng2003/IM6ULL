cmake_minimum_required(VERSION 3.16)

project(Pc_data LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Pc_data
    main.cpp

    ipc/IpcServer.cpp
    ipc/IpcServer.hpp
    model/DeviceData.hpp model/ModelConverter.cpp model/ModelConverter.hpp model/SiteContext.hpp model/TelemetryPack.hpp model/TelemetryPoint.hpp
    service/PcDataService.cpp service/PcDataService.hpp
)

target_include_directories(Pc_data PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
)

include(GNUInstallDirs)

install(TARGETS Pc_data
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)