cmake_minimum_required(VERSION 3.24)
project(bmp_overlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_COMPILER "C:\Program Files\mingw-w64\x86_64-7.1.0-posix-seh-rt_v5-rev2\mingw64\bin\g++")
if (UNIX) 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse -msse2 -msse4.1")
endif()

# source
list(APPEND SOURCES "bmp_overlay.cpp")

add_executable(bmp_overlay ${SOURCES})

set_target_properties(bmp_overlay PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_LIST_DIR}/bin)
