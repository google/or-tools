# Copyright 2010-2024 Google LLC
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
list(REMOVE_ITEM _SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/assignment_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/bidirectional_dijkstra_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/bounded_dijkstra_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/christofides_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/cliques_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/dag_constrained_shortest_path_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/dag_shortest_path_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/ebert_graph_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/eulerian_path_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/hamiltonian_path_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/k_shortest_paths_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_assignment_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/max_flow_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/min_cost_flow_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/minimum_spanning_tree_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/multi_dijkstra_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/one_tree_lower_bound_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/perfect_matching_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/shortest_paths_benchmarks.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/shortest_paths_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/solve_flow_model.cc
)

set(NAME ${PROJECT_NAME}_graph)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
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
  ${PROJECT_NAMESPACE}::${PROJECT_NAME}_proto
  $<$<BOOL:${USE_COINOR}>:Coin::Cbc>)
#add_library(${PROJECT_NAMESPACE}::graph ALIAS ${NAME})
