add_executable (fair_rwlock 
	"fair_rwlock.cpp"
	"../libs/catch2/catch_amalgamated.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET fair_rwlock PROPERTY CXX_STANDARD 20)
endif()


if(MSVC)
  target_compile_options(fair_rwlock PRIVATE /W4 /WX)
else()
  target_compile_options(fair_rwlock PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(fair_rwlock
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/catch2
)
