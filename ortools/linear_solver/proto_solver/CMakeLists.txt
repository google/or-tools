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

list(APPEND _SRCS
  preprocessor.cc
  preprocessor.h
  proto_utils.h
  sat_proto_solver.cc
  sat_proto_solver.h
  sat_solver_utils.cc
  sat_solver_utils.h
)

if(USE_GLOP)
  list(APPEND _SRCS
    glop_proto_solver.cc
    glop_proto_solver.h
  )
endif()
if(USE_GUROBI)
  list(APPEND _SRCS
    gurobi_proto_solver.cc
    gurobi_proto_solver.h
  )
endif()
if(USE_HIGHS)
  list(APPEND _SRCS
    highs_proto_solver.cc
    highs_proto_solver.h
  )
endif()
if(USE_PDLP)
  list(APPEND _SRCS
    pdlp_proto_solver.cc
    pdlp_proto_solver.h
  )
endif()
if(USE_SCIP)
  list(APPEND _SRCS
    scip_params.cc
    scip_params.h
    scip_proto_solver.cc
    scip_proto_solver.h
  )
endif()

set(NAME ${PROJECT_NAME}_linear_solver_proto_solver)

# Will be merge in libortools.so
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
