cmake_minimum_required(VERSION 3.31)
project(roboEyes)

set(CMAKE_CXX_STANDARD 20)

add_executable(shoubingTcpServer
        src/st7789_4num/pc_server/udpServer.cpp)
add_executable(forzaUdp
        src/forza_horizon5/ForzaUdp.cpp
        src/forza_horizon5/main.cpp)
add_executable(hasEyes
        src/version2/has18_kind_eye_all_in_one_esp32.cpp
        src/version2/MyDisplay.h)
