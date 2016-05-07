cmake_minimum_required(VERSION 3.4)
project(zad1 C)

set(CMAKE_C_FLAGS "-Wall -pthread")

add_executable(main1 main.c)
add_executable(main2 main.c)
add_executable(main3 main.c)

set_target_properties(main1 PROPERTIES COMPILE_DEFINITIONS "VER1")
set_target_properties(main2 PROPERTIES COMPILE_DEFINITIONS "VER2")
set_target_properties(main3 PROPERTIES COMPILE_DEFINITIONS "VER3")
