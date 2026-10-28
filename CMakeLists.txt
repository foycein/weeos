cmake_minimum_required(VERSION 3.20)
project(WeeOS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/W4 /permissive- /utf-8 /EHsc)
include_directories(source tests)

add_subdirectory(source/Formats)
add_subdirectory(source/Ios)
add_subdirectory(tools/TextureDump)

enable_testing()
add_subdirectory(tests)
