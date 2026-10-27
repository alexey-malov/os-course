cmake_minimum_required(VERSION 3.20)
project(scheduler_tools_lab LANGUAGES CXX)

# Все исполняемые файлы помещаются в каталог сборки bin/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(cpu-local)
add_subdirectory(cpu-shared)
add_subdirectory(io-pipe)
