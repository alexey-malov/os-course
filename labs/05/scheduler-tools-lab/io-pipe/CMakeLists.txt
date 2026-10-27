cmake_minimum_required(VERSION 3.20)
project(io_pipe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(io-pipe src/main.cpp)
target_compile_options(io-pipe PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(io-pipe PRIVATE _POSIX_C_SOURCE=200809L)
