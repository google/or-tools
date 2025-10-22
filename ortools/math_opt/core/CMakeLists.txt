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

add_subdirectory(c_api)

file(GLOB _SRCS "*.h" "*.cc")
#list(APPEND _SRCS "${CMAKE_CURRENT_SOURCE_DIR}/c_api/solver.cc")
list(FILTER _SRCS EXCLUDE REGEX "/[^/]*_test\\.cc$")

set(NAME ${PROJECT_NAME}_math_opt_core)
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC AND BUILD_SHARED_LIBS)
  target_compile_definitions(${NAME} PUBLIC "OR_BUILD_DLL")
  target_compile_definitions(${NAME} PRIVATE "OR_EXPORT")
endif()
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::strings
  GTest::gmock
  protobuf-matchers
  ${PROJECT_NAMESPACE}::math_opt_proto
)
