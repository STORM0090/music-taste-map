cmake_minimum_required(VERSION 3.16)
project(music_taste_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(music_taste_lib STATIC
    src/csv_loader.cpp
    src/normalizer.cpp
    src/distance.cpp
)
target_include_directories(music_taste_lib PUBLIC include)

add_executable(music_taste_core src/main.cpp)
target_link_libraries(music_taste_core PRIVATE music_taste_lib)

enable_testing()
add_executable(music_taste_tests tests/test_core.cpp)
target_link_libraries(music_taste_tests PRIVATE music_taste_lib)
add_test(NAME core_tests COMMAND music_taste_tests)

foreach(target music_taste_lib music_taste_core music_taste_tests)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
