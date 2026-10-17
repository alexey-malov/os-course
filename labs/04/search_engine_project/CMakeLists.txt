cmake_minimum_required(VERSION 3.16)

project(search_engine LANGUAGES CXX)

add_executable(search_engine search_engine_starter.cpp)

target_compile_features(search_engine PRIVATE cxx_std_20)
set_target_properties(search_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

# Понадобится при добавлении рабочих потоков в лабораторной работе.
find_package(Threads REQUIRED)
target_link_libraries(search_engine PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(search_engine PRIVATE /W4)
else()
    target_compile_options(search_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
