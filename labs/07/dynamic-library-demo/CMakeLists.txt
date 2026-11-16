cmake_minimum_required(VERSION 3.20)

project(DynamicLibraryDemo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# $<CONFIG> размещает Debug/Release в отдельных каталогах для всех генераторов.
# Получаем: bin/<config>/demo-host и bin/<config>/plugins/<library>.
set(DEMO_BIN_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Демонстрационная динамическая библиотека.
add_library(string_plugin SHARED
    library/StringLibrary.cpp
    common/StringApi.h
)
target_include_directories(string_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/common")
target_compile_definitions(string_plugin PRIVATE STRING_LIBRARY_BUILD)
set_target_properties(string_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${DEMO_BIN_DIR}/plugins"   # Linux: .so
    RUNTIME_OUTPUT_DIRECTORY "${DEMO_BIN_DIR}/plugins"   # Windows: .dll
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>" # Windows: .lib
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
enable_project_warnings(string_plugin)

# Приложение загружает библиотеку только во время выполнения.
add_executable(demo-host
    host/main.cpp
    host/DynamicLibrary.cpp
    host/DynamicLibrary.h
    host/LibraryString.cpp
    host/LibraryString.h
    common/StringApi.h
)
target_include_directories(demo-host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/host"
)
target_link_libraries(demo-host PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(demo-host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DEMO_BIN_DIR}"
)
enable_project_warnings(demo-host)

# Важно: зависимости по сборке есть, но линковки demo-host с string_plugin НЕТ.
add_dependencies(demo-host string_plugin)

enable_testing()
add_test(NAME loads_dynamic_library COMMAND $<TARGET_FILE:demo-host>)
set_tests_properties(loads_dynamic_library PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    PASS_REGULAR_EXPRESSION "Hello from the dynamic library demo!"
)
