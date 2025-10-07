add_executable (thread_racing "thread_racing.cpp"  "lib.h" "lib.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET thread_racing PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(thread_racing PRIVATE /W4 /WX)
else()
  target_compile_options(thread_racing PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
