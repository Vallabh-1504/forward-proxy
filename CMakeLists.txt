cmake_minimum_required(VERSION 3.10)

project(miniCDN VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main/main.cpp
    src/server/TcpServer.cpp
    src/http/HttpRequest.cpp
    src/proxy/ProxyHandler.cpp
)

include_directories(
    src/server
    src/http
    src/proxy
)

add_executable(miniCDN ${SOURCES})

if(WIN32)
    target_link_libraries(miniCDN ws2_32)
    message(STATUS "Windows detected: Linking ws2_32 library.")
endif()



# Unit Tests
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
