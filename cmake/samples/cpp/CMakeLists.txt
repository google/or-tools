cmake_minimum_required(VERSION 3.14)
project(Sample VERSION 1.0.0 LANGUAGES CXX)

include(CTest)
find_package(ortools CONFIG REQUIRED)

add_executable(sample main.cpp)
target_compile_features(sample PUBLIC cxx_std_17)
set_target_properties(sample PROPERTIES VERSION ${PROJECT_VERSION})
target_link_libraries(sample PRIVATE ortools::ortools)

if(BUILD_TESTING)
  add_test(NAME sample_UT COMMAND sample)
endif()

include(GNUInstallDirs)
install(TARGETS sample
  EXPORT SampleTargets
  DESTINATION ${CMAKE_INSTALL_BINDIR})
