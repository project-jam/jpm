cmake_minimum_required(VERSION 3.16)
project(jpm VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g -DNDEBUG -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    else()
        message(STATUS "No CMAKE_BUILD_TYPE specified, or unknown type. Adding ASan flags with -g.")
        add_compile_options(-g -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
else()
    message(STATUS "Using default compiler flags for unknown compiler.")
endif()

# --- Dependencies ---
include(FetchContent)

# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# libcurl (system)
find_package(CURL REQUIRED)

# --- Target Definition ---
add_executable(jpm "") # Start with no sources, we'll add them below

# --- Source Files ---
set(JPM_MAIN_SOURCES
    src/main.cpp
)

set(JPM_COMMANDS_SOURCES
    src/commands/install.cpp
)
set(JPM_COMMANDS_HEADERS
    src/commands/install.h
)

set(JPM_NETWORK_SOURCES
    src/network/http_client.cpp
)
set(JPM_NETWORK_HEADERS
    src/network/http_client.h
)

set(JPM_PARSING_SOURCES
    src/parsing/json_parser.cpp
)
set(JPM_PARSING_HEADERS
    src/parsing/json_parser.h
)

set(JPM_PACKAGE_MANAGER_SOURCES
    src/package/dependency_resolver.cpp
    src/package/tarball_handler.cpp
)
set(JPM_PACKAGE_MANAGER_HEADERS
    src/package/package_spec.h
    src/package/package_info.h
    src/package/dependency_resolver.h
    src/package/tarball_handler.h
)

set(JPM_UTILS_SOURCES
    src/utils/file_utils.cpp
    src/utils/ui_utils.cpp
)
set(JPM_UTILS_HEADERS
    src/utils/file_utils.h
    src/jpm_config.h
    src/utils/ui_utils.h
)

target_sources(jpm PRIVATE
    ${JPM_MAIN_SOURCES}
    ${JPM_COMMANDS_SOURCES}
    ${JPM_NETWORK_SOURCES}
    ${JPM_PARSING_SOURCES}
    ${JPM_PACKAGE_MANAGER_SOURCES}
    ${JPM_UTILS_SOURCES}
)

# --- Include Directories ---
target_include_directories(jpm PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${nlohmann_json_SOURCE_DIR}/include
)

# --- Link Libraries ---
target_link_libraries(jpm PRIVATE
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# --- Install ---
install(TARGETS jpm DESTINATION bin)

# --- Messages ---
message(STATUS "JPM Project Configured")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
