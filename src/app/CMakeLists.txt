cmake_minimum_required(VERSION 3.20)

project(
    arti-spotify
    VERSION 0.0.1
    DESCRIPTION "Spotify TUI by an Artichoke"
    LANGUAGES CXX
)

add_subdirectory(configured_files)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(
    ${PROJECT_NAME} 
        main.cpp
        ${SOURCES}
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE 
        fmt::fmt
        cppcoro::cppcoro
        nlohmann_json::nlohmann_json

        ftxui::dom
        ftxui::screen
        ftxui::component

        spdlog::spdlog

        PkgConfig::chafa

        arti::curl
        arti::spotify
)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/configured_files/include
)
