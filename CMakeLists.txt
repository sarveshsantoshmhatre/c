cmake_minimum_required(VERSION 3.16)
project(shattered_depths LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(shattered_depths
    src/main.cpp
    src/rng.c
)

target_include_directories(shattered_depths PRIVATE include)

if (UNIX)
    target_compile_options(shattered_depths PRIVATE -Wall -Wextra -Wpedantic)
endif()
