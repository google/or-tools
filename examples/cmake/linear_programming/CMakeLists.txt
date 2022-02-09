cmake_minimum_required(VERSION 3.12)
project(linear_programming_example)

if(NOT ORTOOLS_INSTALL)
    message(FATAL_ERROR "ORTOOLS_INSTALL: not found")
endif()

set(CMAKE_PREFIX_PATH ${ORTOOLS_INSTALL})
find_package(ortools)

add_executable(linear_programming linear_programming.cc)
target_link_libraries(linear_programming ortools::ortools)