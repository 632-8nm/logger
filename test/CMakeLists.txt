cmake_minimum_required(VERSION 3.20)
project(Logger)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system filesystem thread)
add_executable(queue_test queue_test.cpp ${CMAKE_SOURCE_DIR}/include/MPMCQueue.hpp)
target_include_directories(queue_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(queue_test PRIVATE Boost::system Boost::filesystem Boost::thread)
