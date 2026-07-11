cmake_minimum_required(VERSION 3.16)
project(music_taste_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(music_taste_core
    src/main.cpp
    src/csv_loader.cpp
    src/normalizer.cpp
)

target_include_directories(music_taste_core PRIVATE include)

if (MSVC)
    target_compile_options(music_taste_core PRIVATE /W4)
else()
    target_compile_options(music_taste_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
