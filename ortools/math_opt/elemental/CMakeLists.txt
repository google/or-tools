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

set(NAME ${PROJECT_NAME}_math_opt_elemental)
add_library(${NAME} OBJECT)

file(GLOB_RECURSE _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX "/elemental_matcher.*")
list(FILTER _SRCS EXCLUDE REGEX "/python/.*")
list(FILTER _SRCS EXCLUDE REGEX "/codegen/codegen.cc")
target_sources(${NAME} PRIVATE ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  ${PROJECT_NAMESPACE}::math_opt_proto
  absl::strings
)

ortools_cxx_library(
  NAME
    math_opt_elemental_matcher
  SOURCES
    "elemental_matcher.cc"
    "elemental_matcher.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    absl::log
    absl::status
    absl::strings
    GTest::gmock
  TESTING
)

if(BUILD_TESTING)
  file(GLOB _TEST_SRCS "*_test.cc")
  list(FILTER _TEST_SRCS
    EXCLUDE REGEX "elemental_export_model_update_test.cc$")
  list(FILTER _TEST_SRCS
    EXCLUDE REGEX "elemental_from_proto_fuzz_test.cc$") # need fuzztest
  foreach(_FULL_FILE_NAME IN LISTS _TEST_SRCS)
    get_filename_component(_NAME ${_FULL_FILE_NAME} NAME_WE)
    get_filename_component(_FILE_NAME ${_FULL_FILE_NAME} NAME)
    ortools_cxx_test(
      NAME
        math_opt_elemental_${_NAME}
      SOURCES
        ${_FILE_NAME}
      LINK_LIBRARIES
        benchmark::benchmark
        GTest::gmock
        GTest::gtest_main
        ortools::math_opt_matchers
        ortools::math_opt_elemental_matcher
    )
  endforeach()
endif()
