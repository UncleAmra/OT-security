cmake_minimum_required(VERSION 3.15)
project(Layer1_PhysicsEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include ../common)

set(SOURCES
    src/main.cpp
    src/BoilerSystem.cpp
    src/CombustionSystem.cpp
    src/TurbineSystem.cpp
    src/GeneratorSystem.cpp
    src/CondenserFeedwaterSystem.cpp
    src/DigitalTwinPlant.cpp
)

add_executable(physics_engine ${SOURCES})

if(WIN32)
    target_link_libraries(physics_engine PRIVATE ws2_32)
else()
    target_link_libraries(physics_engine PRIVATE pthread)
endif()
