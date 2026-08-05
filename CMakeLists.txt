cmake_minimum_required(VERSION 3.10)
project(proxy VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Default to Release Build (-DCMAKE_BUILD_TYPE=Debug for debug build)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not set — defaulting to Release")
endif()

option(BUILD_TESTS "Build GTest unit tests" OFF)
option(BUILD_BENCHMARKS "Build Google Benchmarks" OFF)

# Define the executable and add source files
add_executable(proxy
    src/main.cpp
    src/TcpServer.cpp
    src/HttpRequest.cpp
    src/ProxyHandler.cpp
    src/LRUCache.cpp
    src/Threadpool.cpp
)

# specify where to find header filer
target_include_directories(ProxyHandler PUBLIC
    include
)

# Add custom target to run after building
add_custom_target(try_run
    COMMAND proxy
    DEPENDS proxy
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)




# ---- Benchmarks ---------------------------------------------------------------------
if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
    )

    # Disable benchmark's own internal tests
    set(BENCHMARK_ENABLE_TESTING       OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS   OFF CACHE BOOL "" FORCE)
    # Prevent -Werror from failing benchmark's build on MinGW (e.g. CacheUnknown switch warning)
    set(BENCHMARK_ENABLE_WERROR        OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(
        proxy_bench
        Gbenchmarks/bench_Httprequest.cpp
        src/HttpRequest.cpp
    )

    target_include_directories(proxy_bench PRIVATE
        include
    )

    target_link_libraries(proxy_bench benchmark::benchmark)
endif()


# ---- Unit Tests --------------------------------------------------------------------------
# if(BUILD_TESTS)
#     # Get GTest through FetchContent at build time
#     include(FetchContent)

#     FetchContent_Declare(
#         googletest
#         URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
#     )

#     # Prevent GTest from overriding CRT Settings
#     set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#     FetchContent_MakeAvailable(googletest)

#     enable_testing()

#     # Test executables
#     add_executable(
#         proxy_tests
#         tests/test_HttpRequest.cpp
#         src/http/HttpRequest.cpp
#     )

#     target_include_directories(proxy_tests PRIVATE
#         src/http
#     )

#     target_link_libraries(proxy_tests GTest::gtest_main)

#     include(GoogleTest)
#     gtest_discover_tests(proxy_tests)
# endif()
