cmake_minimum_required(VERSION 3.30)

project(
  KIV_UPP_Sem01
  VERSION 1.0
  DESCRIPTION "Semester project #1 for KIV/UPP"
  LANGUAGES CXX)

add_compile_options(-Wall -Wextra -Wpedantic)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

add_compile_definitions(PERF_TEST_MACRO)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_executable(
  meteo
  src/main.cpp
  src/config.cpp
  src/outliers.cpp
  src/preprocessor.cpp
  src/renderer.cpp
  src/threadpool.cpp
)
