cmake_minimum_required(VERSION 3.31)
project(roboEyes)

set(CMAKE_CXX_STANDARD 20)

add_executable(roboEyes
        src/forza_horizon5/ForzaUdp.cpp
        src/forza_horizon5/ForzaUdp.h
        src/forza_horizon5/main.cpp
        src/st7789_4num/shoubing.cpp
        src/st7789_4num/main.cpp)
