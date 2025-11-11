
# Add source to this project's executable.
add_executable (windows_file_mapping "windows_file_mapping.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET windows_file_mapping PROPERTY CXX_STANDARD 20)
endif()
