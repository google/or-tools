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

if(NOT USE_PDLP)
  return()
endif()

list(APPEND _SRCS
  iteration_stats.cc
  iteration_stats.h
  primal_dual_hybrid_gradient.cc
  primal_dual_hybrid_gradient.h
  quadratic_program_io.cc
  quadratic_program_io.h
  quadratic_program.cc
  quadratic_program.h
  scheduler.cc
  scheduler.h
  sharded_optimization_utils.cc
  sharded_optimization_utils.h
  sharded_quadratic_program.cc
  sharded_quadratic_program.h
  sharder.cc
  sharder.h
  solvers_proto_validation.cc
  solvers_proto_validation.h
  termination.cc
  termination.h
  trust_region.cc
  trust_region.h
)

set(NAME ${PROJECT_NAME}_pdlp)

# Will be merge in libortools.so
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::memory
  absl::strings
  absl::str_format
  Eigen3::Eigen
  ${PROJECT_NAMESPACE}::ortools_proto)
