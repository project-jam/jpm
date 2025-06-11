cmake_minimum_required(VERSION 3.16)
project(jpm VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g -DNDEBUG -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    else()
        message(STATUS "No CMAKE_BUILD_TYPE specified or unknown; adding ASan flags with -g by default.")
        add_compile_options(-g -fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
else()
    message(STATUS "Using default compiler flags for unknown compiler.")
endif()

# JavaScriptCore embedding
set(USE_JAVASCRIPTCORE ON)

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
add_executable(jpm "")  # sources added below

# --- Source Files ---
set(JPM_MAIN_SOURCES
    src/main.cpp
)

set(JPM_COMMANDS_SOURCES
    src/commands/install.cpp
    # js_command.cpp will be added conditionally below
)

# Include js_command.cpp only if JSCore embedding is enabled
if(USE_JAVASCRIPTCORE)
    list(APPEND JPM_COMMANDS_SOURCES
         src/commands/js_command.cpp
    )
    # Define macro so code sees USE_JAVASCRIPTCORE
    add_definitions(-DUSE_JAVASCRIPTCORE)
endif()

set(JPM_NETWORK_SOURCES
    src/network/http_client.cpp
)
set(JPM_PARSING_SOURCES
    src/parsing/json_parser.cpp
)
set(JPM_PACKAGE_MANAGER_SOURCES
    src/package/dependency_resolver.cpp
    src/package/tarball_handler.cpp
)
set(JPM_UTILS_SOURCES
    src/utils/file_utils.cpp
    src/utils/ui_utils.cpp
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

# --- Link Libraries (base) ---
target_link_libraries(jpm PRIVATE
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# --- JavaScriptCore embedding if requested ---
if(USE_JAVASCRIPTCORE)
    message(STATUS "JavaScriptCore embedding enabled.")
    if(APPLE)
        # macOS: find the JavaScriptCore.framework
        find_library(JAVASCRIPTCORE_FRAMEWORK JavaScriptCore
            PATHS /System/Library/Frameworks /Library/Frameworks NO_DEFAULT_PATH)
        if(NOT JAVASCRIPTCORE_FRAMEWORK)
            message(FATAL_ERROR "JavaScriptCore.framework not found on this macOS system.")
        endif()
        # Link the framework
        target_link_libraries(jpm PRIVATE "${JAVASCRIPTCORE_FRAMEWORK}")
        # Include headers from the framework
        get_filename_component(JSC_FRAMEWORK_DIR "${JAVASCRIPTCORE_FRAMEWORK}" DIRECTORY)
        set(JSC_HEADERS_DIR "${JSC_FRAMEWORK_DIR}/Headers")
        target_include_directories(jpm PRIVATE "${JSC_HEADERS_DIR}")
        message(STATUS "Linked JavaScriptCore.framework at ${JAVASCRIPTCORE_FRAMEWORK}")
    elseif(UNIX)
        # Linux (or other UNIX): attempt to find via pkg-config
        find_package(PkgConfig REQUIRED)
        # Adjust the module name if your distro uses a different pkg-config name
        pkg_check_modules(JSC javascriptcoregtk-4.0)
        if(NOT JSC_FOUND)
            message(FATAL_ERROR "Could not find JavaScriptCore via pkg-config (tried 'javascriptcoregtk-4.0'). "
                               "Install the appropriate dev package or adjust the module name.")
        endif()
        target_include_directories(jpm PRIVATE ${JSC_INCLUDE_DIRS})
        target_link_libraries(jpm PRIVATE ${JSC_LIBRARIES})
        message(STATUS "Linked JavaScriptCore via pkg-config: includes=${JSC_INCLUDE_DIRS}, libs=${JSC_LIBRARIES}")
    elseif(WIN32)
        message(FATAL_ERROR "JavaScriptCore embedding on Windows is not configured in this setup.")
    else()
        message(FATAL_ERROR "JavaScriptCore embedding not supported on this platform.")
    endif()
else()
    message(STATUS "JavaScriptCore embedding disabled.")
endif()

# --- Install ---
install(TARGETS jpm DESTINATION bin)

# --- Messages ---
message(STATUS "JPM Project Configured")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# --- Version Command ---
add_custom_target(version
    COMMAND ${CMAKE_COMMAND} -E echo "jpm version ${PROJECT_VERSION}"
    COMMENT "Printing jpm version"
)

# --- Define PROJECT_VERSION for main.cpp if needed ---
add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")
