add_executable (tsl_lock "tsl_lock.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tsl_lock PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(tsl_lock PRIVATE /W4 /WX)
else()
  target_compile_options(tsl_lock PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
