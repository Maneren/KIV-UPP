CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

PROJECT(upp_mpi_bonus)

SET(CMAKE_CXX_STANDARD 20)

FILE(GLOB_RECURSE sources src/*.cpp src/*.c src/*.hpp src/*.h)

ADD_EXECUTABLE(upp_mpi_bonus ${sources})
