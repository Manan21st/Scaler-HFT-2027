cmake_minimum_required(VERSION 3.16)
project(MarketDataSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization flags for release builds
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")

# Fetch external dependencies
include(FetchContent)

# fmt library for logging
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)

# nlohmann_json for JSON serialization
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Find Boost (ASIO is header-only, but we need Boost.System)
find_package(Boost 1.70 REQUIRED COMPONENTS system)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Common libraries
set(COMMON_LIBS
    fmt::fmt
    nlohmann_json::nlohmann_json
    Boost::system
    pthread
    rt  # Required for shm_open/shm_unlink on Linux
)

# Publisher executable (Process A)
add_executable(publisher
    src/publisher.cpp
)
target_link_libraries(publisher PRIVATE ${COMMON_LIBS})
target_compile_options(publisher PRIVATE -Wall -Wextra -Wpedantic)

# SHM Consumer executable (Process B)
add_executable(shm_consumer
    src/shm_consumer.cpp
)
target_link_libraries(shm_consumer PRIVATE ${COMMON_LIBS})
target_compile_options(shm_consumer PRIVATE -Wall -Wextra -Wpedantic)

# TCP Consumer executable (Process C)
add_executable(tcp_consumer
    src/tcp_consumer.cpp
)
target_link_libraries(tcp_consumer PRIVATE ${COMMON_LIBS})
target_compile_options(tcp_consumer PRIVATE -Wall -Wextra -Wpedantic)

# Installation rules
install(TARGETS publisher shm_consumer tcp_consumer
    RUNTIME DESTINATION bin
)
