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