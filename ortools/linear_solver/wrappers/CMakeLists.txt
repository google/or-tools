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
set(NAME ${PROJECT_NAME}_linear_solver_wrappers)

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
  absl::status
  $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::linear_solver_wrappers ALIAS ${NAME})
