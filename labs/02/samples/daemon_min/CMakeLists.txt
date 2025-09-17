
# Add source to this project's executable.
add_executable (daemon_min "daemon_min.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET daemon_min PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
