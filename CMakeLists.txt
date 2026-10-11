cmake_minimum_required(VERSION 3.15)
project(OT_PowerPlant_Testbed CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(common)

add_subdirectory(Layer_1)
add_subdirectory(Layer_2)
add_subdirectory(Layer_3)
