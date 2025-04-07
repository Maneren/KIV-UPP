CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(upp_04)

SET(CMAKE_CXX_STANDARD 20)

FILE(GLOB_RECURSE 01_prodcons_src 01_prodcons/*.cpp 01_prodcons/*.h)
FILE(GLOB_RECURSE 02_barrier_src 02_barrier/*.cpp 02_barrier/*.h)
FILE(GLOB_RECURSE 03_atomic_src 03_atomic/*.cpp 03_atomic/*.h)
FILE(GLOB_RECURSE 04_ctenar_pisar_src 04_ctenar_pisar/*.cpp 04_ctenar_pisar/*.h)

ADD_EXECUTABLE(01_prodcons ${01_prodcons_src})
ADD_EXECUTABLE(02_barrier ${02_barrier_src})
ADD_EXECUTABLE(03_atomic ${03_atomic_src})
ADD_EXECUTABLE(04_ctenar_pisar ${04_ctenar_pisar_src})
