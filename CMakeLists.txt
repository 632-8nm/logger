cmake_minimum_required(VERSION 3.20)
project(Logger)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(test)

set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")

# 添加可执行文件
add_executable(log_client
    ${SOURCE_DIR}/main.cpp
    ${INCLUDE_DIR}/Logger.hpp
    ${INCLUDE_DIR}/Sink.hpp
    ${INCLUDE_DIR}/MPMCQueue.hpp
)
target_include_directories(log_client PRIVATE ${INCLUDE_DIR})


if(UNIX)
    target_link_libraries(log_client pthread)
endif()


add_executable(log_server
    ${SOURCE_DIR}/Server.cpp
    ${INCLUDE_DIR}/Server.hpp
)
target_include_directories(log_server PRIVATE ${INCLUDE_DIR})
