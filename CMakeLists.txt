cmake_minimum_required(VERSION 3.21)
project(workers_manager)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# Boost
find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIR})


add_executable(workers_manager workers_manager.cpp)
target_link_libraries(workers_manager ${Boost_LIBRARIES})