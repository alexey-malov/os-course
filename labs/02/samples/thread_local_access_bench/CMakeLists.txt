
# Add source to this project's executable.
add_executable (thread_local_access_bench 
    "thread_local_access_bench.cpp" 
    "../libs/catch2/catch_amalgamated.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET thread_local_access_bench PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(thread_local_access_bench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/catch2
)
# TODO: Add tests and install targets if needed.
