# CPM Download
file(
    DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.6/CPM.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
)

include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage("gh:fmtlib/fmt#10.1.1")
CPMAddPackage("gh:nlohmann/json@3.11.2")
CPMAddPackage("gh:ArthurSonzogni/FTXUI@5.0.0")
CPMAddPackage(
    GITHUB_REPOSITORY "gabime/spdlog"
    VERSION "1.12.0"
    OPTIONS "SPDLOG_FMT_EXTERNAL ON"
)

find_package(CURL REQUIRED)

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/chafa" ${CMAKE_PREFIX_PATH})
include(FindPkgConfig REQUIRED)
pkg_check_modules(chafa REQUIRED IMPORTED_TARGET chafa)

find_package(
    cppcoro CONFIG REQUIRED
        PATHS dependencies/cppcoro/lib/cmake/cppcoro
)

add_subdirectory(dependencies/mongoose)
