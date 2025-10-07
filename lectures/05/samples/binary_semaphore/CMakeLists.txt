add_executable (binary_semaphore "binary_semaphore.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET binary_semaphore PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(binary_semaphore PRIVATE /W4 /WX)
else()
  target_compile_options(binary_semaphore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
