# Copyright 2010-2022 Google LLC
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
#list(FILTER _SRCS EXCLUDE REGEX ".*/solver_interface.cc")

if(USE_SCIP)
  list(APPEND _SRCS ${LPI_GLOP_SRC})
endif()

set(NAME ${PROJECT_NAME}_linear_solver)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::memory
  absl::strings
  absl::status
  absl::str_format
  protobuf::libprotobuf
  $<$<BOOL:${USE_COINOR}>:Coin::Cbc>
  $<$<BOOL:${USE_COINOR}>:Coin::Clp>
  $<$<BOOL:${USE_CPLEX}>:CPLEX::CPLEX>
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
  $<$<BOOL:${USE_HIGHS}>:HIGHS::HIGHS>
  $<$<BOOL:${USE_PDLP}>:Eigen3::Eigen>
  $<$<BOOL:${USE_SCIP}>:libscip>
  $<$<BOOL:${USE_SIRIUS}>:sirius_solver>
  ${PROJECT_NAME}::proto)
#add_library(${PROJECT_NAME}::linear_solver ALIAS ${NAME})
