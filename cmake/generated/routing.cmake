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

if(BUILD_ROUTING)
  ortools_cxx_library(
    NAME ortools_routing
    TYPE SHARED
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/alldiff_cst.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/alldiff_cst.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/assignment.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/assignment.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraint_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraint_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraints.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraints.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/count_cst.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/count_cst.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/default_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/default_search.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/deviation.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/deviation.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/diffn.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/diffn.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/element.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/element.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_array.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_array.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_cst.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_cst.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expressions.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expressions.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/graph_constraints.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/graph_constraints.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/interval.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/interval.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/local_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/local_search.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/model_cache.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/model_cache.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/pack.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/pack.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/range_cst.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/range_cst.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/resource.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/resource.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/reversible_data.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/reversible_data.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/reversible_engine.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/reversible_engine.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_constraints.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_constraints.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_expr.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_expr.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_search.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/search.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/search.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sequence_var.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sequence_var.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/table.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/table.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/timetabling.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/timetabling.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/trace.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/trace.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/utilities.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/utilities.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/variables.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/variables.h
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/visitor.cc
      ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/visitor.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/breaks.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/breaks.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/constraints.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/constraints.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/decision_builders.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/decision_builders.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/filter_committables.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/filter_committables.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/filters.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/filters.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/flow.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/fourier_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/fourier_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/ils.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/ils.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/ils_parameters_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/ils_parameters_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/index_manager.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/index_manager.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/insertion_lns.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/insertion_lns.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/lp_scheduling.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/lp_scheduling.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/lp_scheduling_test_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/lp_scheduling_test_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/neighborhoods.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/neighborhoods.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parameters.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parameters.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parameters_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parameters_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/carp_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/carp_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/cvrptw_lib.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/cvrptw_lib.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/dow_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/dow_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/lilim_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/lilim_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/nearp_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/nearp_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/pdtsp_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/pdtsp_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/simple_graph.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/simple_graph.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/solomon_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/solomon_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/solution_serializer.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/solution_serializer.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/tsplib_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/tsplib_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/tsptw_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/tsptw_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/path_state.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/path_state.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/routing.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/routing.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/sat.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/search.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/search.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/types.h
      ${PROJECT_SOURCE_DIR}/ortools/routing/utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/routing/utils.h
    COMPILE_DEFINITIONS ${OR_TOOLS_COMPILE_DEFINITIONS}
    COMPILE_OPTIONS ${OR_TOOLS_COMPILE_OPTIONS}
    LINK_OPTIONS ${OR_TOOLS_LINK_OPTIONS}
    LOCAL_COMPILE_DEFINITIONS $<$<AND:$<BOOL:${MSVC}>,$<BOOL:${BUILD_SHARED_LIBS}>>:OR_ROUTING_EXPORT>
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools_proto
      ${PROJECT_NAMESPACE}::ortools_core
      ZLIB::ZLIB
      absl::algorithm_container
      absl::any_invocable
      absl::bind_front
      absl::bits
      absl::btree
      absl::check
      absl::clock_interface
      absl::core_headers
      absl::die_if_null
      absl::flags
      absl::flat_hash_map
      absl::flat_hash_set
      absl::hash
      absl::inlined_vector
      absl::linked_hash_map
      absl::log
      absl::memory
      absl::no_destructor
      absl::nullability
      absl::random_distributions
      absl::random_random
      absl::span
      absl::status
      absl::status_macros
      absl::statusor
      absl::str_format
      absl::string_view
      absl::strings
      absl::time
      absl::vlog_is_on
      protobuf::libprotobuf
      re2::re2
  )
  if(XCODE)
    file(GENERATE
      OUTPUT ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/routing_version.cpp
      CONTENT "namespace {char* routing_version = \"${PROJECT_VERSION}\";}}")
    target_sources(ortools_routing PRIVATE
      ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/routing_version.cpp)
  endif()
  target_link_libraries(ortools PUBLIC ortools_routing)

  ortools_cxx_binary(
    NAME constraint_solver_alldiff_bench
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/alldiff_bench.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flags
      absl::check
      absl::str_format
    NO_INSTALL
  )

  if(BUILD_CXX_SAMPLES)
    ortools_cxx_binary(
      NAME routing_samples_cvrptw_soft_capacity
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_soft_capacity.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        protobuf::libprotobuf
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_samples_cvrptw
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        protobuf::libprotobuf
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_samples_cvrp_disjoint_tw
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrp_disjoint_tw.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        protobuf::libprotobuf
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_samples_cvrptw_with_breaks
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_breaks.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        absl::strings
        protobuf::libprotobuf
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_samples_cvrptw_with_resources
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_resources.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        protobuf::libprotobuf
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_samples_cvrptw_with_stop_times_and_resources
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_stop_times_and_resources.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        absl::strings
        protobuf::libprotobuf
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_samples_cvrptw_with_refueling
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_refueling.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::check
        absl::log_globals
        absl::random_random
        protobuf::libprotobuf
      NO_INSTALL
    )
  endif()

  if(BUILD_CXX_TESTING)
    ortools_cxx_library(
      NAME constraint_solver_constraint_solver_test_util
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraint_solver_test_util.cc
        ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraint_solver_test_util.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::log
        absl::check
        absl::strings
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME routing_filter_test_utils
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/routing/filter_test_utils.cc
        ${PROJECT_SOURCE_DIR}/ortools/routing/filter_test_utils.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::str_format
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME routing_routing_test_utils
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/routing/routing_test_utils.cc
        ${PROJECT_SOURCE_DIR}/ortools/routing/routing_test_utils.h
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME util_piecewise_linear_function_test_utils
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/util/piecewise_linear_function_test_utils.cc
        ${PROJECT_SOURCE_DIR}/ortools/util/piecewise_linear_function_test_utils.h
      LINK_LIBRARIES
        ortools::ortools
        absl::inlined_vector
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME constraint_solver_assignment_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/assignment_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME constraint_solver_element_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/element_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::flags
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME constraint_solver_local_search_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/local_search_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_fourier_solver_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/fourier_solver_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_filters_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/filters_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::log
        absl::check
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_routing_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/routing_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::bind_front
        absl::check
        absl::random_random
        absl::strings
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME routing_lp_scheduling_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/lp_scheduling_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        protobuf::libprotobuf
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
        ortools::util_piecewise_linear_function_test_utils
      TESTING
      NO_INSTALL
    )

    ortools_cxx_test(
      NAME constraint_solver_alldiff_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/alldiff_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME constraint_solver_assignment_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/assignment_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flat_hash_set
        absl::span
    )

    ortools_cxx_test(
      NAME constraint_solver_compact_table_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/table_test.cc
      COMPILE_DEFINITIONS USE_COMPACT_TABLE
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::core_headers
        absl::flags
        absl::log
    )

    ortools_cxx_test(
      NAME constraint_solver_constraints_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraints_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_count_cst_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/count_cst_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_diffn_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/diffn_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_default_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/default_search_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_domain_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/domain_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_deviation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/deviation_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_element_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/element_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flags
        absl::log
        absl::check
        absl::random_distributions
        absl::str_format
    )

    ortools_cxx_test(
      NAME constraint_solver_expressions_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expressions_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::linked_hash_set
        absl::flags
        absl::log
        absl::check
        absl::str_format
    )

    ortools_cxx_test(
      NAME constraint_solver_expr_array_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_array_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::str_format
    )

    ortools_cxx_test(
      NAME constraint_solver_small_expr_array_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_array_test.cc
      COMPILE_DEFINITIONS SMALL_ARRAY
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::str_format
    )

    ortools_cxx_test(
      NAME constraint_solver_expr_cst_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/expr_cst_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::log
    )

    ortools_cxx_test(
      NAME constraint_solver_graph_constraints_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/graph_constraints_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flat_hash_map
    )

    ortools_cxx_test(
      NAME constraint_solver_graph_delayed_constraints_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/graph_constraints_test.cc
      COMPILE_DEFINITIONS DELAYED_PATH_CUMUL
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flat_hash_map
    )

    ortools_cxx_test(
      NAME constraint_solver_interval_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/interval_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_constraint_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/constraint_solver_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::base
        absl::log
        absl::check
        absl::strings
        absl::str_format
        absl::time
    )

    ortools_cxx_test(
      NAME constraint_solver_local_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/local_search_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flat_hash_set
        absl::check
        absl::strings
        absl::str_format
        absl::time
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME constraint_solver_model_cache_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/model_cache_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_pack_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/pack_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_range_cst_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/range_cst_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::log
    )

    ortools_cxx_test(
      NAME constraint_solver_resource_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/resource_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flags
        absl::check
    )

    ortools_cxx_test(
      NAME constraint_solver_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/search_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::flat_hash_map
        absl::flat_hash_set
        absl::log
        absl::check
        absl::log_entry
        absl::log_sink
        absl::log_sink_registry
        absl::strings
        absl::str_format
        absl::time
    )

    ortools_cxx_test(
      NAME constraint_solver_sched_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_search_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
    )

    ortools_cxx_test(
      NAME constraint_solver_sched_constraints_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_constraints_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_sched_expr_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/sched_expr_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_table_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/table_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
        absl::core_headers
        absl::flags
        absl::log
    )

    ortools_cxx_test(
      NAME constraint_solver_timetabling_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/timetabling_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_reversible_data_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/reversible_data_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_watcher_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/watcher_test.cc
      LINK_LIBRARIES
        ortools::constraint_solver_constraint_solver_test_util
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME constraint_solver_tsp_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/tsp_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::log
        absl::check
        absl::strings
        absl::str_format
        absl::time
        absl::span
    )

    ortools_cxx_bintest(
      NAME constraint_solver_alldiff_bench_test
      SCRIPT ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/alldiff_bench_test.bintest
      ENVIRONMENT "BINTEST_alldiff_bench=$<TARGET_FILE:ortools::constraint_solver_alldiff_bench>"
    )

    ortools_cxx_test(
      NAME graph_one_tree_lower_bound_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/one_tree_lower_bound_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::span
    )

    ortools_cxx_test(
      NAME routing_fourier_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/fourier_solver_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::algorithm_container
        absl::flat_hash_map
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME routing_parameters_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parameters_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::log
        absl::strings
    )

    ortools_cxx_test(
      NAME routing_parameters_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parameters_utils_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_ils_parameters_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/ils_parameters_utils_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
    )

    ortools_cxx_test(
      NAME routing_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/utils_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_neighborhoods_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/neighborhoods_test.cc
      LINK_LIBRARIES
        ortools::routing_routing_test_utils
        protobuf::libprotobuf
        GTest::gtest_main
        absl::flat_hash_set
        absl::bind_front
        absl::span
        ortools::constraint_solver_constraint_solver_test_util
    )

    ortools_cxx_test(
      NAME routing_insertion_lns_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/insertion_lns_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME routing_index_manager_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/index_manager_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_filters_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/filters_test.cc
      LINK_LIBRARIES
        ortools::routing_filter_test_utils
        ortools::routing_routing_test_utils
        protobuf::libprotobuf
        GTest::gtest_main
        absl::algorithm_container
        absl::bind_front
        absl::log
        absl::check
        absl::strings
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME routing_breaks_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/breaks_test.cc
      LINK_LIBRARIES
        ortools::routing_filter_test_utils
        GTest::gtest_main
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME routing_routing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/routing_test.cc
      LINK_LIBRARIES
        ortools::routing_routing_test_utils
        protobuf::libprotobuf
        GTest::gtest_main
        absl::algorithm_container
        absl::flat_hash_map
        absl::flat_hash_set
        absl::inlined_vector
        absl::flags
        absl::bind_front
        absl::check
        absl::random_random
        absl::strings
        absl::str_format
        absl::time
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME routing_decision_builders_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/decision_builders_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_constraints_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/constraints_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        ortools::constraint_solver_constraint_solver_test_util
    )

    ortools_cxx_test(
      NAME routing_ils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/ils_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::nullability
        absl::check
        absl::time
        absl::span
    )

    ortools_cxx_test(
      NAME routing_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/search_test.cc
      LINK_LIBRARIES
        ortools::routing_routing_test_utils
        protobuf::libprotobuf
        GTest::gtest_main
        absl::flat_hash_map
        absl::bind_front
        absl::strings
        absl::time
        absl::simulated_clock
        absl::span
    )

    ortools_cxx_test(
      NAME routing_sat_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/sat_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::flags
        absl::bind_front
        absl::time
      DISABLED
    )

    ortools_cxx_test(
      NAME routing_flow_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/flow_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_filter_committables_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/filter_committables_test.cc
      LINK_LIBRARIES
        ortools::routing_filter_test_utils
        GTest::gtest_main
        absl::algorithm_container
        absl::span
    )

    ortools_cxx_test(
      NAME routing_lp_scheduling_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/lp_scheduling_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::algorithm_container
        absl::core_headers
        absl::flat_hash_set
        absl::inlined_vector
        absl::check
        absl::int128
        absl::random_random
        absl::random_distributions
        absl::time
        absl::span
        ortools::util_piecewise_linear_function_test_utils
      DISABLED
    )

    ortools_cxx_test(
      NAME routing_parsers_simple_graph_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/simple_graph_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::hash_testing
    )

    ortools_cxx_test(
      NAME routing_parsers_solomon_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/solomon_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::strings
    )

    ortools_cxx_test(
      NAME routing_parsers_lilim_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/lilim_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        absl::string_view
    )

    ortools_cxx_test(
      NAME routing_parsers_carp_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/carp_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::scoped_mock_log
        absl::strings
    )

    ortools_cxx_test(
      NAME routing_parsers_nearp_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/nearp_parser_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_parsers_pdtsp_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/pdtsp_parser_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME routing_parsers_tsplib_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/tsplib_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::btree
        absl::status
        absl::strings
        absl::str_format
    )

    ortools_cxx_test(
      NAME routing_parsers_tsptw_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/tsptw_parser_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::string_view
    )

    if(NOT WIN32)
      ortools_cxx_test(
        NAME routing_parsers_solution_serializer_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/parsers/solution_serializer_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::check
          absl::time
      )
    endif()

    ortools_cxx_test(
      NAME util_piecewise_linear_function_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/piecewise_linear_function_test.cc
      LINK_LIBRARIES
        ortools::util_piecewise_linear_function_test_utils
        GTest::gtest_main
        absl::random_distributions
    )

    if(BUILD_CXX_SAMPLES)
      ortools_cxx_test(
        NAME constraint_solver_samples_cp_is_fun_cp_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/cp_is_fun_cp.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME constraint_solver_samples_minimal_jobshop_cp_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/minimal_jobshop_cp.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME constraint_solver_samples_nqueens_cp_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/nqueens_cp.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::strings
      )

      ortools_cxx_test(
        NAME constraint_solver_samples_nurses_cp_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/nurses_cp.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME constraint_solver_samples_rabbits_and_pheasants_cp_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/rabbits_and_pheasants_cp.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME constraint_solver_samples_simple_cp_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/simple_cp_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME constraint_solver_samples_simple_ls_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/simple_ls_program.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_bintest(
        NAME routing_samples_cvrptw_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_test.bintest
        ENVIRONMENT "BINTEST_cvrptw=$<TARGET_FILE:ortools::routing_samples_cvrptw>"
      )

      ortools_cxx_bintest(
        NAME routing_samples_cvrp_disjoint_tw_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrp_disjoint_tw_test.bintest
        ENVIRONMENT "BINTEST_cvrp_disjoint_tw=$<TARGET_FILE:ortools::routing_samples_cvrp_disjoint_tw>"
      )

      ortools_cxx_bintest(
        NAME routing_samples_cvrptw_with_resources_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_resources_test.bintest
        ENVIRONMENT "BINTEST_cvrptw_with_resources=$<TARGET_FILE:ortools::routing_samples_cvrptw_with_resources>"
        DISABLED
      )

      ortools_cxx_bintest(
        NAME routing_samples_cvrptw_with_stop_times_and_resources_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_stop_times_and_resources_test.bintest
        ENVIRONMENT "BINTEST_cvrptw_with_stop_times_and_resources=$<TARGET_FILE:ortools::routing_samples_cvrptw_with_stop_times_and_resources>"
      )

      ortools_cxx_bintest(
        NAME routing_samples_cvrptw_with_refueling_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_with_refueling_test.bintest
        ENVIRONMENT "BINTEST_cvrptw_with_refueling=$<TARGET_FILE:ortools::routing_samples_cvrptw_with_refueling>"
        DISABLED
      )

      ortools_cxx_test(
        NAME routing_samples_simple_routing_program_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/simple_routing_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_tsp_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_tsp_circuit_board_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_circuit_board.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
        DISABLED
      )

      ortools_cxx_test(
        NAME routing_samples_tsp_cities_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_cities.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_tsp_cities_routes_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_cities_routes.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_tsp_distance_matrix_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_distance_matrix.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_breaks_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_breaks.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
          absl::strings
        DISABLED
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_capacity_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_capacity.cc
        LINK_LIBRARIES
          protobuf::libprotobuf
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_drop_nodes_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_drop_nodes.cc
        LINK_LIBRARIES
          protobuf::libprotobuf
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_global_span_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_global_span.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_initial_routes_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_initial_routes.cc
        LINK_LIBRARIES
          protobuf::libprotobuf
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_starts_ends_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_starts_ends.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_pickup_delivery_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_pickup_delivery.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_pickup_delivery_fifo_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_pickup_delivery_fifo.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_pickup_delivery_lifo_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_pickup_delivery_lifo.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_resources_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_resources.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_routes_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_routes.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_solution_callback_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_solution_callback.cc
        LINK_LIBRARIES
          protobuf::libprotobuf
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_time_windows_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_time_windows.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrp_with_time_limit_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_with_time_limit.cc
        LINK_LIBRARIES
          protobuf::libprotobuf
          absl::log
          absl::log_flags
          absl::log_globals
      )

      ortools_cxx_test(
        NAME routing_samples_vrptw_store_solution_data_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrptw_store_solution_data.cc
        LINK_LIBRARIES
          absl::log
          absl::log_flags
          absl::log_globals
      )
    endif()
  endif()
endif()
