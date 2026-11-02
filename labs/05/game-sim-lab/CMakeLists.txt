cmake_minimum_required(VERSION 3.20)
project(game_sim_lab LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "game-sim uses Linux-specific scheduling APIs and requires Linux")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(game-sim
    src/main.cpp
    src/Workload.cpp
    src/Simulation.cpp
    src/Statistics.cpp
    src/ThreadScheduling.cpp
)
target_link_libraries(game-sim PRIVATE Threads::Threads)
target_compile_definitions(game-sim PRIVATE _GNU_SOURCE)
target_compile_options(game-sim PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME smoke_baseline
        COMMAND game-sim --duration 1 --asset-threads 2 --pool-size 1 --frame-work-ms 1 --ai-work-ms 1
                         --audio-work-ms 0.25 --asset-work-ms 1 --events-csv ${CMAKE_CURRENT_BINARY_DIR}/smoke-events.csv
                         --summary-csv ${CMAKE_CURRENT_BINARY_DIR}/smoke-summary.csv)
    set_tests_properties(smoke_baseline PROPERTIES TIMEOUT 15)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME validate_csv
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_output.py
                ${CMAKE_CURRENT_BINARY_DIR}/smoke-summary.csv
                ${CMAKE_CURRENT_BINARY_DIR}/smoke-events.csv)
        set_tests_properties(validate_csv PROPERTIES DEPENDS smoke_baseline)
    endif()
endif()
