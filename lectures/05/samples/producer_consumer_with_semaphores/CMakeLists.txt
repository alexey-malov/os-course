add_executable (producer_consumer_with_semaphores "producer_consumer_with_semaphores.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET producer_consumer_with_semaphores PROPERTY CXX_STANDARD 20)
endif()

if(MSVC)
  target_compile_options(producer_consumer_with_semaphores PRIVATE /W4 /WX)
else()
  target_compile_options(producer_consumer_with_semaphores PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
