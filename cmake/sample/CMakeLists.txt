cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(Sample VERSION 1.0 LANGUAGES CXX)

find_package(ortools CONFIG REQUIRED)

add_executable(sample main.cpp)
target_link_libraries(sample PRIVATE ortools::ortools)

include(CTest)
if(BUILD_TESTING)
	add_test(NAME sample_UT COMMAND sample)
endif()
