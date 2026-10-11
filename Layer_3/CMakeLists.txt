cmake_minimum_required(VERSION 3.15)
project(Layer3_ScadaHmi CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include ../common)

set(SOURCES
    src/main.cpp
    src/ScadaPoller.cpp
    src/HttpScadaServer.cpp
)

add_executable(scada_hmi ${SOURCES})

if(WIN32)
    target_link_libraries(scada_hmi PRIVATE ws2_32)
else()
    target_link_libraries(scada_hmi PRIVATE pthread)
endif()
