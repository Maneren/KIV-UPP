CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

PROJECT(upp_08)

# MSVC kompilator
IF(MSVC)
  ADD_DEFINITIONS("/openmp")
ELSE() # gcc/clang
  ADD_DEFINITIONS("-fopenmp")
  ADD_LINK_OPTIONS("-fopenmp")
ENDIF()

FILE(GLOB_RECURSE 01_parallel_for_src 01_parallel_for/*.cpp 01_parallel_for/*.h)
FILE(GLOB_RECURSE 02_parallel_reduce_src 02_parallel_reduce/*.cpp 02_parallel_reduce/*.h)
FILE(GLOB_RECURSE 03_barriers_src 03_barriers/*.cpp 03_barriers/*.h)
FILE(GLOB_RECURSE 04_bonus_src 04_bonus/*.cpp 04_bonus/*.h)

ADD_EXECUTABLE(01_parallel_for ${01_parallel_for_src})
ADD_EXECUTABLE(02_parallel_reduce ${02_parallel_reduce_src})
ADD_EXECUTABLE(03_barriers ${03_barriers_src})
ADD_EXECUTABLE(04_bonus ${04_bonus_src})
