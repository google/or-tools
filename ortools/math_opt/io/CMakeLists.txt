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

set(NAME ${PROJECT_NAME}_math_opt_io)
add_library(${NAME} OBJECT)

file(GLOB_RECURSE _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX "/python/.*")
list(FILTER _SRCS EXCLUDE REGEX "/lp/matchers\..*")
if(NOT USE_SCIP)
  list(FILTER _SRCS EXCLUDE REGEX "/lp_parser.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/lp_parser.cc$")
endif()
target_sources(${NAME} PRIVATE ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  ${PROJECT_NAMESPACE}::math_opt_proto
  absl::strings
  $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
)
