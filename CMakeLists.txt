cmake_minimum_required(VERSION 3.31)
project(roboEyes)

set(CMAKE_CXX_STANDARD 20)

add_executable(roboEyes
        src/st7789_4num/pc_server/udpServer.cpp)
