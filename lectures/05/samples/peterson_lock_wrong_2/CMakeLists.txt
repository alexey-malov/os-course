add_executable (peterson_lock_wrong_2 "peterson_lock_wrong_2.cpp"  "lib.h" "lib.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET peterson_lock_wrong_2 PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(peterson_lock_wrong_2 PRIVATE /W4 /WX)
else()
  target_compile_options(peterson_lock_wrong_2 PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
