cmake_minimum_required(VERSION 3.10)

project(http-client C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(http-client src/main.c)
