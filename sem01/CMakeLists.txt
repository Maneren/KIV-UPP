cmake_minimum_required(VERSION 3.30)

project(
  KIV_UPP_Sem01
  VERSION 1.0
  DESCRIPTION "Semester project #1 for KIV/UPP"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_executable(
  meteo
  src/main.cpp
  src/config.cpp
  src/preprocessor.cpp
  src/threadpool.cpp
  src/utils.cpp
)
