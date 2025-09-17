# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("samples")

# Include sub-projects.
add_subdirectory ("create_process_linux")
add_subdirectory ("create_process_windows")
add_subdirectory ("daemon_min")
add_subdirectory ("daemon")
add_subdirectory ("pthreads")
add_subdirectory ("win_threads")
add_subdirectory ("bad_errno")
add_subdirectory ("good_errno")
add_subdirectory ("thread_local_access_bench")
