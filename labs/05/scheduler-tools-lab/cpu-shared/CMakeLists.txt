cmake_minimum_required(VERSION 3.20)
project(cpu_shared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Threads REQUIRED)
add_executable(cpu-shared src/main.cpp)
target_link_libraries(cpu-shared PRIVATE Threads::Threads)
target_compile_options(cpu-shared PRIVATE -Wall -Wextra -Wpedantic)
