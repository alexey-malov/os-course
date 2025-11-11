
# Add source to this project's executable.
add_executable (linux_file_mapping "linux_file_mapping.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET linux_file_mapping PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
