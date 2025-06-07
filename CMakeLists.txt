cmake_minimum_required(VERSION 3.16)
project(jpm VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    else() # Default to Debug-like ASan flags if no build type is specified
        message(STATUS "No CMAKE_BUILD_TYPE specified, or unknown type. Adding ASan flags with -g.")
        add_compile_options(-g -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
elseif(MSVC)
    # MSVC-specific flags
    add_compile_options(/W4) # Warning Level 4
    # No sanitizers typically available like ASan directly with MSVC
    # Debug/Release flags are often handled by CMake's default build types with MSVC
else()
    message(STATUS "Using default compiler flags for unknown compiler.")
endif()

# --- Include FetchContent for managing dependencies ---
include(FetchContent)

# --- nlohmann/json ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3 # Or a more recent tag
)
FetchContent_MakeAvailable(nlohmann_json)

# --- cpr (C++ Requests) ---
# CPR can be used header-only for basic GET if you don't need complex features
# or can be built fully. For simplicity, let's assume header-only usage initially.
# If full build is needed:
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.10.5 # Or a more recent tag
)
FetchContent_MakeAvailable(cpr)
# Note: cpr might require libcurl. On Arch/CachyOS: sudo pacman -S curl
# Ensure CMake finds libcurl: find_package(CURL REQUIRED)
# Then link cpr::cpr to your target.

# --- Target Definition ---
add_executable(jpm "") # Start with no sources, we'll add them

# --- Source Files ---
# Group sources by their purpose for better organization
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
    # src/package/package_spec.cpp # If it has methods
    # src/package/package_info.cpp # If it has methods
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src" # Allow #include "commands/install_command.h"
    ${nlohmann_json_SOURCE_DIR}/include # For nlohmann/json.hpp
    # For CPR, if using its built form, it would be through cpr::cpr target.
    # If header-only, you might need to add its include dir if not automatically handled.
    # ${cpr_SOURCE_DIR}/include # If needed and not handled by target_link_libraries
)


# --- Link Libraries ---
target_link_libraries(jpm PRIVATE
    nlohmann_json::nlohmann_json # Alias target for nlohmann_json
    cpr::cpr                   # Link against the cpr library target
)
# If cpr requires manual linking of curl:
# find_package(CURL REQUIRED)
# target_link_libraries(jpm PRIVATE ${CURL_LIBRARIES})


# --- JavaScriptCore (Placeholder - As before) ---
# Use pkg-config on non-Windows systems for javascriptcoregtk
if(NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(JavaScriptCore QUIET javascriptcoregtk-4.0) # Or 4.1, or other version
        if(JavaScriptCore_FOUND)
            target_include_directories(jpm PRIVATE ${JavaScriptCore_INCLUDE_DIRS})
            target_link_libraries(jpm PRIVATE ${JavaScriptCore_LIBRARIES})
            message(STATUS "Found JavaScriptCore: ${JavaScriptCore_LIBRARIES}")
        else()
            message(WARNING "JavaScriptCore (GTK) not found via pkg-config.")
        endif()
    else()
        message(WARNING "pkg-config not found, cannot auto-detect JavaScriptCore.")
    endif()
else()
    message(STATUS "Skipping JavaScriptCore (GTK) check on Windows.")
    # On Windows, you would need a different method to find/include/link JavaScriptCore
    # This would likely involve adding include directories and library paths manually
    # or through a package manager like vcpkg, or building from source.
    # find_package(JavaScriptCore QUIET) # Example if find module existed
    # if(JavaScriptCore_FOUND)
    #    target_include_directories(jpm PRIVATE ${JavaScriptCore_INCLUDE_DIRS})
    #    target_link_libraries(jpm PRIVATE ${JavaScriptCore_LIBRARIES})
    #    message(STATUS "Found JavaScriptCore (Windows method).")
    # endif()
endif()


install(TARGETS jpm DESTINATION bin)

message(STATUS "JPM Project Configured")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
