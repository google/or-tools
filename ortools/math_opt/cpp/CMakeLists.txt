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

set(NAME ${PROJECT_NAME}_math_opt_cpp)
add_library(${NAME} OBJECT)

file(GLOB _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX "/matchers\.")

target_sources(${NAME} PRIVATE ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::strings
  ${PROJECT_NAMESPACE}::math_opt_proto)

ortools_cxx_library(
  NAME
    math_opt_matchers
  SOURCES
    "matchers.cc"
    "matchers.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    ortools::base_gmock
    absl::log
    absl::status
    absl::strings
  TESTING
)

if(BUILD_TESTING)
  file(GLOB _TEST_SRCS "*_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/callback_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/formatters_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/message_callback_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/model_solve_parameters_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/parameters_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/solve_impl_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/solve_result_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/solve_test.cc")
  list(FILTER _TEST_SRCS EXCLUDE REGEX "/variable_and_expressions_nc_test.cc")
  foreach(_FULL_FILE_NAME IN LISTS _TEST_SRCS)
    get_filename_component(_NAME ${_FULL_FILE_NAME} NAME_WE)
    get_filename_component(_FILE_NAME ${_FULL_FILE_NAME} NAME)
    ortools_cxx_test(
      NAME
        math_opt_cpp_${_NAME}
      SOURCES
        ${_FILE_NAME}
      LINK_LIBRARIES
        ortools::math_opt_matchers
        benchmark::benchmark
        GTest::gmock
        GTest::gtest_main
    )
  endforeach()

  # These tests are too long so we disable them.
  set_tests_properties(
    cxx_math_opt_cpp_model_test
    cxx_math_opt_cpp_variable_and_expressions_test
    PROPERTIES DISABLED TRUE)

endif()
