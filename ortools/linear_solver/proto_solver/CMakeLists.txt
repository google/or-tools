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
if(NOT USE_COINOR)
  list(FILTER _SRCS EXCLUDE REGEX "/clp_proto_solver.")
  list(FILTER _SRCS EXCLUDE REGEX "/cbc_proto_solver.")
endif()
if(NOT USE_GLPK)
  list(FILTER _SRCS EXCLUDE REGEX "/glpk_proto_solver.")
endif()
if(NOT USE_HIGHS)
  list(FILTER _SRCS EXCLUDE REGEX "/highs_proto_solver.")
endif()
if(NOT USE_PDLP)
  list(FILTER _SRCS EXCLUDE REGEX "/pdlp_proto_solver.")
endif()
if(NOT USE_SCIP)
  list(FILTER _SRCS EXCLUDE REGEX "/scip_proto_solver.")
endif()

set(NAME ${PROJECT_NAME}_linear_solver_proto_solver)

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
  $<$<BOOL:${USE_PDLP}>:Eigen3::Eigen>
  $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
  $<$<BOOL:${USE_HIGHS}>:highs::highs>
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::linear_solver_proto_solver ALIAS ${NAME})
