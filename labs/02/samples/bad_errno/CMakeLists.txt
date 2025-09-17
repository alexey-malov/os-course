
# Add source to this project's executable.
add_executable (bad_errno "bad_errno.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET bad_errno PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
