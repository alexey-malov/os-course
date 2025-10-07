add_executable (shared_mutex "shared_mutex.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET shared_mutex PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(shared_mutex PRIVATE /W4 /WX)
else()
  target_compile_options(shared_mutex PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
