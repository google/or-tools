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

if(BUILD_PACKING)
  ortools_cxx_library(
    NAME ortools_packing
    TYPE SHARED
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_builder.cc
      ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_builder.h
      ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/packing/binpacking_2d_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/packing/binpacking_2d_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/packing/vector_bin_packing_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/packing/vector_bin_packing_parser.h
    COMPILE_DEFINITIONS ${OR_TOOLS_COMPILE_DEFINITIONS}
    COMPILE_OPTIONS ${OR_TOOLS_COMPILE_OPTIONS}
    LINK_OPTIONS ${OR_TOOLS_LINK_OPTIONS}
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools_proto
      ${PROJECT_NAMESPACE}::ortools_core
      absl::btree
      absl::check
      absl::flags
      absl::flat_hash_map
      absl::log
      absl::span
      absl::strings
      absl::time
  )
  set_property(SOURCE
    ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_builder.cc
    APPEND PROPERTY COMPILE_OPTIONS
    "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
  )
  target_link_libraries(ortools PUBLIC ortools_packing)

  ortools_cxx_binary(
    NAME vector_bin_packing
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/packing/vector_bin_packing_main.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flags
      absl::log
      absl::check
      absl::log_globals
      absl::strings
  )

  if(BUILD_CXX_TESTING)
    ortools_cxx_test(
      NAME packing_arc_flow_builder_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_builder_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME packing_arc_flow_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/packing/arc_flow_solver_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME packing_vector_bin_packing_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/packing/vector_bin_packing_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_bintest(
      NAME packing_vector_bin_packing_test
      SCRIPT ${PROJECT_SOURCE_DIR}/ortools/packing/vector_bin_packing.bintest
      ENVIRONMENT
        "BINTEST_vector_bin_packing=$<TARGET_FILE:ortools::vector_bin_packing>"
        "BINTEST_testdata=${PROJECT_SOURCE_DIR}/ortools/packing/testdata/1D__bpp_scholl__bin2data.N2W2B1R0.vbp"
      DISABLED
    )

    ortools_cxx_test(
      NAME packing_binpacking_2d_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/packing/binpacking_2d_parser_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )
  endif()
endif()
