CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(upp_05_dcv)

SET(CMAKE_CXX_STANDARD 20)

FILE(GLOB_RECURSE 04_homework_src 04_homework/*.cpp 04_homework/*.h)

ADD_EXECUTABLE(04_homework ${04_homework_src})
