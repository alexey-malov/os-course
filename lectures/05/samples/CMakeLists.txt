cmake_minimum_required (VERSION 3.30)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("samples")

# Include sub-projects.
add_subdirectory ("thread_racing")
add_subdirectory ("atomic_thread_racing")
add_subdirectory ("tsl_lock")
add_subdirectory ("peterson_lock")
add_subdirectory ("peterson_lock_wrong")
add_subdirectory ("peterson_lock_wrong_2")
add_subdirectory ("producer_consumer_with_semaphores")
add_subdirectory ("binary_semaphore")
add_subdirectory ("mutex")
add_subdirectory ("shared_mutex")
add_subdirectory ("fair_rwlock")
