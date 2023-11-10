project(
    arti-curl
    VERSION 0.0.1
    DESCRIPTION "cURL wrapper"
    LANGUAGES CXX
)

add_subdirectory(configured_files)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_library(${PROJECT_NAME} STATIC
    ${SOURCES}
)

add_library(arti::curl ALIAS ${PROJECT_NAME})

target_include_directories(
    ${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/configured_files/include
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
        fmt::fmt
        cppcoro::cppcoro
        CURL::libcurl
        spdlog::spdlog
)
