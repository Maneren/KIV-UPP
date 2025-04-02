CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(upp_07)

SET(CMAKE_THREAD_PREFER_PTHREAD TRUE)
SET(THREADS_PREFER_PTHREAD_FLAG TRUE)
FIND_PACKAGE(Threads REQUIRED)
LINK_LIBRARIES(Threads::Threads)

FILE(GLOB_RECURSE 01_basics_src 01_basics/*.c 01_basics/*.h)
FILE(GLOB_RECURSE 02_condvar_src 02_condvar/*.c 02_condvar/*.h)
FILE(GLOB_RECURSE 03_ipc_semafor_src 03_ipc_semafor/*.c 03_ipc_semafor/*.h)
FILE(GLOB_RECURSE 05_thread_pool_src 05_thread_pool/*.c 05_thread_pool/*.h)

ADD_EXECUTABLE(01_basics ${01_basics_src})
ADD_EXECUTABLE(02_condvar ${02_condvar_src})
ADD_EXECUTABLE(03_ipc_semafor ${03_ipc_semafor_src})
ADD_EXECUTABLE(04_producent 04_shared_memory/producent.c)
ADD_EXECUTABLE(04_konzument 04_shared_memory/konzument.c)
ADD_EXECUTABLE(05_thread_pool ${05_thread_pool_src})

TARGET_LINK_LIBRARIES(03_ipc_semafor rt)
TARGET_LINK_LIBRARIES(04_producent rt)
TARGET_LINK_LIBRARIES(04_konzument rt)

