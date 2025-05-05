CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

# NOTE: toto neni uplne nejlepsi praktika, lepsi je nastavit bud promennou prostredi CC a CXX
# nebo do CMake prikazu tyto konstanty predat
SET(CMAKE_C_COMPILER "mpicc")
SET(CMAKE_CXX_COMPILER "mpicxx")

PROJECT(upp_09)

FILE(GLOB_RECURSE 01_matrix_mul_src 01_matrix_mul/*.cpp 01_matrix_mul/*.h)
FILE(GLOB_RECURSE 02_reduce_src 02_reduce/*.cpp 02_reduce/*.h)
FILE(GLOB_RECURSE 03_image_src 03_image/*.cpp 03_image/*.h)

ADD_EXECUTABLE(01_matrix_mul ${01_matrix_mul_src})
ADD_EXECUTABLE(02_reduce ${02_reduce_src})
ADD_EXECUTABLE(03_image ${03_image_src})

