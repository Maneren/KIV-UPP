cmake_minimum_required(VERSION 3.30)

PROJECT(
  upp_03
  # LANGUAGES CXX
)

# set(CMAKE_CXX_COMPILER "g++")
# add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-lambda-capture)
#
# set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_CXX_EXTENSIONS OFF)
#
# set_property(GLOBAL PROPERTY USE_FOLDERS ON)

FILE(GLOB_RECURSE 01_mutex_src 01_mutex/*.cpp 01_mutex/*.h)
FILE(GLOB_RECURSE 02_deadlock_src 02_deadlock/*.cpp 02_deadlock/*.h)
FILE(GLOB_RECURSE 03_condvar_src 03_condvar/*.cpp 03_condvar/*.h)
FILE(GLOB_RECURSE 04_semaphore_src 04_semaphore/*.cpp 04_semaphore/*.h)

ADD_EXECUTABLE(01_mutex ${01_mutex_src})
ADD_EXECUTABLE(02_deadlock ${02_deadlock_src})
ADD_EXECUTABLE(03_condvar ${03_condvar_src})
ADD_EXECUTABLE(04_semaphore ${04_semaphore_src})
