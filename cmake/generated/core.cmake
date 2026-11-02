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

ortools_cxx_library(
  NAME ortools
  TYPE SHARED
  SOURCES
    ${PROJECT_SOURCE_DIR}/ortools/init/init.cc
    ${PROJECT_SOURCE_DIR}/ortools/init/init.h
  COMPILE_DEFINITIONS ${OR_TOOLS_COMPILE_DEFINITIONS}
  COMPILE_OPTIONS ${OR_TOOLS_COMPILE_OPTIONS}
  LINK_OPTIONS ${OR_TOOLS_LINK_OPTIONS}
  LOCAL_COMPILE_DEFINITIONS $<$<AND:$<BOOL:${MSVC}>,$<BOOL:${BUILD_SHARED_LIBS}>>:OR_INIT_EXPORT>
  LINK_LIBRARIES
    ${PROJECT_NAMESPACE}::ortools_proto
    absl::flags
    absl::log_globals
    absl::string_view
)
set_target_properties(ortools PROPERTIES
  INTERFACE_ortools_MAJOR_VERSION ${PROJECT_VERSION_MAJOR}
  COMPATIBLE_INTERFACE_STRING ortools_MAJOR_VERSION
)
if(XCODE)
  file(GENERATE
    OUTPUT ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/version.cpp
    CONTENT "namespace {char* version = \"${PROJECT_VERSION}\";}}")
  target_sources(ortools PRIVATE
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/version.cpp)
endif()

if(BUILD_CORE)
  ortools_cxx_library(
    NAME ortools_core
    TYPE SHARED
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/adjustable_k_ary_heap.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/auto_radix_sort.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/binary_indexed_tree.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/binary_search.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/dense_doubly_linked_list.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/duplicate_remover.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/duplicate_remover.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/dynamic_partition.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/dynamic_partition.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/dynamic_permutation.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/dynamic_permutation.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/find_graph_symmetries.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/find_graph_symmetries.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/float_ordinal_bijection.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/hungarian.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/hungarian.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/knapsack_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/knapsack_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/multikey_radix_sort.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/n_choose_k.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/n_choose_k.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/radix_sort.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/space_saving_most_frequent.h
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/sparse_permutation.cc
      ${PROJECT_SOURCE_DIR}/ortools/algorithms/sparse_permutation.h
      ${PROJECT_SOURCE_DIR}/ortools/base/accurate_sum.h
      ${PROJECT_SOURCE_DIR}/ortools/base/adjustable_priority_queue.h
      ${PROJECT_SOURCE_DIR}/ortools/base/base_export.h
      ${PROJECT_SOURCE_DIR}/ortools/base/basictypes.h
      ${PROJECT_SOURCE_DIR}/ortools/base/bzip2file.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/bzip2file.h
      ${PROJECT_SOURCE_DIR}/ortools/base/constant_divisor.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/constant_divisor.h
      ${PROJECT_SOURCE_DIR}/ortools/base/container_logging.h
      ${PROJECT_SOURCE_DIR}/ortools/base/dump_vars.h
      ${PROJECT_SOURCE_DIR}/ortools/base/file.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/file.h
      ${PROJECT_SOURCE_DIR}/ortools/base/filesystem.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/filesystem.h
      ${PROJECT_SOURCE_DIR}/ortools/base/gzipfile.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/gzipfile.h
      ${PROJECT_SOURCE_DIR}/ortools/base/gzipstring.h
      ${PROJECT_SOURCE_DIR}/ortools/base/hash.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/hash.h
      ${PROJECT_SOURCE_DIR}/ortools/base/helpers.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/helpers.h
      ${PROJECT_SOURCE_DIR}/ortools/base/init_google.h
      ${PROJECT_SOURCE_DIR}/ortools/base/iterator_adaptors.h
      ${PROJECT_SOURCE_DIR}/ortools/base/log_severity.h
      ${PROJECT_SOURCE_DIR}/ortools/base/macros/buildenv.h
      ${PROJECT_SOURCE_DIR}/ortools/base/macros/os.h
      ${PROJECT_SOURCE_DIR}/ortools/base/macros/os_support.h
      ${PROJECT_SOURCE_DIR}/ortools/base/map_util.h
      ${PROJECT_SOURCE_DIR}/ortools/base/mathutil.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/mathutil.h
      ${PROJECT_SOURCE_DIR}/ortools/base/murmur.h
      ${PROJECT_SOURCE_DIR}/ortools/base/options.h
      ${PROJECT_SOURCE_DIR}/ortools/base/parse_text_proto.h
      ${PROJECT_SOURCE_DIR}/ortools/base/path.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/path.h
      ${PROJECT_SOURCE_DIR}/ortools/base/proto_enum_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/base/protobuf_util.h
      ${PROJECT_SOURCE_DIR}/ortools/base/protoutil.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/protoutil.h
      ${PROJECT_SOURCE_DIR}/ortools/base/recordio.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/recordio.h
      ${PROJECT_SOURCE_DIR}/ortools/base/safe_hash_map.h
      ${PROJECT_SOURCE_DIR}/ortools/base/status_builder.h
      ${PROJECT_SOURCE_DIR}/ortools/base/stl_util.h
      ${PROJECT_SOURCE_DIR}/ortools/base/strong_int.h
      ${PROJECT_SOURCE_DIR}/ortools/base/strong_vector.h
      ${PROJECT_SOURCE_DIR}/ortools/base/temp_file.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/temp_file.h
      ${PROJECT_SOURCE_DIR}/ortools/base/temp_path.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/temp_path.h
      ${PROJECT_SOURCE_DIR}/ortools/base/threadpool.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/threadpool.h
      ${PROJECT_SOURCE_DIR}/ortools/base/timer.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/timer.h
      ${PROJECT_SOURCE_DIR}/ortools/base/top_n.h
      ${PROJECT_SOURCE_DIR}/ortools/base/types.h
      ${PROJECT_SOURCE_DIR}/ortools/base/unique_array.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/unique_array.h
      ${PROJECT_SOURCE_DIR}/ortools/base/version.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/version.h
      ${PROJECT_SOURCE_DIR}/ortools/base/zipfile.cc
      ${PROJECT_SOURCE_DIR}/ortools/base/zipfile.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/boolean_problem.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/boolean_problem.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_base.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_base.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_fs.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_fs.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_lns.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_lns.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_ls.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_ls.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_portfolio.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_portfolio.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_solution.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_solution.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_types.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/bop_util.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/complete_optimizer.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/complete_optimizer.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/integral_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/integral_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/bop/lp_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/bop/lp_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/basis_representation.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/basis_representation.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/dual_edge_norms.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/dual_edge_norms.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/entering_variable.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/entering_variable.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/initial_basis.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/initial_basis.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/lp_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/lp_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/lu_factorization.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/lu_factorization.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/markowitz.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/markowitz.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/parameters_validation.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/parameters_validation.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/preprocessor.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/preprocessor.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/pricing.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/primal_edge_norms.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/primal_edge_norms.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/rank_one_update.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/reduced_costs.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/reduced_costs.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/revised_simplex.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/revised_simplex.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/status.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/status.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/update_row.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/update_row.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/variable_values.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/variable_values.h
      ${PROJECT_SOURCE_DIR}/ortools/glop/variables_info.cc
      ${PROJECT_SOURCE_DIR}/ortools/glop/variables_info.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/assignment.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/assignment.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/bfs.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/bidirectional_dijkstra.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/bounded_dijkstra.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/christofides.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/cliques.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/cliques.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/connected_components.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/connected_components.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/dag_connectivity.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/dag_connectivity.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/dag_constrained_shortest_path.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/dag_constrained_shortest_path.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/dag_shortest_path.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/dag_shortest_path.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/eulerian_path.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/flow_graph.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/generic_graph.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/generic_max_flow.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/graph.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/graph_generator.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/hamiltonian_path.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/hash_or_tree_container.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/index.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/io.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/iterators.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/k_shortest_paths.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/linear_assignment.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/linear_assignment.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/max_flow.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/max_flow.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/min_cost_flow.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/min_cost_flow.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_spanning_tree.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_vertex_cover.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_vertex_cover.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/multi_dijkstra.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/one_tree_lower_bound.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/perfect_matching.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/perfect_matching.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/rooted_tree.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/shortest_paths.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/strongly_connected_components.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/test_util.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/topologicalsorter.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/topologicalsorter.h
      ${PROJECT_SOURCE_DIR}/ortools/graph/util.cc
      ${PROJECT_SOURCE_DIR}/ortools/graph/util.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glop_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glop_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/gurobi_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/gurobi_util.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_expr.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_expr.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_solver_callback.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_solver_callback.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/model_exporter.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/model_exporter.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/model_exporter_swig_helper.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/model_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/model_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/preprocessor.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/preprocessor.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/proto_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/sat_proto_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/sat_proto_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/sat_solver_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/sat_solver_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/sat_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/solve_mp_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/solve_mp_model.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/wrappers/model_builder_helper.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/wrappers/model_builder_helper.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_decomposer.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_decomposer.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_print_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_print_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_types.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_types.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_scaler.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_scaler.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader_template.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader_template.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/permutation.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/proto_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/proto_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/scattered_vector.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sol_reader.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sol_reader.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_column.cc
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_column.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_row.h
      ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_vector.h
      ${PROJECT_SOURCE_DIR}/ortools/port/attributes.h
      ${PROJECT_SOURCE_DIR}/ortools/port/file.cc
      ${PROJECT_SOURCE_DIR}/ortools/port/file.h
      ${PROJECT_SOURCE_DIR}/ortools/port/proto_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/port/proto_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/port/sysinfo.cc
      ${PROJECT_SOURCE_DIR}/ortools/port/sysinfo.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_distances_propagator.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_distances_propagator.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_mandatory_overlap_propagator.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_mandatory_overlap_propagator.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_packing_brute_force.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_packing_brute_force.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_rectangle_presolve.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_rectangle_presolve.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_try_edge_propagator.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/2d_try_edge_propagator.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/all_different.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/all_different.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/c_api/cp_solver_c.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/c_api/cp_solver_c.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/circuit.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/circuit.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/clause.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/clause.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/combine_solutions.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/combine_solutions.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/constraint_violation.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/constraint_violation.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/container.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/continuous_prober.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/continuous_prober.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraint_presolve.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraint_presolve.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraints.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraints.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_checker.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_checker.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_copy.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_copy.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_expand.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_expand.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_lns.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_lns.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_loader.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_loader.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_mapping.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_postsolve.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_postsolve.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_presolve.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_presolve.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_search.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver_helpers.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver_helpers.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver_logging.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver_logging.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_symmetries.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_symmetries.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_table.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_table.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_test_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_test_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative_energy.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative_energy.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/cuts.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/cuts.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/debug_solution.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/debug_solution.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/diffn.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/diffn.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_cuts.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_cuts.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_util.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/diophantine.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/diophantine.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/disjunctive.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/disjunctive.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/drat_checker.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/drat_checker.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/encoding.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/encoding.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement_helper.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement_helper.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/feasibility_jump.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/feasibility_jump.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/feasibility_pump.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/feasibility_pump.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/gate_congruence_closure.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/gate_congruence_closure.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/gate_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/gate_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/implied_bounds.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/implied_bounds.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/inclusion.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_base.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_base.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_expr.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_expr.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_resolution.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_resolution.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/integer_search.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/intervals.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/intervals.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/lb_tree_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/lb_tree_search.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_constraint_manager.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_constraint_manager.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_model.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_programming_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_programming_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_propagation.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_propagation.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_relaxation.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/linear_relaxation.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/lp_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/lp_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_checker.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_checker.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_proof_handler.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_proof_handler.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/max_hs.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/max_hs.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/model.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/mp_model_wrapper.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/mp_model_wrapper.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/no_overlap_2d_helper.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/no_overlap_2d_helper.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/old_precedences_propagator.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/old_precedences_propagator.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/opb_reader.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/optimization.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/optimization.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/parameters_validation.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/parameters_validation.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/pb_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/pb_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/precedences.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/precedences.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_context.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_context.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_encoding.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_encoding.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_util.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/primary_variables.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/primary_variables.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/probing.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/probing.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/pseudo_costs.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/pseudo_costs.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/linear_expr.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/linear_expr.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/recordio.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/recordio.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/restart.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/restart.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/rins.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/rins.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/routing_cuts.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/routing_cuts.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_assignment.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_base.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_clause.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_clause.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_cnf_reader.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_decision.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_decision.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_inprocessing.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_inprocessing.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_literal.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_sweeping.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_sweeping.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_trail.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/sat_trail.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_cuts.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_cuts.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_helpers.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_helpers.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_local_search.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_local_search.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_model.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/shaving_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/shaving_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/simplification.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/simplification.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/solution_crush.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/solution_crush.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/stat_tables.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/stat_tables.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/subsolver.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/subsolver.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/swig_helper.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/swig_helper.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry_util.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/synchronization.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/synchronization.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/table.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/table.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/timetable.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/timetable.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/timetable_edgefinding.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/timetable_edgefinding.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/util.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/util.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/var_domination.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/var_domination.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/variable_expand.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/variable_expand.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/vivification.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/vivification.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/work_assignment.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/work_assignment.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/zero_half_cuts.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/zero_half_cuts.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/assignment.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/assignment.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/base_types.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/capacity_invariant.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/capacity_invariant.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/capacity_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/capacity_model.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/reporting.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/reporting.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_cft.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_cft.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_heuristics.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_heuristics.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_invariant.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_invariant.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_lagrangian.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_lagrangian.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_model.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_reader.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_reader.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_submodel.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_submodel.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_views.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/views.h
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/dynamic_library.h
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/gurobi_environment.cc
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/gurobi_environment.h
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/xpress_environment.cc
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/xpress_environment.h
      ${PROJECT_SOURCE_DIR}/ortools/util/adaptative_parameter_value.h
      ${PROJECT_SOURCE_DIR}/ortools/util/affine_relation.h
      ${PROJECT_SOURCE_DIR}/ortools/util/bitset.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/bitset.h
      ${PROJECT_SOURCE_DIR}/ortools/util/cached_log.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/cached_log.h
      ${PROJECT_SOURCE_DIR}/ortools/util/data_path_resolver.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/data_path_resolver.h
      ${PROJECT_SOURCE_DIR}/ortools/util/dense_set.h
      ${PROJECT_SOURCE_DIR}/ortools/util/deterministic_time.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/deterministic_time.h
      ${PROJECT_SOURCE_DIR}/ortools/util/file_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/file_util.h
      ${PROJECT_SOURCE_DIR}/ortools/util/filelineiter.h
      ${PROJECT_SOURCE_DIR}/ortools/util/fixed_shape_binary_tree.h
      ${PROJECT_SOURCE_DIR}/ortools/util/flat_matrix.h
      ${PROJECT_SOURCE_DIR}/ortools/util/fp_roundtrip_conv.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/fp_roundtrip_conv.h
      ${PROJECT_SOURCE_DIR}/ortools/util/fp_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/fp_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/util/full_precision_inequalities.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/full_precision_inequalities.h
      ${PROJECT_SOURCE_DIR}/ortools/util/functions_swig_helpers.h
      ${PROJECT_SOURCE_DIR}/ortools/util/integer_pq.h
      ${PROJECT_SOURCE_DIR}/ortools/util/lazy_mutable_copy.h
      ${PROJECT_SOURCE_DIR}/ortools/util/logging.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/logging.h
      ${PROJECT_SOURCE_DIR}/ortools/util/monoid_operation_tree.h
      ${PROJECT_SOURCE_DIR}/ortools/util/mp_model_solution_checker.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/mp_model_solution_checker.h
      ${PROJECT_SOURCE_DIR}/ortools/util/parse_proto.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/parse_proto.h
      ${PROJECT_SOURCE_DIR}/ortools/util/permutation.h
      ${PROJECT_SOURCE_DIR}/ortools/util/piecewise_linear_function.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/piecewise_linear_function.h
      ${PROJECT_SOURCE_DIR}/ortools/util/proto_tools.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/proto_tools.h
      ${PROJECT_SOURCE_DIR}/ortools/util/qap_reader.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/qap_reader.h
      ${PROJECT_SOURCE_DIR}/ortools/util/random_engine.h
      ${PROJECT_SOURCE_DIR}/ortools/util/random_subset.h
      ${PROJECT_SOURCE_DIR}/ortools/util/range_minimum_query.h
      ${PROJECT_SOURCE_DIR}/ortools/util/range_query_function.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/range_query_function.h
      ${PROJECT_SOURCE_DIR}/ortools/util/rational_approximation.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/rational_approximation.h
      ${PROJECT_SOURCE_DIR}/ortools/util/recycled_var_guard.h
      ${PROJECT_SOURCE_DIR}/ortools/util/return_macros.h
      ${PROJECT_SOURCE_DIR}/ortools/util/rev.h
      ${PROJECT_SOURCE_DIR}/ortools/util/running_stat.h
      ${PROJECT_SOURCE_DIR}/ortools/util/saturated_arithmetic.h
      ${PROJECT_SOURCE_DIR}/ortools/util/scheduling.h
      ${PROJECT_SOURCE_DIR}/ortools/util/sigint.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/sigint.h
      ${PROJECT_SOURCE_DIR}/ortools/util/solve_interrupter.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/solve_interrupter.h
      ${PROJECT_SOURCE_DIR}/ortools/util/sort.h
      ${PROJECT_SOURCE_DIR}/ortools/util/sorted_interval_list.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/sorted_interval_list.h
      ${PROJECT_SOURCE_DIR}/ortools/util/stats.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/stats.h
      ${PROJECT_SOURCE_DIR}/ortools/util/status_macros.h
      ${PROJECT_SOURCE_DIR}/ortools/util/status_streaming.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/status_streaming.h
      ${PROJECT_SOURCE_DIR}/ortools/util/string_array.h
      ${PROJECT_SOURCE_DIR}/ortools/util/strong_index_map.h
      ${PROJECT_SOURCE_DIR}/ortools/util/strong_integers.h
      ${PROJECT_SOURCE_DIR}/ortools/util/testing_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/testing_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/util/time_limit.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/time_limit.h
      ${PROJECT_SOURCE_DIR}/ortools/util/tuple_set.h
    COMPILE_DEFINITIONS ${OR_TOOLS_COMPILE_DEFINITIONS}
    COMPILE_OPTIONS ${OR_TOOLS_COMPILE_OPTIONS}
    LINK_OPTIONS ${OR_TOOLS_LINK_OPTIONS}
    LOCAL_COMPILE_DEFINITIONS OR_EXPORT
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools_proto
      ${CMAKE_DL_LIBS}
      ${COINOR_DEPS}
      ${CPLEX_DEPS}
      Threads::Threads
      $<$<BOOL:${USE_COINOR}>:Coin::ClpSolver>
      $<$<BOOL:${USE_COINOR}>:Coin::CoinUtils>
      $<$<BOOL:${USE_COINOR}>:Coin::OsiClp>
      $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
      $<$<BOOL:${USE_HIGHS}>:highs::highs>
      $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      BZip2::BZip2
      ZLIB::ZLIB
      absl::algorithm_container
      absl::any_invocable
      absl::base
      absl::bind_front
      absl::bits
      absl::btree
      absl::check
      absl::cleanup
      absl::config
      absl::cord
      absl::core_headers
      absl::die_if_null
      absl::fixed_array
      absl::flags
      absl::flags_parse
      absl::flags_usage
      absl::flat_hash_map
      absl::flat_hash_set
      absl::function_ref
      absl::hash
      absl::hash_container_defaults
      absl::inlined_vector
      absl::int128
      absl::internal_optional_ref
      absl::linked_hash_map
      absl::log
      absl::log_globals
      absl::log_initialize
      absl::log_severity
      absl::log_streamer
      absl::memory
      absl::no_destructor
      absl::nullability
      absl::optional
      absl::overload
      absl::random_bit_gen_ref
      absl::random_distributions
      absl::random_random
      absl::random_seed_sequences
      absl::source_location
      absl::span
      absl::stacktrace
      absl::status
      absl::status_builder
      absl::status_macros
      absl::statusor
      absl::str_format
      absl::string_view
      absl::strings
      absl::synchronization
      absl::time
      absl::type_traits
      absl::vlog_is_on
      protobuf::libprotobuf
      re2::re2
  )
  if(USE_BOP)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/bop_interface.cc
    )
  endif()
  if(USE_COINOR)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/cbc_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/clp_interface.cc
    )
  endif()
  if(USE_CPLEX)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/cplex_interface.cc
    )
  endif()
  if(USE_GLOP)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glop_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/glop_proto_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/glop_proto_solver.h
    )
  endif()
  if(USE_GLPK)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glpk_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/glpk/glpk_computational_form.h
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/glpk/glpk_env_deleter.cc
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/glpk/glpk_env_deleter.h
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/glpk/glpk_formatters.cc
      ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/glpk/glpk_formatters.h
    )
  endif()
  if(USE_GUROBI)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/gurobi_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/gurobi_proto_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/gurobi_proto_solver.h
    )
  endif()
  if(USE_HIGHS)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/highs_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/highs_proto_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/highs_proto_solver.h
    )
  endif()
  if(USE_PDLP)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/pdlp_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/pdlp_proto_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/pdlp_proto_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/iteration_stats.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/iteration_stats.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/primal_dual_hybrid_gradient.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/primal_dual_hybrid_gradient.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/quadratic_program.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/quadratic_program.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/quadratic_program_io.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/quadratic_program_io.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/scheduler.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/scheduler.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharded_optimization_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharded_optimization_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharded_quadratic_program.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharded_quadratic_program.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharder.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharder.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/solvers_proto_validation.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/solvers_proto_validation.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/termination.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/termination.h
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/trust_region.cc
      ${PROJECT_SOURCE_DIR}/ortools/pdlp/trust_region.h
    )
  endif()
  if(USE_SCIP)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/scip_params.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/scip_params.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/scip_proto_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/scip_proto_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_callback.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_callback.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_helper_macros.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_helper_macros.h
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_interface.cc
    )
  endif()
  if(USE_XPRESS)
    target_sources(ortools_core PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/xpress_interface.cc
    )
  endif()
  if(APPLE)
    target_link_options(ortools_core PRIVATE "SHELL:-framework CoreFoundation")
  endif()
  if(UNIX AND NOT APPLE)
    target_link_options(ortools_core PRIVATE "SHELL:-Wl,--no-as-needed -ldl")
  endif()
  if(USE_BOP)
    target_link_libraries(ortools_core PUBLIC absl::core_headers absl::log absl::check absl::time protobuf::libprotobuf)
  endif()
  if(USE_COINOR)
    target_link_libraries(ortools_core PUBLIC absl::core_headers absl::log absl::check absl::str_format absl::time absl::status absl::strings)
  endif()
  if(USE_GLOP)
    target_link_libraries(ortools_core PUBLIC absl::core_headers absl::log absl::check absl::time absl::overload absl::strings)
  endif()
  if(USE_GLPK)
    target_link_libraries(ortools_core PUBLIC absl::core_headers absl::check absl::str_format absl::time absl::log absl::strings)
  endif()
  if(USE_GUROBI)
    target_link_libraries(ortools_core PUBLIC absl::core_headers absl::flat_hash_map absl::flat_hash_set absl::flags absl::log absl::check absl::die_if_null absl::status absl::str_format absl::synchronization absl::time absl::cleanup absl::statusor absl::strings absl::string_view)
  endif()
  if(USE_HIGHS)
    target_link_libraries(ortools_core PUBLIC absl::log absl::check absl::status absl::strings absl::time absl::statusor absl::string_view protobuf::libprotobuf)
  endif()
  if(USE_PDLP)
    target_link_libraries(ortools_core PUBLIC absl::log absl::check absl::status absl::strings absl::time protobuf::libprotobuf absl::status_macros absl::statusor absl::any_invocable absl::synchronization Eigen3::Eigen absl::random_distributions absl::algorithm_container absl::core_headers absl::nullability absl::str_format absl::base absl::flat_hash_map absl::status_builder absl::vlog_is_on)
  endif()
  if(USE_SCIP)
    target_link_libraries(ortools_core PUBLIC absl::log absl::check absl::span absl::core_headers absl::cleanup absl::flat_hash_map absl::flags absl::status absl::str_format absl::synchronization absl::time absl::base absl::nullability absl::status_macros absl::strings absl::string_view absl::btree absl::statusor)
  endif()
  if(USE_XPRESS)
    target_link_libraries(ortools_core PUBLIC absl::algorithm_container absl::core_headers absl::btree absl::flat_hash_map absl::log absl::check absl::strings absl::str_format absl::synchronization absl::time)
  endif()
  set_property(SOURCE ${PROJECT_SOURCE_DIR}/ortools/base/version.cc
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OR_TOOLS_MAJOR=${PROJECT_VERSION_MAJOR}"
    "OR_TOOLS_MINOR=${PROJECT_VERSION_MINOR}"
    "OR_TOOLS_PATCH=${PROJECT_VERSION_PATCH}"
  )
  if(WIN32)
    target_link_libraries(ortools_core PUBLIC psapi.lib ws2_32.lib)
  endif()
  set_property(SOURCE
    ${PROJECT_SOURCE_DIR}/ortools/linear_solver/model_validator.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data_utils.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_decomposer.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_parser.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_print_utils.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_types.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_utils.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_scaler.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_utils.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader_template.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/proto_utils.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/sol_reader.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse.cc
    ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_column.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/2d_packing_brute_force.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/2d_rectangle_presolve.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/2d_try_edge_propagator.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/circuit.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/clause.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraints.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative_energy.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/diffn.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_util.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/disjunctive.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/drat_checker.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/encoding.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement_helper.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/integer.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/integer_base.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/integer_expr.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/integer_resolution.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/intervals.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/lp_utils.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_checker.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_proof_handler.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/max_hs.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/no_overlap_2d_helper.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/optimization.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/pb_constraint.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/precedences.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/probing.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/restart.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/sat_decision.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/sat_inprocessing.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/sat_solver.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_helpers.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/simplification.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry_util.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/table.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/timetable.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/timetable_edgefinding.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/util.cc
    ${PROJECT_SOURCE_DIR}/ortools/sat/var_domination.cc
    APPEND PROPERTY COMPILE_OPTIONS
    "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
  )
  set_property(SOURCE
    ${PROJECT_SOURCE_DIR}/ortools/util/fp_utils.cc
    APPEND PROPERTY COMPILE_OPTIONS
    "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>>:-frounding-math>"
  )
  set_property(SOURCE
    ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_expr.cc
    ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_solver.cc
    APPEND PROPERTY COMPILE_OPTIONS
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
    "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
  )
  if(USE_COINOR)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/cbc_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/clp_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(USE_CPLEX)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/cplex_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(USE_GLOP)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glop_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(USE_GLPK)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glpk_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(USE_GUROBI)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/gurobi_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(USE_SCIP)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(USE_XPRESS)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/xpress_interface.cc
      APPEND PROPERTY COMPILE_OPTIONS
      "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>"
      "$<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>"
    )
  endif()
  if(XCODE)
    file(GENERATE
      OUTPUT ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/core_version.cpp
      CONTENT "namespace {char* core_version = \"${PROJECT_VERSION}\";}}")
    target_sources(ortools_core PRIVATE
      ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/core_version.cpp)
  endif()
  target_link_libraries(ortools PUBLIC ortools_core)

  ortools_cxx_binary(
    NAME graph_solve_flow_model
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/solve_flow_model.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flags
      absl::log
      absl::check
      absl::log_globals
      absl::strings
      absl::str_format
      absl::string_view
    NO_INSTALL
  )

  if(NOT EMSCRIPTEN)
    ortools_cxx_binary(
      NAME solve
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/solve.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::check
        absl::log_flags
        absl::statusor
        absl::status
        absl::str_format
        absl::strings
        absl::time
    )
  endif()

  ortools_cxx_binary(
    NAME sat_lrat_checker_main
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_checker_main.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flat_hash_map
      absl::flags
      absl::log
      absl::check
      absl::log_flags
      absl::strings
      absl::string_view
      absl::span
    NO_INSTALL
  )

  if(NOT EMSCRIPTEN)
    ortools_cxx_binary(
      NAME sat_runner
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_runner.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        absl::core_headers
        absl::flags
        absl::log
        absl::check
        absl::log_flags
        absl::strings
        absl::str_format
        absl::synchronization
        absl::span
        protobuf::libprotobuf
    )
  endif()

  if(BUILD_CXX_SAMPLES)
    ortools_cxx_binary(
      NAME glop_samples_simple_glop_program_cc
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/samples/simple_glop_program.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_root_a_tree
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/root_a_tree.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_rooted_tree_paths
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/rooted_tree_paths.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dijkstra_directed
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_directed.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dijkstra_undirected
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_undirected.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dijkstra_one_to_all
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_one_to_all.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dijkstra_sequential
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_sequential.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dijkstra_all_pairs_shortest_paths
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_all_pairs_shortest_paths.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::algorithm_container
        absl::flags
        absl::check
        absl::random_random
        absl::time
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_simple_shortest_path
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_simple_shortest_path.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_shortest_path_one_to_all
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_shortest_path_one_to_all.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_shortest_path_sequential
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_shortest_path_sequential.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_simple_multiple_shortest_paths
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_simple_multiple_shortest_paths.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_multiple_shortest_paths_one_to_all
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_multiple_shortest_paths_one_to_all.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_multiple_shortest_paths_sequential
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_multiple_shortest_paths_sequential.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_simple_constrained_shortest_path
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_simple_constrained_shortest_path.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_dag_constrained_shortest_path_sequential
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_constrained_shortest_path_sequential.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_bfs_directed
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/bfs_directed.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_bfs_undirected
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/bfs_undirected.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_samples_bfs_one_to_all
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/bfs_one_to_all.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )

    if(USE_PDLP)
      ortools_cxx_binary(
        NAME pdlp_samples_simple_pdlp_program_cc
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/samples/simple_pdlp_program.cc
        LINK_LIBRARIES
          ortools::ortools
          Eigen3::Eigen
        NO_INSTALL
      )
    endif()

    ortools_cxx_binary(
      NAME set_cover_samples_set_cover_cc
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/samples/set_cover.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::log
        absl::log_globals
      NO_INSTALL
    )
  endif()

  if(BUILD_CXX_TESTING)
    ortools_cxx_library(
      NAME base_gmock
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/gmock.h
      LINK_LIBRARIES
        absl::status_matchers
        GTest::gtest
        GTest::gmock
        protobuf-matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME base_parse_test_proto
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/parse_test_proto.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        protobuf::libprotobuf
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME glop_glop_testing
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/glop/glop_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/glop/glop_testing.h
      LINK_LIBRARIES ortools::ortools
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME graph_random_graph
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/graph/random_graph.cc
        ${PROJECT_SOURCE_DIR}/ortools/graph/random_graph.h
      LINK_LIBRARIES
        ortools::ortools
        absl::flat_hash_set
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME lp_data_lp_types_testing
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_types_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_types_testing.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME glop_preprocessor_testing
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/glop/preprocessor_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/glop/preprocessor_testing.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        ortools::lp_data_lp_types_testing
      TESTING
      NO_INSTALL
    )

    if(USE_PDLP)
      ortools_cxx_library(
        NAME pdlp_test_util
        TYPE STATIC
        SOURCES
          ${PROJECT_SOURCE_DIR}/ortools/pdlp/test_util.cc
          ${PROJECT_SOURCE_DIR}/ortools/pdlp/test_util.h
        LINK_LIBRARIES
          ortools::ortools
          ortools::base_gmock
          absl::flags
          absl::check
          absl::span
          Eigen3::Eigen
        TESTING
        NO_INSTALL
      )
    endif()

    ortools_cxx_library(
      NAME port_scoped_std_stream_capture
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/port/scoped_std_stream_capture.cc
        ${PROJECT_SOURCE_DIR}/ortools/port/scoped_std_stream_capture.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME sat_2d_orthogonal_packing_testing
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing_testing.h
      LINK_LIBRARIES
        ortools::ortools
        absl::check
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME util_fp_roundtrip_conv_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fp_roundtrip_conv_testing.h
      LINK_LIBRARIES
        ortools::ortools
        absl::string_view
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME util_fp_utils_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fp_utils_testing.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::str_format
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME lp_data_lp_test_utils
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_test_utils.cc
        ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_test_utils.h
      LOCAL_COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        ortools::util_fp_utils_testing
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME util_functions_swig_test_helpers
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/functions_swig_test_helpers.h
      LINK_LIBRARIES ortools::ortools
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_binary_search_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/binary_search_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::int128
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_radix_sort_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/radix_sort_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::log
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_radix_sort_release_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/radix_sort_release_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_duplicate_remover_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/duplicate_remover_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::flat_hash_set
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_space_saving_most_frequent_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/space_saving_most_frequent_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::nullability
        absl::hash
        absl::check
        absl::random_random
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_float_ordinal_bijection_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/float_ordinal_bijection_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME algorithms_auto_radix_sort_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/auto_radix_sort_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_random
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME base_constant_divisor_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/constant_divisor_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME glop_update_row_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/update_row_benchmark.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
        ortools::lp_data_lp_test_utils
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_graph_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/graph_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::check
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_strongly_connected_components_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/strongly_connected_components_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_topologicalsorter_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/topologicalsorter_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::flat_hash_set
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_connected_components_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/connected_components_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_flow_graph_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/flow_graph_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_bounded_dijkstra_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/bounded_dijkstra_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_random
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_cliques_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/cliques_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::flat_hash_set
        absl::bind_front
        absl::log
        absl::check
        absl::random_distributions
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_christofides_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/christofides_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_eulerian_path_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/eulerian_path_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_minimum_spanning_tree_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_spanning_tree_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_k_shortest_paths_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/k_shortest_paths_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_generic_max_flow_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/generic_max_flow_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_min_cost_flow_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/min_cost_flow_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::log
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_assignment_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/assignment_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_linear_assignment_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/linear_assignment_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_dag_constrained_shortest_path_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/dag_constrained_shortest_path_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::check
        absl::random_random
        absl::random_distributions
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_rooted_tree_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/rooted_tree_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::check
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_minimum_vertex_cover_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_vertex_cover_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME graph_dag_shortest_path_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/dag_shortest_path_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::algorithm_container
        absl::check
        absl::random_random
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME linear_solver_proto_solver_preprocessor_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/preprocessor_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME lp_data_sparse_vector_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_vector_benchmark.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_bit_gen_ref
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME lp_data_sparse_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_benchmark.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        ortools::lp_data_lp_test_utils
        benchmark::benchmark_main
        absl::random_bit_gen_ref
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME lp_data_lp_utils_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_utils_benchmark.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME lp_data_triangular_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/triangular_benchmark.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::ortools
        ortools::lp_data_lp_test_utils
        benchmark::benchmark_main
        absl::random_bit_gen_ref
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME sat_integer_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/integer_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME sat_util_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/util_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME sat_2d_packing_brute_force_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/2d_packing_brute_force_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME sat_2d_orthogonal_packing_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        ortools::sat_2d_orthogonal_packing_testing
        benchmark::benchmark_main
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME sat_diffn_util_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_util_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        ortools::sat_2d_orthogonal_packing_testing
        benchmark::benchmark_main
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME set_cover_base_types_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/base_types_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME set_cover_knights_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/knights_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_dense_set_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/dense_set_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::flat_hash_set
        absl::random_random
        absl::random_bit_gen_ref
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_flat_matrix_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/flat_matrix_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::base
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_time_limit_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/time_limit_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::strings
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_saturated_arithmetic_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/saturated_arithmetic_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::log
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_random_subset_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/random_subset_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_full_precision_inequalities_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/full_precision_inequalities_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_bitset_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/bitset_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::check
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_sorted_interval_list_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/sorted_interval_list_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::log
        absl::check
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_stats_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/stats_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_range_minimum_query_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/range_minimum_query_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::fixed_array
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_sort_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/sort_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_bit_gen_ref
        absl::random_distributions
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_integer_pq_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/integer_pq_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::random_random
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_fp_roundtrip_conv_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fp_roundtrip_conv_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
        absl::no_destructor
        absl::check
        absl::random_random
        absl::strings
        absl::str_format
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_scheduling_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/scheduling_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_test(
      NAME algorithms_binary_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/binary_search_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::hash
        absl::int128
        absl::random_random
        absl::random_distributions
        absl::str_format
        absl::string_view
        absl::time
    )

    ortools_cxx_test(
      NAME algorithms_radix_sort_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/radix_sort_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::log
        absl::bits
        absl::int128
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME algorithms_duplicate_remover_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/duplicate_remover_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::random_random
        absl::random_distributions
        absl::span
        protobuf::libprotobuf
      DISABLED
    )

    ortools_cxx_test(
      NAME algorithms_hungarian_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/hungarian_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::flat_hash_map
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME algorithms_adjustable_k_ary_heap_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/adjustable_k_ary_heap_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME algorithms_adjustable_k_ary_heap_stress_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/adjustable_k_ary_heap_stress_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::log
        absl::check
        absl::random_random
      DISABLED
    )

    ortools_cxx_test(
      NAME algorithms_knapsack_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/knapsack_solver_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::span
    )

    ortools_cxx_test(
      NAME algorithms_dense_doubly_linked_list_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/dense_doubly_linked_list_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME algorithms_dynamic_partition_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/dynamic_partition_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::random_bit_gen_ref
        absl::span
    )

    ortools_cxx_test(
      NAME algorithms_space_saving_most_frequent_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/space_saving_most_frequent_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::nullability
        absl::hash
        absl::check
        absl::random_random
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME algorithms_sparse_permutation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/sparse_permutation_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME algorithms_find_graph_symmetries_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/find_graph_symmetries_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::flat_hash_set
        absl::log
        absl::check
        absl::bits
        absl::int128
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::statusor
        absl::strings
        absl::str_format
        absl::string_view
        absl::time
        absl::span
        ortools::graph_random_graph
    )

    ortools_cxx_test(
      NAME algorithms_binary_indexed_tree_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/binary_indexed_tree_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME algorithms_dynamic_permutation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/dynamic_permutation_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME algorithms_float_ordinal_bijection_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/float_ordinal_bijection_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME algorithms_n_choose_k_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/n_choose_k_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::int128
        absl::random_random
        absl::random_distributions
        absl::status
        absl::statusor
    )

    ortools_cxx_test(
      NAME algorithms_auto_radix_sort_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/auto_radix_sort_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::absl_log
        absl::bits
        absl::random_random
        absl::time
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME base_accurate_sum_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/accurate_sum_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME base_constant_divisor_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/constant_divisor_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME base_dump_vars_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/dump_vars_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::int128
    )

    ortools_cxx_test(
      NAME base_status_builder_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/status_builder_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::status_builder
    )

    ortools_cxx_test(
      NAME base_strong_int_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/strong_int_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::node_hash_map
        absl::flags_marshalling
        absl::hash_testing
        absl::log
        absl::int128
        absl::strings
        absl::str_format
    )

    ortools_cxx_test(
      NAME base_gzipfile_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/gzipfile_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        ZLIB::ZLIB
    )

    ortools_cxx_test(
      NAME base_bzip2file_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/base/bzip2file_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        BZip2::BZip2
    )

    ortools_cxx_test(
      NAME bop_bop_solution_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/bop/bop_solution_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME bop_lp_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/bop/lp_utils_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::str_format
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME bop_boolean_problem_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/bop/boolean_problem_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME glop_rank_one_update_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/rank_one_update_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        ortools::lp_data_lp_test_utils
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_pricing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/pricing_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME glop_basis_representation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/basis_representation_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        ortools::lp_data_lp_types_testing
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_variables_info_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/variables_info_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME glop_parameters_validation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/parameters_validation_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME glop_reduced_costs_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/reduced_costs_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME glop_preprocessor_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/preprocessor_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::glop_preprocessor_testing
        GTest::gtest_main
        absl::random_distributions
        absl::strings
        absl::str_format
        ortools::lp_data_lp_test_utils
    )

    ortools_cxx_test(
      NAME glop_preprocessor_random_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/preprocessor_random_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::overload
        absl::log
        absl::random_random
        absl::random_bit_gen_ref
        ortools::lp_data_lp_types_testing
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_markowitz_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/markowitz_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_distributions
        ortools::lp_data_lp_test_utils
    )

    ortools_cxx_test(
      NAME glop_dual_edge_norms_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/dual_edge_norms_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        ortools::lp_data_lp_test_utils
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_revised_simplex_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/revised_simplex_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::glop_glop_testing
        ortools::glop_preprocessor_testing
        GTest::gtest_main
        absl::cleanup
        absl::flags
        absl::log_flags
        absl::strings
        absl::str_format
        absl::span
        ortools::lp_data_lp_types_testing
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_revised_simplex_test_coverage
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/revised_simplex_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::glop_glop_testing
        ortools::glop_preprocessor_testing
        GTest::gtest_main
        absl::cleanup
        absl::flags
        absl::log_flags
        absl::strings
        absl::str_format
        absl::span
        ortools::lp_data_lp_types_testing
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_revised_simplex_random_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/revised_simplex_random_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::glop_preprocessor_testing
        GTest::gtest_main
        absl::algorithm_container
        absl::no_destructor
        absl::flags
        absl::log
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::str_format
        absl::time
        ortools::lp_data_lp_types_testing
    )

    ortools_cxx_test(
      NAME glop_initial_basis_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/initial_basis_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME glop_variable_values_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/variable_values_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME glop_update_row_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/update_row_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        ortools::lp_data_lp_test_utils
    )

    ortools_cxx_test(
      NAME glop_lp_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/lp_solver_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::glop_glop_testing
        GTest::gtest_main
        absl::cleanup
        absl::flags
        absl::log
        absl::check
        absl::log_flags
        absl::strings
        absl::time
        absl::internal_optional_ref
        ortools::lp_data_lp_test_utils
        ortools::lp_data_lp_types_testing
        ortools::port_scoped_std_stream_capture
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME glop_lu_factorization_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/glop/lu_factorization_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_bit_gen_ref
        ortools::lp_data_lp_test_utils
        ortools::lp_data_lp_types_testing
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME graph_graph_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/graph_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::check
        absl::random_random
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME graph_generic_graph_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/generic_graph_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME graph_graph_generator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/graph_generator_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME graph_random_graph_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/random_graph_test.cc
      LINK_LIBRARIES
        ortools::graph_random_graph
        GTest::gtest_main
        absl::random_random
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME graph_io_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/io_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::status
        absl::statusor
        absl::strings
        absl::str_format
    )

    ortools_cxx_test(
      NAME graph_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/util_test.cc
      LINK_LIBRARIES
        ortools::graph_random_graph
        GTest::gtest_main
        absl::check
        absl::random_random
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME graph_test_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/test_util_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME graph_iterators_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/iterators_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME graph_bfs_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/bfs_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::inlined_vector
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::str_format
    )

    ortools_cxx_test(
      NAME graph_strongly_connected_components_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/strongly_connected_components_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME graph_topologicalsorter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/topologicalsorter_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::flat_hash_set
        absl::log
        absl::check
        absl::log_globals
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::status
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME graph_hash_or_tree_container_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/hash_or_tree_container_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::btree
        absl::flat_hash_map
        absl::flat_hash_set
        absl::hash
    )

    ortools_cxx_test(
      NAME graph_index_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/index_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::hash
        absl::random_random
        absl::random_distributions
        absl::string_view
    )

    ortools_cxx_test(
      NAME graph_connected_components_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/connected_components_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::btree
        absl::flat_hash_set
        absl::inlined_vector
        absl::hash
        absl::random_random
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME graph_flow_graph_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/flow_graph_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::log
        absl::random_random
    )

    ortools_cxx_test(
      NAME graph_bounded_dijkstra_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/bounded_dijkstra_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME graph_multi_dijkstra_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/multi_dijkstra_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::flat_hash_set
        absl::check
        absl::random_distributions
        ortools::graph_random_graph
    )

    ortools_cxx_test(
      NAME graph_bidirectional_dijkstra_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/bidirectional_dijkstra_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_distributions
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME graph_cliques_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/cliques_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::flags
        absl::bind_front
        absl::log
        absl::check
        absl::random_distributions
        absl::strings
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME graph_hamiltonian_path_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/hamiltonian_path_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::log
        absl::check
        absl::random_distributions
        absl::str_format
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME graph_christofides_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/christofides_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::status
        absl::str_format
        absl::span
    )

    ortools_cxx_test(
      NAME graph_eulerian_path_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/eulerian_path_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
    )

    ortools_cxx_test(
      NAME graph_minimum_spanning_tree_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_spanning_tree_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::span
    )

    ortools_cxx_test(
      NAME graph_shortest_paths_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/shortest_paths_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::check
        absl::random_random
      DISABLED
    )

    ortools_cxx_test(
      NAME graph_k_shortest_paths_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/k_shortest_paths_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::check
        absl::random_distributions
        absl::strings
    )

    ortools_cxx_test(
      NAME graph_max_flow_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/max_flow_test.cc
      COMPILE_DEFINITIONS "ROOT_DIR=\"${PROJECT_SOURCE_DIR}/\""
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME graph_generic_max_flow_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/generic_max_flow_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::random_bit_gen_ref
        absl::str_format
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME graph_min_cost_flow_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/min_cost_flow_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_distributions
        absl::str_format
        absl::span
    )

    ortools_cxx_test(
      NAME graph_assignment_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/assignment_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME graph_linear_assignment_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/linear_assignment_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME graph_linear_assignment_precision_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/linear_assignment_test.cc
      COMPILE_DEFINITIONS LARGE
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::check
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME graph_dag_connectivity_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/dag_connectivity_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME graph_perfect_matching_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/perfect_matching_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME graph_rooted_tree_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/rooted_tree_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::status
    )

    ortools_cxx_test(
      NAME graph_minimum_vertex_cover_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/minimum_vertex_cover_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
    )

    ortools_cxx_test(
      NAME graph_dag_shortest_path_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/dag_shortest_path_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::check
        absl::random_random
        absl::status
        absl::span
    )

    if(USE_XPRESS)
      ortools_cxx_test(
        NAME linear_solver_xpress_interface_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/xpress_interface_test.cc
        COMPILE_OPTIONS
          $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>
          $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
        LINK_LIBRARIES
          absl::log
          absl::log_globals
          absl::random_random
          absl::strings
      )
    endif()

    if(USE_SCIP)
      ortools_cxx_test(
        NAME linear_solver_scip_callback_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_callback_test.cc
        LINK_LIBRARIES GTest::gtest_main
      )
    endif()

    ortools_cxx_test(
      NAME linear_solver_glop_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/glop_utils_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    if(USE_SCIP)
      ortools_cxx_test(
        NAME linear_solver_scip_helper_macros_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/scip_helper_macros_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          ortools::port_scoped_std_stream_capture
          $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      )
    endif()

    if(USE_GLOP)
      ortools_cxx_test(
        NAME linear_solver_linear_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/linear_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          absl::time
      )
    endif()

    if(USE_PDLP)
      ortools_cxx_test(
        NAME linear_solver_proto_solver_pdlp_proto_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/pdlp_proto_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          absl::statusor
          absl::strings
      )
    endif()

    if(USE_HIGHS)
      ortools_cxx_test(
        NAME linear_solver_proto_solver_highs_proto_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/highs_proto_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::strings
          absl::str_format
      )
    endif()

    if(USE_GLOP)
      ortools_cxx_test(
        NAME linear_solver_proto_solver_glop_proto_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/glop_proto_solver_test.cc
        LINK_LIBRARIES GTest::gtest_main
      )
    endif()

    if(USE_SCIP)
      ortools_cxx_test(
        NAME linear_solver_proto_solver_scip_params_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/scip_params_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::check
          absl::status
          absl::strings
          $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      )
    endif()

    ortools_cxx_test(
      NAME linear_solver_proto_solver_preprocessor_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/proto_solver/preprocessor_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        absl::string_view
        absl::span
        ortools::glop_preprocessor_testing
        ortools::lp_data_lp_test_utils
    )

    ortools_cxx_test(
      NAME lp_data_lp_types_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_types_test.cc
      LINK_LIBRARIES
        ortools::lp_data_lp_types_testing
        GTest::gtest_main
        absl::core_headers
        absl::strings
    )

    ortools_cxx_test(
      NAME lp_data_permutation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/permutation_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME lp_data_sparse_vector_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_vector_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME lp_data_sparse_column_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_column_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME lp_data_sparse_row_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_row_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME lp_data_sparse_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sparse_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::lp_data_lp_test_utils
        GTest::gtest_main
        absl::log
        absl::random_random
        absl::random_bit_gen_ref
    )

    ortools_cxx_test(
      NAME lp_data_matrix_scaler_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_scaler_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::random_random
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME lp_data_lp_data_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::strings
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME lp_data_lp_data_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_data_utils_test.cc
      LINK_LIBRARIES
        ortools::lp_data_lp_test_utils
        ortools::lp_data_lp_types_testing
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME lp_data_lp_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_utils_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_distributions
        absl::span
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME lp_data_matrix_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/matrix_utils_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::lp_data_lp_test_utils
        GTest::gtest_main
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME lp_data_lp_parser_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_parser_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::random_random
        absl::statusor
        absl::strings
    )

    ortools_cxx_test(
      NAME lp_data_lp_print_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_print_utils_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME lp_data_proto_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/proto_utils_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME lp_data_mps_reader_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/mps_reader_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        ortools::lp_data_lp_types_testing
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flags
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::status
        absl::strings
        absl::str_format
        ortools::util_fp_utils_testing
        re2::re2
    )

    ortools_cxx_test(
      NAME lp_data_sol_reader_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/sol_reader_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME lp_data_lp_decomposer_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/lp_data/lp_decomposer_test.cc
      COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::strings
        absl::span
    )

    if(USE_PDLP)
      ortools_cxx_test(
        NAME pdlp_scheduler_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/scheduler_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::log
      )

      ortools_cxx_test(
        NAME pdlp_iteration_stats_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/iteration_stats_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_primal_dual_hybrid_gradient_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/primal_dual_hybrid_gradient_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          absl::flat_hash_map
          absl::log
          absl::check
          absl::statusor
          absl::strings
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_quadratic_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/quadratic_program_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          absl::status
          absl::statusor
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_sharded_optimization_utils_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharded_optimization_utils_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_sharded_quadratic_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharded_quadratic_program_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_sharder_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/sharder_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::log
          absl::random_distributions
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_solvers_proto_validation_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/solvers_proto_validation_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          absl::strings
      )

      ortools_cxx_test(
        NAME pdlp_termination_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/termination_test.cc
        LINK_LIBRARIES GTest::gtest_main
      )

      ortools_cxx_test(
        NAME pdlp_test_util_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/test_util_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          absl::check
          absl::span
          Eigen3::Eigen
      )

      ortools_cxx_test(
        NAME pdlp_trust_region_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/trust_region_test.cc
        LINK_LIBRARIES
          ortools::pdlp_test_util
          GTest::gtest_main
          absl::core_headers
          absl::strings
          Eigen3::Eigen
      )
    endif()

    ortools_cxx_test(
      NAME port_sysinfo_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/port/sysinfo_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME port_scoped_std_stream_capture_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/port/scoped_std_stream_capture_test.cc
      LINK_LIBRARIES
        ortools::port_scoped_std_stream_capture
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_cp_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::log
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/model_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_scheduling_local_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_local_search_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cp_model_copy_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_copy_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_primary_variables_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/primary_variables_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_container_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/container_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_gate_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/gate_utils_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME sat_sat_sweeping_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_sweeping_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_presolve_encoding_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_encoding_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_map
        absl::check
    )

    ortools_cxx_test(
      NAME sat_cp_model_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_utils_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_set
        absl::str_format
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME sat_synchronization_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/synchronization_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cp_model_checker_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_checker_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_constraint_violation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/constraint_violation_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_feasibility_jump_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/feasibility_jump_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_linear_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/linear_model_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_parameters_validation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/parameters_validation_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME sat_cp_model_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_search_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_map
        absl::log
        absl::strings
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_cp_model_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_solver_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_set
        absl::log
        absl::strings
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_presolve_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_util_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_set
        absl::log
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME sat_presolve_context_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/presolve_context_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cp_model_table_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_table_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::inlined_vector
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cp_constraint_presolve_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraint_presolve_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME sat_cp_model_presolve_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_presolve_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_cp_model_presolve_random_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_presolve_random_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags
        absl::log
        absl::check
        absl::random_random
        absl::strings
        absl::str_format
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_cp_model_postsolve_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_postsolve_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_cp_model_expand_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_expand_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::btree
        absl::log
        absl::check
        absl::random_random
        absl::strings
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_sat_literal_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_literal_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_sat_assignment_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_assignment_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_sat_clause_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_clause_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_restart_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/restart_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
    )

    ortools_cxx_test(
      NAME sat_probing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/probing_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_sat_inprocessing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_inprocessing_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_sat_decision_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_decision_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME sat_clause_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/clause_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME sat_pb_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/pb_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_symmetry_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_symmetry_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/symmetry_util_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_var_domination_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/var_domination_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_integer_base_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/integer_base_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_random
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME sat_integer_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/integer_search_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::log
        absl::check
    )

    ortools_cxx_test(
      NAME sat_lb_tree_search_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/lb_tree_search_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_pseudo_costs_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/pseudo_costs_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_intervals_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/intervals_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_scheduling_helpers_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_helpers_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_precedences_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/precedences_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_map
        absl::span
    )

    ortools_cxx_test(
      NAME sat_integer_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/integer_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_integer_expr_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/integer_expr_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::btree
        absl::log
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::strings
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_linear_propagation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/linear_propagation_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::check
        absl::random_random
        absl::random_distributions
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_all_different_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/all_different_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_disjunctive_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/disjunctive_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME sat_timetable_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/timetable_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::btree
        absl::log
        absl::check
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cumulative_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_random
        absl::strings
    )

    ortools_cxx_test(
      NAME sat_cumulative_energy_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cumulative_energy_test.cc
      LINK_LIBRARIES
        ortools::sat_2d_orthogonal_packing_testing
        GTest::gtest_main
        absl::log
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME sat_linear_relaxation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/linear_relaxation_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_linear_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/linear_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_linear_programming_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/linear_programming_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME sat_linear_constraint_manager_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/linear_constraint_manager_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_cuts_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cuts_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::int128
        absl::strings
        absl::span
        ortools::util_fp_utils_testing
    )

    ortools_cxx_test(
      NAME sat_routing_cuts_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/routing_cuts_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_map
        absl::log
        absl::random_random
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME sat_scheduling_cuts_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/scheduling_cuts_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_zero_half_cuts_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/zero_half_cuts_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_optimization_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/optimization_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/util_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::btree
        absl::flat_hash_set
        absl::log
        absl::check
        absl::bits
        absl::int128
        absl::random_random
        absl::strings
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_table_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/table_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::btree
        absl::log
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cp_constraints_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_constraints_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_enforcement_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/enforcement_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::span
    )

    ortools_cxx_test(
      NAME sat_2d_packing_brute_force_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/2d_packing_brute_force_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME sat_2d_rectangle_presolve_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/2d_rectangle_presolve_test.cc
      LINK_LIBRARIES
        ortools::sat_2d_orthogonal_packing_testing
        GTest::gtest_main
        absl::algorithm_container
        absl::flat_hash_map
        absl::flat_hash_set
        absl::log
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME sat_2d_orthogonal_packing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/2d_orthogonal_packing_test.cc
      LINK_LIBRARIES
        ortools::sat_2d_orthogonal_packing_testing
        GTest::gtest_main
        absl::check
        absl::random_random
        absl::random_bit_gen_ref
        absl::random_distributions
        absl::strings
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_2d_try_edge_propagator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/2d_try_edge_propagator_test.cc
      LINK_LIBRARIES
        ortools::sat_2d_orthogonal_packing_testing
        GTest::gtest_main
        absl::algorithm_container
        absl::random_random
        absl::strings
        absl::span
    )

    if(UNIX AND NOT APPLE)
      ortools_cxx_test(
        NAME sat_diffn_util_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_util_test.cc
        LINK_LIBRARIES
          ortools::sat_2d_orthogonal_packing_testing
          GTest::gtest_main
          absl::algorithm_container
          absl::flat_hash_map
          absl::log
          absl::check
          absl::random_random
          absl::random_bit_gen_ref
          absl::random_distributions
          absl::strings
          absl::span
        DISABLED
      )
    endif()

    ortools_cxx_test(
      NAME sat_diffn_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/diffn_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::strings
    )

    ortools_cxx_test(
      NAME sat_circuit_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/circuit_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::btree
        absl::check
        absl::random_random
        absl::string_view
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_encoding_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/encoding_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME sat_cp_model_lns_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_lns_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::flat_hash_set
        absl::check
        absl::random_random
        absl::strings
        absl::synchronization
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME sat_feasibility_pump_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/feasibility_pump_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_rins_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/rins_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME sat_subsolver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/subsolver_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::synchronization
    )

    ortools_cxx_test(
      NAME sat_drat_checker_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/drat_checker_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_lrat_proof_handler_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_proof_handler_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME sat_lrat_checker_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/lrat_checker_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME sat_opb_reader_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/opb_reader_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::strings
    )

    ortools_cxx_test(
      NAME sat_recordio_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/recordio_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_sat_cnf_reader_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/sat_cnf_reader_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::strings
        absl::string_view
        absl::span
    )

    ortools_cxx_test(
      NAME sat_cp_model_symmetries_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/cp_model_symmetries_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::check
        absl::strings
    )

    ortools_cxx_test(
      NAME sat_implied_bounds_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/implied_bounds_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::span
    )

    ortools_cxx_test(
      NAME sat_combine_solutions_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/combine_solutions_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME sat_work_assignment_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/work_assignment_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::string_view
        absl::span
    )

    ortools_cxx_test(
      NAME sat_diophantine_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/diophantine_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::int128
        absl::random_random
        absl::strings
        absl::str_format
    )

    ortools_cxx_test(
      NAME sat_inclusion_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/inclusion_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME sat_flaky_models_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/flaky_models_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME set_cover_base_types_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/base_types_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
        absl::span
    )

    ortools_cxx_test(
      NAME set_cover_set_cover_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_model_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::random_random
    )

    ortools_cxx_test(
      NAME set_cover_set_cover_invariant_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_invariant_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME set_cover_set_cover_heuristics_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_heuristics_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME set_cover_assignment_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/assignment_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
    )

    ortools_cxx_test(
      NAME set_cover_set_cover_cft_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_cft_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::strings
        absl::time
      DISABLED
    )

    ortools_cxx_test(
      NAME set_cover_capacity_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/capacity_model_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME set_cover_capacity_invariant_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/capacity_invariant_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    if(USE_GLPK)
      ortools_cxx_test(
        NAME third_party_solvers_glpk_glpk_formatters_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/third_party_solvers/glpk/glpk_formatters_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::strings
      )
    endif()

    ortools_cxx_test(
      NAME util_file_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/file_util_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::status
        absl::strings
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME util_dense_set_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/dense_set_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::random_random
        absl::random_bit_gen_ref
    )

    ortools_cxx_test(
      NAME util_solve_interrupter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/solve_interrupter_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME util_fp_roundtrip_conv_testing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fp_roundtrip_conv_testing_test.cc
      LINK_LIBRARIES
        ortools::util_fp_roundtrip_conv_testing
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_flat_matrix_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/flat_matrix_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::base
        absl::check
    )

    ortools_cxx_test(
      NAME util_status_macros_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/status_macros_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::status_macros
        absl::statusor
    )

    ortools_cxx_test(
      NAME util_logging_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/logging_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::strings
        ortools::port_scoped_std_stream_capture
    )

    ortools_cxx_test(
      NAME util_lazy_mutable_copy_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/lazy_mutable_copy_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME util_adaptative_parameter_value_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/adaptative_parameter_value_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_random
    )

    ortools_cxx_test(
      NAME util_random_engine_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/random_engine_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_rev_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/rev_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::btree
        absl::flat_hash_map
        absl::random_distributions
    )

    ortools_cxx_test(
      NAME util_range_query_function_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/range_query_function_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
    )

    ortools_cxx_test(
      NAME util_running_stat_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/running_stat_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_fp_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fp_utils_test.cc
      LINK_LIBRARIES
        ortools::util_fp_utils_testing
        GTest::gtest_main
        absl::base
        absl::check
        absl::random_random
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME util_time_limit_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/time_limit_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::flags
        absl::log_streamer
        absl::random_random
        absl::strings
        absl::str_format
        absl::time
      DISABLED
    )

    ortools_cxx_test(
      NAME util_tuple_set_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/tuple_set_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_rational_approximation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/rational_approximation_test.cc
      LINK_LIBRARIES
        ortools::util_fp_utils_testing
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_saturated_arithmetic_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/saturated_arithmetic_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::int128
        absl::random_random
    )

    ortools_cxx_test(
      NAME util_permutation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/permutation_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_cached_log_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/cached_log_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_affine_relation_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/affine_relation_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_status_streaming_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/status_streaming_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::scoped_mock_log
        absl::status
        absl::source_location
    )

    ortools_cxx_test(
      NAME util_random_subset_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/random_subset_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::btree
        absl::flat_hash_map
        absl::flat_hash_set
        absl::int128
        absl::random_random
        absl::random_distributions
        absl::str_format
        absl::time
      DISABLED
    )

    ortools_cxx_test(
      NAME util_strong_index_map_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/strong_index_map_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::string_view
    )

    ortools_cxx_test(
      NAME util_mp_model_solution_checker_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/mp_model_solution_checker_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME util_full_precision_inequalities_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/full_precision_inequalities_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
      DISABLED
    )

    ortools_cxx_test(
      NAME util_filelineiter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/filelineiter_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::strings
        absl::str_format
    )

    ortools_cxx_test(
      NAME util_bitset_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/bitset_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::flat_hash_set
        absl::check
        absl::random_random
        absl::strings
        absl::str_format
    )

    ortools_cxx_test(
      NAME util_sorted_interval_list_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/sorted_interval_list_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::int128
        absl::random_distributions
        absl::span
    )

    ortools_cxx_test(
      NAME util_stats_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/stats_test.cc
      LINK_LIBRARIES
        ortools::util_fp_utils_testing
        GTest::gtest_main
        absl::random_random
        absl::random_distributions
        absl::time
    )

    ortools_cxx_test(
      NAME util_monoid_operation_tree_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/monoid_operation_tree_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::str_format
    )

    ortools_cxx_test(
      NAME util_range_minimum_query_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/range_minimum_query_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::strings
    )

    ortools_cxx_test(
      NAME util_qap_reader_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/qap_reader_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_sort_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/sort_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::random_bit_gen_ref
        absl::random_distributions
      DISABLED
    )

    ortools_cxx_test(
      NAME util_integer_pq_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/integer_pq_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::random_random
    )

    ortools_cxx_test(
      NAME util_fp_roundtrip_conv_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fp_roundtrip_conv_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::base
        absl::no_destructor
        absl::log
        absl::check
        absl::random_random
        absl::status
        absl::statusor
        absl::strings
        absl::str_format
        absl::time
    )

    ortools_cxx_test(
      NAME util_parse_proto_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/parse_proto_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME util_fixed_shape_binary_tree_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/fixed_shape_binary_tree_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::bits
      DISABLED
    )

    ortools_cxx_test(
      NAME util_scheduling_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/scheduling_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME util_recycled_var_guard_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/recycled_var_guard_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
    )

    if(BUILD_CXX_SAMPLES)
      ortools_cxx_test(
        NAME algorithms_samples_simple_knapsack_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/samples/simple_knapsack_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME algorithms_samples_knapsack_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/samples/knapsack.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_bintest(
        NAME glop_samples_simple_glop_program_cc_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/glop/samples/simple_glop_program_cc_test.bintest
        ENVIRONMENT "BINTEST_simple_glop_program_cc=$<TARGET_FILE:ortools::glop_samples_simple_glop_program_cc>"
      )

      ortools_cxx_test(
        NAME graph_samples_assignment_linear_sum_assignment_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/assignment_linear_sum_assignment.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME graph_samples_assignment_min_flow_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/assignment_min_flow.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME graph_samples_balance_min_flow_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/balance_min_flow.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME graph_samples_simple_max_flow_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/simple_max_flow_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME graph_samples_simple_min_cost_flow_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/samples/simple_min_cost_flow_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_bintest(
        NAME graph_samples_root_a_tree_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/root_a_tree_test.bintest
        ENVIRONMENT "BINTEST_root_a_tree=$<TARGET_FILE:ortools::graph_samples_root_a_tree>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_rooted_tree_paths_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/rooted_tree_paths_test.bintest
        ENVIRONMENT "BINTEST_rooted_tree_paths=$<TARGET_FILE:ortools::graph_samples_rooted_tree_paths>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dijkstra_directed_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_directed_test.bintest
        ENVIRONMENT "BINTEST_dijkstra_directed=$<TARGET_FILE:ortools::graph_samples_dijkstra_directed>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dijkstra_undirected_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_undirected_test.bintest
        ENVIRONMENT "BINTEST_dijkstra_undirected=$<TARGET_FILE:ortools::graph_samples_dijkstra_undirected>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dijkstra_one_to_all_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_one_to_all_test.bintest
        ENVIRONMENT "BINTEST_dijkstra_one_to_all=$<TARGET_FILE:ortools::graph_samples_dijkstra_one_to_all>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dijkstra_sequential_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_sequential_test.bintest
        ENVIRONMENT "BINTEST_dijkstra_sequential=$<TARGET_FILE:ortools::graph_samples_dijkstra_sequential>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dijkstra_all_pairs_shortest_paths_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dijkstra_all_pairs_shortest_paths_test.bintest
        ENVIRONMENT "BINTEST_dijkstra_all_pairs_shortest_paths=$<TARGET_FILE:ortools::graph_samples_dijkstra_all_pairs_shortest_paths>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_simple_shortest_path_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_simple_shortest_path_test.bintest
        ENVIRONMENT "BINTEST_dag_simple_shortest_path=$<TARGET_FILE:ortools::graph_samples_dag_simple_shortest_path>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_shortest_path_one_to_all_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_shortest_path_one_to_all_test.bintest
        ENVIRONMENT "BINTEST_dag_shortest_path_one_to_all=$<TARGET_FILE:ortools::graph_samples_dag_shortest_path_one_to_all>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_shortest_path_sequential_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_shortest_path_sequential_test.bintest
        ENVIRONMENT "BINTEST_dag_shortest_path_sequential=$<TARGET_FILE:ortools::graph_samples_dag_shortest_path_sequential>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_simple_multiple_shortest_paths_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_simple_multiple_shortest_paths_test.bintest
        ENVIRONMENT "BINTEST_dag_simple_multiple_shortest_paths=$<TARGET_FILE:ortools::graph_samples_dag_simple_multiple_shortest_paths>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_multiple_shortest_paths_one_to_all_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_multiple_shortest_paths_one_to_all_test.bintest
        ENVIRONMENT "BINTEST_dag_multiple_shortest_paths_one_to_all=$<TARGET_FILE:ortools::graph_samples_dag_multiple_shortest_paths_one_to_all>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_multiple_shortest_paths_sequential_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_multiple_shortest_paths_sequential_test.bintest
        ENVIRONMENT "BINTEST_dag_multiple_shortest_paths_sequential=$<TARGET_FILE:ortools::graph_samples_dag_multiple_shortest_paths_sequential>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_simple_constrained_shortest_path_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_simple_constrained_shortest_path_test.bintest
        ENVIRONMENT "BINTEST_dag_simple_constrained_shortest_path=$<TARGET_FILE:ortools::graph_samples_dag_simple_constrained_shortest_path>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_dag_constrained_shortest_path_sequential_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/dag_constrained_shortest_path_sequential_test.bintest
        ENVIRONMENT "BINTEST_dag_constrained_shortest_path_sequential=$<TARGET_FILE:ortools::graph_samples_dag_constrained_shortest_path_sequential>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_bfs_directed_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/bfs_directed_test.bintest
        ENVIRONMENT "BINTEST_bfs_directed=$<TARGET_FILE:ortools::graph_samples_bfs_directed>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_bfs_undirected_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/bfs_undirected_test.bintest
        ENVIRONMENT "BINTEST_bfs_undirected=$<TARGET_FILE:ortools::graph_samples_bfs_undirected>"
      )

      ortools_cxx_bintest(
        NAME graph_samples_bfs_one_to_all_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/graph/samples/bfs_one_to_all_test.bintest
        ENVIRONMENT "BINTEST_bfs_one_to_all=$<TARGET_FILE:ortools::graph_samples_bfs_one_to_all>"
      )

      ortools_cxx_test(
        NAME linear_solver_samples_assignment_mip_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_mip.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_assignment_groups_mip_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_groups_mip.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME linear_solver_samples_assignment_task_sizes_mip_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_task_sizes_mip.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME linear_solver_samples_assignment_teams_mip_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_teams_mip.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME linear_solver_samples_basic_example_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/basic_example.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_bin_packing_mip_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/bin_packing_mip.cc
        LINK_LIBRARIES absl::log
      )

      ortools_cxx_test(
        NAME linear_solver_samples_linear_programming_example_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/linear_programming_example.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_integer_programming_example_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/integer_programming_example.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_mip_var_array_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/mip_var_array.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_multiple_knapsack_mip_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/multiple_knapsack_mip.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME linear_solver_samples_simple_lp_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/simple_lp_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_simple_mip_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/simple_mip_program.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME linear_solver_samples_stigler_diet_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/stigler_diet.cc
        LINK_LIBRARIES
          absl::log
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_assignment_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_assignment_groups_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_groups_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_assignment_task_sizes_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_task_sizes_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME sat_samples_assignment_teams_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_teams_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME sat_samples_assumptions_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assumptions_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_binpacking_problem_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/binpacking_problem_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_bool_or_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/bool_or_sample_sat.cc
        LINK_LIBRARIES
          absl::log_globals
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_channeling_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/channeling_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_clone_model_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/clone_model_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_cp_is_fun_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/cp_is_fun_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_cp_sat_example_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/cp_sat_example.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_earliness_tardiness_cost_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/earliness_tardiness_cost_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_interval_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/interval_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_minimal_jobshop_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/minimal_jobshop_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME sat_samples_literal_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/literal_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_multiple_knapsack_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/multiple_knapsack_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME sat_samples_non_linear_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/non_linear_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_nqueens_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/nqueens_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::strings
      )

      ortools_cxx_test(
        NAME sat_samples_nurses_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/nurses_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME sat_samples_optional_interval_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/optional_interval_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_no_overlap_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/no_overlap_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_rabbits_and_pheasants_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/rabbits_and_pheasants_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_ranking_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/ranking_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_reified_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/reified_sample_sat.cc
        LINK_LIBRARIES
          absl::log_globals
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_schedule_requests_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/schedule_requests_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::str_format
      )

      ortools_cxx_test(
        NAME sat_samples_search_for_all_solutions_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/search_for_all_solutions_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_simple_sat_program_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/simple_sat_program.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_solution_hinting_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/solution_hinting_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_solve_and_print_intermediate_solutions_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/solve_and_print_intermediate_solutions_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_step_function_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/step_function_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
          absl::span
      )

      ortools_cxx_test(
        NAME sat_samples_solve_with_time_limit_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/solve_with_time_limit_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_test(
        NAME sat_samples_stop_after_n_solutions_sample_sat_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/samples/stop_after_n_solutions_sample_sat.cc
        LINK_LIBRARIES
          absl::log
          absl::check
          absl::log_globals
      )

      ortools_cxx_bintest(
        NAME set_cover_samples_set_cover_cc_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/set_cover/samples/set_cover_cc_test.bintest
        ENVIRONMENT "BINTEST_set_cover_cc=$<TARGET_FILE:ortools::set_cover_samples_set_cover_cc>"
      )
    endif()
  endif()
endif()
