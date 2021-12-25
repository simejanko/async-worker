cmake_minimum_required(VERSION 3.21)
project(workers_manager)

set(CMAKE_CXX_STANDARD 17)

# Boost
find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIR})


add_executable(workers_manager main.cpp)
target_link_libraries(workers_manager ${Boost_LIBRARIES})