add_executable (peterson_lock_wrong "peterson_lock_wrong.cpp"  "lib.h" "lib.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET peterson_lock_wrong PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(peterson_lock_wrong PRIVATE /W4 /WX)
else()
  target_compile_options(peterson_lock_wrong PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
