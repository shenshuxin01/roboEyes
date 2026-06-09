cmake_minimum_required(VERSION 3.31)
project(roboEyes)

set(CMAKE_CXX_STANDARD 20)

add_executable(roboEyes main.cpp
        src/forza_horizon5/ForzaUdp.cpp
        src/forza_horizon5/ForzaUdp.h
        src/forza_horizon5/main.cpp
        src/forza_horizon5/main.h
        src/display_4_number/display_4_number.cpp
        src/display_4_number/display_4_number.h
        src/display_4_number/display_4_number.cpp
        src/st7789/st7789.cpp
        src/st7789/st7789.h
        src/st7789/st7789_esp.cpp
        src/st7789/st7789_esp.h)
