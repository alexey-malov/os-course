
# Add source to this project's executable.
add_executable (hello_fuse "hello_fuse.cpp" )

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET hello_fuse PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
