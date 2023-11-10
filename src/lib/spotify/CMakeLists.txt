project(
    arti-spotify-lib
    VERSION 0.0.1
    DESCRIPTION "Spotify utility library"
    LANGUAGES CXX
)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_library(${PROJECT_NAME} STATIC
    ${SOURCES}
)

add_library(arti::spotify ALIAS ${PROJECT_NAME})

target_include_directories(
    ${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/configured_files/include
)

target_link_libraries(
    ${PROJECT_NAME} PUBLIC
        fmt::fmt
        cppcoro::cppcoro
        nlohmann_json::nlohmann_json

        spdlog::spdlog

        mongoose

        arti::curl
)
