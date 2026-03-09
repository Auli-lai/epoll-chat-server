cmake_minimum_required(VERSION 3.10)
project(epoll_chat_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -O2)

# 收集所有源文件
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/network.cpp
    src/protocol.cpp
)

add_executable(epoll_chat_server ${SOURCES})

# 包含当前 src 目录以便找到 head 文件
target_include_directories(epoll_chat_server PRIVATE ${CMAKE_SOURCE_DIR}/src)

install(TARGETS epoll_chat_server DESTINATION bin)