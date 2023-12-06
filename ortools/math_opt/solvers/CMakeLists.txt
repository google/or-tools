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

set(NAME ${PROJECT_NAME}_math_opt_solvers)
add_library(${NAME} OBJECT)

file(GLOB_RECURSE _SRCS "*.h" "*.cc")
if(NOT USE_GLPK)
  list(FILTER _SRCS EXCLUDE REGEX "/glpk/")
  list(FILTER _SRCS EXCLUDE REGEX "/glpk_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/glpk_.*.cc$")
endif()
if(NOT USE_GUROBI)
  list(FILTER _SRCS EXCLUDE REGEX "/gurobi/")
  list(FILTER _SRCS EXCLUDE REGEX "/gurobi_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/gurobi_.*.cc$")
endif()
if(NOT USE_SCIP)
  list(FILTER _SRCS EXCLUDE REGEX "/gscip/")
  list(FILTER _SRCS EXCLUDE REGEX "/gscip_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/gscip_.*.cc$")
endif()
target_sources(${NAME} PRIVATE ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::strings
  Eigen3::Eigen
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
  $<$<BOOL:${USE_SCIP}>:libscip>
  ${PROJECT_NAMESPACE}::math_opt_proto)
