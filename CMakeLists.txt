cmake_minimum_required(VERSION 3.24)
project(bmp_overlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

list(APPEND SOURCES "bmp_overlay.cpp")
list(APPEND SOURCES "bmp_overlay.h")

add_executable(bmp_overlay ${SOURCES})

set_target_properties(bmp_overlay PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)
