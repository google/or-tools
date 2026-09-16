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
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_benchmark.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_testing.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_testing.h")
list(FILTER _SRCS EXCLUDE REGEX ".*/lp_test_utils.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/lp_test_utils.h")
set(NAME ${PROJECT_NAME}_lp_data)

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
  absl::synchronization
  absl::status_builder
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  ${RE2_DEPS}
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::lp_data ALIAS ${NAME})

if(BUILD_CXX_TESTING)
  ortools_cxx_library(
    NAME
    ortools_lp_test
    SOURCES
      "lp_test_utils.h"
      "lp_test_utils.cc"
      "lp_types_test.cc"
      "lp_types_testing.h"
      "lp_types_testing.cc"
    TYPE
      STATIC
    LINK_LIBRARIES
    ortools::base_gmock
    ortools::ortools_core
    TESTING
    NO_INSTALL
  )

  # Test
  file(GLOB _TEST_SRCS "*_test.cc")
  foreach(_FULL_FILE_NAME IN LISTS _TEST_SRCS)
    get_filename_component(_NAME ${_FULL_FILE_NAME} NAME_WE)
    get_filename_component(_FILE_NAME ${_FULL_FILE_NAME} NAME)
    ortools_cxx_test(
      NAME
        lp_data_${_NAME}
      SOURCES
        ${_FILE_NAME}
      LINK_LIBRARIES
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        ortools::ortools_core
        ortools::ortools_lp_test
    )
  endforeach()
endif()
