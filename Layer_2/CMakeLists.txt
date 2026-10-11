cmake_minimum_required(VERSION 3.15)
project(Layer2_PlcController CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include ../common)

set(SOURCES
    src/main.cpp
    src/PlcDataBank.cpp
    src/PlcSafetyInterlocks.cpp
    src/PlcScanEngine.cpp
    src/ModbusTcpServer.cpp
)

add_executable(plc_controller ${SOURCES})

if(WIN32)
    target_link_libraries(plc_controller PRIVATE ws2_32)
else()
    target_link_libraries(plc_controller PRIVATE pthread)
endif()
