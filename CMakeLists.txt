cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_AUTHOR "erick saúl")
set(PROJECT_AUTHOR_GITHUB "@erick3900")

project(arti-spotify-tui)

include(dependencies/dependencies.cmake)

add_subdirectory(src/lib)
add_subdirectory(src/app)
