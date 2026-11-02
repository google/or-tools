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

# AUTO-GENERATED FROM BUILD FILES - DO NOT EDIT.

include_guard(GLOBAL)

if(BUILD_SCHEDULING)
  ortools_cxx_library(
    NAME ortools_scheduling
    TYPE SHARED
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/scheduling/course_scheduling.cc
      ${PROJECT_SOURCE_DIR}/ortools/scheduling/course_scheduling.h
      ${PROJECT_SOURCE_DIR}/ortools/scheduling/jobshop_scheduling_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/scheduling/jobshop_scheduling_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/scheduling/rcpsp_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/scheduling/rcpsp_parser.h
    COMPILE_DEFINITIONS ${OR_TOOLS_COMPILE_DEFINITIONS}
    COMPILE_OPTIONS ${OR_TOOLS_COMPILE_OPTIONS}
    LINK_OPTIONS ${OR_TOOLS_LINK_OPTIONS}
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools_proto
      ${PROJECT_NAMESPACE}::ortools_core
      absl::check
      absl::core_headers
      absl::flags
      absl::flat_hash_set
      absl::log
      absl::span
      absl::status
      absl::str_format
      absl::strings
      protobuf::libprotobuf
  )
  target_link_libraries(ortools PUBLIC ortools_scheduling)

  ortools_cxx_binary(
    NAME scheduling_course_scheduling_run
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/scheduling/course_scheduling_run.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flags
      absl::log
    NO_INSTALL
  )

  if(BUILD_CXX_TESTING)
    ortools_cxx_test(
      NAME scheduling_rcpsp_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/scheduling/rcpsp_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::string_view
    )

    ortools_cxx_test(
      NAME scheduling_jobshop_scheduling_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/scheduling/jobshop_scheduling_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::string_view
    )

    ortools_cxx_test(
      NAME scheduling_course_scheduling_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/scheduling/course_scheduling_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )
  endif()
endif()
