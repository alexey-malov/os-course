find_package(Boost REQUIRED COMPONENTS context)

add_executable(
    context_switch_in_user_space
    "context_switch_in_user_space.cpp"
)

target_compile_features(
    context_switch_in_user_space
    PRIVATE cxx_std_20
)

target_link_libraries(
    context_switch_in_user_space
    PRIVATE Boost::context
)