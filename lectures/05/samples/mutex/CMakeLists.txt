add_executable (mutex "mutex.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET mutex PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(mutex PRIVATE /W4 /WX)
else()
  target_compile_options(mutex PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
