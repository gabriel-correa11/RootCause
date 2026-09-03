cmake_minimum_required(VERSION 3.10)
project(rootcauze)

set(CMAKE_CXX_STANDARD 20)

add_executable(rootcauze src/main.cpp)

target_link_libraries(rootcauze PRIVATE ws2_32 psapi ${PD_LIBRARIES})
