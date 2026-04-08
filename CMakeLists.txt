cmake_minimum_required(VERSION 3.10)

project(miniCDN VERSION 1.0 LANGUAGES CXX)

option(BUILD_TESTS "Build GTest unit tests" OFF)
option(BUILD_BENCHMARKS "Build Google Benchmarks" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main/main.cpp
    src/server/TcpServer.cpp
    src/http/HttpRequest.cpp
    src/proxy/ProxyHandler.cpp
    src/cache/LRUCache.cpp
)

include_directories(
    src/server
    src/http
    src/proxy
    src/cache
)

add_executable(miniCDN ${SOURCES})

if(WIN32)
    target_link_libraries(miniCDN ws2_32)
    message(STATUS "Windows detected: Linking ws2_32 library.")
endif()


# ---- Unit Tests ----------------------------------------------------------------------------------------
if(BUILD_TESTS)
    # Get GTest through FetchContent at build time
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )

    # Prevent GTest from overriding CRT Settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # Test executables
    add_executable(
        miniCDN_tests
        tests/test_HttpRequest.cpp
        src/http/HttpRequest.cpp
    )

    target_include_directories(miniCDN_tests PRIVATE
        src/http
    )

    target_link_libraries(miniCDN_tests GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(miniCDN_tests)
endif()


# ---- Benchmarks ----------------------------------------------------------------------------
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
        miniCDN_bench
        benchmarks/bench_HttpRequest.cpp
        src/http/HttpRequest.cpp
    )

    target_include_directories(miniCDN_bench PRIVATE
        src/http
    )

    target_link_libraries(miniCDN_bench benchmark::benchmark)
endif()

