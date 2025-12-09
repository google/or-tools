# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

file(GLOB _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(REMOVE_ITEM _SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/shortest_paths_benchmarks.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/solve_flow_model.cc
)

set(NAME ${PROJECT_NAME}_graph)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
if(MSVC AND BUILD_SHARED_LIBS)
  target_compile_definitions(${NAME} PUBLIC "OR_BUILD_DLL")
  target_compile_definitions(${NAME} PRIVATE "OR_EXPORT")
endif()
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  absl::hash
  absl::meta
  absl::memory
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  ${PROJECT_NAMESPACE}::ortools_proto
  $<$<BOOL:${USE_COINOR}>:Coin::Cbc>)
#add_library(${PROJECT_NAMESPACE}::graph ALIAS ${NAME})

if(BUILD_TESTING)
  file(GLOB _TEST_SRCS "*_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "shortest_paths_test.cc") # timeout
  list(FILTER _TEST_SRCS EXCLUDE REGEX "max_flow_test.cc")
  foreach(_FULL_FILE_NAME IN LISTS _TEST_SRCS)
    get_filename_component(_NAME ${_FULL_FILE_NAME} NAME_WE)
    get_filename_component(_FILE_NAME ${_FULL_FILE_NAME} NAME)
    ortools_cxx_test(
      NAME
        graph_${_NAME}
      SOURCES
        ${_FILE_NAME}
      LINK_LIBRARIES
        absl::status_matchers
        benchmark::benchmark
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )
  endforeach()
  ortools_cxx_test(
    NAME
      graph_max_flow_test
    SOURCES
      "max_flow_test.cc"
    LINK_LIBRARIES
      benchmark::benchmark
      GTest::gtest
      GTest::gtest_main
      GTest::gmock
    COMPILE_DEFINITIONS
    -DROOT_DIR="${CMAKE_SOURCE_DIR}"
  )

  # These tests are too long so we disable them.
  set_tests_properties(
    cxx_graph_min_cost_flow_test
    cxx_graph_bidirectional_dijkstra_test
    PROPERTIES DISABLED TRUE)
endif()
