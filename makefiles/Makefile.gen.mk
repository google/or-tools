BASE_DEPS = \
 $(INC_DIR)/ortools/base/accurate_sum.h \
 $(INC_DIR)/ortools/base/adjustable_priority_queue.h \
 $(INC_DIR)/ortools/base/adjustable_priority_queue-inl.h \
 $(INC_DIR)/ortools/base/base_export.h \
 $(INC_DIR)/ortools/base/basictypes.h \
 $(INC_DIR)/ortools/base/bitmap.h \
 $(INC_DIR)/ortools/base/case.h \
 $(INC_DIR)/ortools/base/cleanup.h \
 $(INC_DIR)/ortools/base/commandlineflags.h \
 $(INC_DIR)/ortools/base/container_logging.h \
 $(INC_DIR)/ortools/base/dynamic_library.h \
 $(INC_DIR)/ortools/base/encodingutils.h \
 $(INC_DIR)/ortools/base/file.h \
 $(INC_DIR)/ortools/base/gzipstring.h \
 $(INC_DIR)/ortools/base/hash.h \
 $(INC_DIR)/ortools/base/helpers.h \
 $(INC_DIR)/ortools/base/init_google.h \
 $(INC_DIR)/ortools/base/integral_types.h \
 $(INC_DIR)/ortools/base/int_type.h \
 $(INC_DIR)/ortools/base/iterator_adaptors.h \
 $(INC_DIR)/ortools/base/jniutil.h \
 $(INC_DIR)/ortools/base/linked_hash_map.h \
 $(INC_DIR)/ortools/base/logging_export.h \
 $(INC_DIR)/ortools/base/logging_flags.h \
 $(INC_DIR)/ortools/base/logging.h \
 $(INC_DIR)/ortools/base/logging_utilities.h \
 $(INC_DIR)/ortools/base/log_severity.h \
 $(INC_DIR)/ortools/base/macros.h \
 $(INC_DIR)/ortools/base/map_util.h \
 $(INC_DIR)/ortools/base/mathutil.h \
 $(INC_DIR)/ortools/base/murmur.h \
 $(INC_DIR)/ortools/base/protobuf_util.h \
 $(INC_DIR)/ortools/base/protoutil.h \
 $(INC_DIR)/ortools/base/ptr_util.h \
 $(INC_DIR)/ortools/base/python-swig.h \
 $(INC_DIR)/ortools/base/raw_logging.h \
 $(INC_DIR)/ortools/base/recordio.h \
 $(INC_DIR)/ortools/base/small_map.h \
 $(INC_DIR)/ortools/base/small_ordered_set.h \
 $(INC_DIR)/ortools/base/source_location.h \
 $(INC_DIR)/ortools/base/status_builder.h \
 $(INC_DIR)/ortools/base/status_macros.h \
 $(INC_DIR)/ortools/base/stl_logging.h \
 $(INC_DIR)/ortools/base/stl_util.h \
 $(INC_DIR)/ortools/base/strong_int.h \
 $(INC_DIR)/ortools/base/strong_vector.h \
 $(INC_DIR)/ortools/base/sysinfo.h \
 $(INC_DIR)/ortools/base/thorough_hash.h \
 $(INC_DIR)/ortools/base/threadpool.h \
 $(INC_DIR)/ortools/base/timer.h \
 $(INC_DIR)/ortools/base/typeid.h \
 $(INC_DIR)/ortools/base/version.h \
 $(INC_DIR)/ortools/base/vlog_is_on.h

PORT_DEPS = \
 $(INC_DIR)/ortools/port/file.h \
 $(INC_DIR)/ortools/port/proto_utils.h \
 $(INC_DIR)/ortools/port/sysinfo.h \
 $(INC_DIR)/ortools/port/utf8.h

UTIL_DEPS = \
 $(INC_DIR)/ortools/util/adaptative_parameter_value.h \
 $(INC_DIR)/ortools/util/affine_relation.h \
 $(INC_DIR)/ortools/util/bitset.h \
 $(INC_DIR)/ortools/util/cached_log.h \
 $(INC_DIR)/ortools/util/filelineiter.h \
 $(INC_DIR)/ortools/util/file_util.h \
 $(INC_DIR)/ortools/util/fp_utils.h \
 $(INC_DIR)/ortools/util/functions_swig_helpers.h \
 $(INC_DIR)/ortools/util/functions_swig_test_helpers.h \
 $(INC_DIR)/ortools/util/graph_export.h \
 $(INC_DIR)/ortools/util/integer_pq.h \
 $(INC_DIR)/ortools/util/lazy_mutable_copy.h \
 $(INC_DIR)/ortools/util/logging.h \
 $(INC_DIR)/ortools/util/monoid_operation_tree.h \
 $(INC_DIR)/ortools/util/permutation.h \
 $(INC_DIR)/ortools/util/piecewise_linear_function.h \
 $(INC_DIR)/ortools/util/proto_tools.h \
 $(INC_DIR)/ortools/util/random_engine.h \
 $(INC_DIR)/ortools/util/range_minimum_query.h \
 $(INC_DIR)/ortools/util/range_query_function.h \
 $(INC_DIR)/ortools/util/rational_approximation.h \
 $(INC_DIR)/ortools/util/return_macros.h \
 $(INC_DIR)/ortools/util/rev.h \
 $(INC_DIR)/ortools/util/running_stat.h \
 $(INC_DIR)/ortools/util/saturated_arithmetic.h \
 $(INC_DIR)/ortools/util/sigint.h \
 $(INC_DIR)/ortools/util/sorted_interval_list.h \
 $(INC_DIR)/ortools/util/sort.h \
 $(INC_DIR)/ortools/util/stats.h \
 $(INC_DIR)/ortools/util/string_array.h \
 $(INC_DIR)/ortools/util/strong_integers.h \
 $(INC_DIR)/ortools/util/testing_utils.h \
 $(INC_DIR)/ortools/util/time_limit.h \
 $(INC_DIR)/ortools/util/tuple_set.h \
 $(INC_DIR)/ortools/util/vector_map.h \
 $(INC_DIR)/ortools/util/vector_or_function.h \
 $(INC_DIR)/ortools/util/zvector.h \
 $(INC_DIR)/ortools/util/optional_boolean.pb.h

INIT_DEPS = \
 $(INC_DIR)/ortools/init/init.h \
 $(INC_DIR)/ortools/sat/cp_model.pb.h \
 $(INC_DIR)/ortools/sat/sat_parameters.pb.h


SCHEDULING_DEPS = \
 $(INC_DIR)/ortools/scheduling/jobshop_scheduling_parser.h \
 $(INC_DIR)/ortools/scheduling/rcpsp_parser.h \
 $(INC_DIR)/ortools/scheduling/jobshop_scheduling.pb.h \
 $(INC_DIR)/ortools/scheduling/rcpsp.pb.h

LP_DATA_DEPS = \
 $(INC_DIR)/ortools/lp_data/lp_data.h \
 $(INC_DIR)/ortools/lp_data/lp_data_utils.h \
 $(INC_DIR)/ortools/lp_data/lp_decomposer.h \
 $(INC_DIR)/ortools/lp_data/lp_parser.h \
 $(INC_DIR)/ortools/lp_data/lp_print_utils.h \
 $(INC_DIR)/ortools/lp_data/lp_types.h \
 $(INC_DIR)/ortools/lp_data/lp_utils.h \
 $(INC_DIR)/ortools/lp_data/matrix_scaler.h \
 $(INC_DIR)/ortools/lp_data/matrix_utils.h \
 $(INC_DIR)/ortools/lp_data/model_reader.h \
 $(INC_DIR)/ortools/lp_data/mps_reader.h \
 $(INC_DIR)/ortools/lp_data/permutation.h \
 $(INC_DIR)/ortools/lp_data/proto_utils.h \
 $(INC_DIR)/ortools/lp_data/scattered_vector.h \
 $(INC_DIR)/ortools/lp_data/sparse_column.h \
 $(INC_DIR)/ortools/lp_data/sparse.h \
 $(INC_DIR)/ortools/lp_data/sparse_row.h \
 $(INC_DIR)/ortools/lp_data/sparse_vector.h

GLOP_DEPS = \
 $(INC_DIR)/ortools/glop/basis_representation.h \
 $(INC_DIR)/ortools/glop/dual_edge_norms.h \
 $(INC_DIR)/ortools/glop/entering_variable.h \
 $(INC_DIR)/ortools/glop/initial_basis.h \
 $(INC_DIR)/ortools/glop/lp_solver.h \
 $(INC_DIR)/ortools/glop/lu_factorization.h \
 $(INC_DIR)/ortools/glop/markowitz.h \
 $(INC_DIR)/ortools/glop/preprocessor.h \
 $(INC_DIR)/ortools/glop/pricing.h \
 $(INC_DIR)/ortools/glop/primal_edge_norms.h \
 $(INC_DIR)/ortools/glop/rank_one_update.h \
 $(INC_DIR)/ortools/glop/reduced_costs.h \
 $(INC_DIR)/ortools/glop/revised_simplex.h \
 $(INC_DIR)/ortools/glop/status.h \
 $(INC_DIR)/ortools/glop/update_row.h \
 $(INC_DIR)/ortools/glop/variables_info.h \
 $(INC_DIR)/ortools/glop/variable_values.h \
 $(INC_DIR)/ortools/glop/parameters.pb.h

GRAPH_DEPS = \
 $(INC_DIR)/ortools/graph/assignment.h \
 $(INC_DIR)/ortools/graph/christofides.h \
 $(INC_DIR)/ortools/graph/cliques.h \
 $(INC_DIR)/ortools/graph/connected_components.h \
 $(INC_DIR)/ortools/graph/ebert_graph.h \
 $(INC_DIR)/ortools/graph/eulerian_path.h \
 $(INC_DIR)/ortools/graph/graph.h \
 $(INC_DIR)/ortools/graph/graphs.h \
 $(INC_DIR)/ortools/graph/hamiltonian_path.h \
 $(INC_DIR)/ortools/graph/io.h \
 $(INC_DIR)/ortools/graph/iterators.h \
 $(INC_DIR)/ortools/graph/linear_assignment.h \
 $(INC_DIR)/ortools/graph/max_flow.h \
 $(INC_DIR)/ortools/graph/min_cost_flow.h \
 $(INC_DIR)/ortools/graph/minimum_spanning_tree.h \
 $(INC_DIR)/ortools/graph/one_tree_lower_bound.h \
 $(INC_DIR)/ortools/graph/perfect_matching.h \
 $(INC_DIR)/ortools/graph/shortestpaths.h \
 $(INC_DIR)/ortools/graph/strongly_connected_components.h \
 $(INC_DIR)/ortools/graph/topologicalsorter.h \
 $(INC_DIR)/ortools/graph/util.h \
 $(INC_DIR)/ortools/graph/flow_problem.pb.h

ALGORITHMS_DEPS = \
 $(INC_DIR)/ortools/algorithms/dense_doubly_linked_list.h \
 $(INC_DIR)/ortools/algorithms/dynamic_partition.h \
 $(INC_DIR)/ortools/algorithms/dynamic_permutation.h \
 $(INC_DIR)/ortools/algorithms/find_graph_symmetries.h \
 $(INC_DIR)/ortools/algorithms/hungarian.h \
 $(INC_DIR)/ortools/algorithms/knapsack_solver_for_cuts.h \
 $(INC_DIR)/ortools/algorithms/knapsack_solver.h \
 $(INC_DIR)/ortools/algorithms/sparse_permutation.h

SAT_DEPS = \
 $(INC_DIR)/ortools/sat/all_different.h \
 $(INC_DIR)/ortools/sat/boolean_problem.h \
 $(INC_DIR)/ortools/sat/circuit.h \
 $(INC_DIR)/ortools/sat/clause.h \
 $(INC_DIR)/ortools/sat/cp_constraints.h \
 $(INC_DIR)/ortools/sat/cp_model_checker.h \
 $(INC_DIR)/ortools/sat/cp_model_expand.h \
 $(INC_DIR)/ortools/sat/cp_model.h \
 $(INC_DIR)/ortools/sat/cp_model_lns.h \
 $(INC_DIR)/ortools/sat/cp_model_loader.h \
 $(INC_DIR)/ortools/sat/cp_model_mapping.h \
 $(INC_DIR)/ortools/sat/cp_model_objective.h \
 $(INC_DIR)/ortools/sat/cp_model_postsolve.h \
 $(INC_DIR)/ortools/sat/cp_model_presolve.h \
 $(INC_DIR)/ortools/sat/cp_model_search.h \
 $(INC_DIR)/ortools/sat/cp_model_solver.h \
 $(INC_DIR)/ortools/sat/cp_model_symmetries.h \
 $(INC_DIR)/ortools/sat/cp_model_utils.h \
 $(INC_DIR)/ortools/sat/cumulative_energy.h \
 $(INC_DIR)/ortools/sat/cumulative.h \
 $(INC_DIR)/ortools/sat/cuts.h \
 $(INC_DIR)/ortools/sat/diffn.h \
 $(INC_DIR)/ortools/sat/diffn_util.h \
 $(INC_DIR)/ortools/sat/disjunctive.h \
 $(INC_DIR)/ortools/sat/drat_checker.h \
 $(INC_DIR)/ortools/sat/drat_proof_handler.h \
 $(INC_DIR)/ortools/sat/drat_writer.h \
 $(INC_DIR)/ortools/sat/encoding.h \
 $(INC_DIR)/ortools/sat/feasibility_pump.h \
 $(INC_DIR)/ortools/sat/implied_bounds.h \
 $(INC_DIR)/ortools/sat/inclusion.h \
 $(INC_DIR)/ortools/sat/integer_expr.h \
 $(INC_DIR)/ortools/sat/integer.h \
 $(INC_DIR)/ortools/sat/integer_search.h \
 $(INC_DIR)/ortools/sat/intervals.h \
 $(INC_DIR)/ortools/sat/lb_tree_search.h \
 $(INC_DIR)/ortools/sat/linear_constraint.h \
 $(INC_DIR)/ortools/sat/linear_constraint_manager.h \
 $(INC_DIR)/ortools/sat/linear_programming_constraint.h \
 $(INC_DIR)/ortools/sat/linear_relaxation.h \
 $(INC_DIR)/ortools/sat/lp_utils.h \
 $(INC_DIR)/ortools/sat/max_hs.h \
 $(INC_DIR)/ortools/sat/model.h \
 $(INC_DIR)/ortools/sat/optimization.h \
 $(INC_DIR)/ortools/sat/parameters_validation.h \
 $(INC_DIR)/ortools/sat/pb_constraint.h \
 $(INC_DIR)/ortools/sat/precedences.h \
 $(INC_DIR)/ortools/sat/presolve_context.h \
 $(INC_DIR)/ortools/sat/presolve_util.h \
 $(INC_DIR)/ortools/sat/probing.h \
 $(INC_DIR)/ortools/sat/pseudo_costs.h \
 $(INC_DIR)/ortools/sat/restart.h \
 $(INC_DIR)/ortools/sat/rins.h \
 $(INC_DIR)/ortools/sat/sat_base.h \
 $(INC_DIR)/ortools/sat/sat_decision.h \
 $(INC_DIR)/ortools/sat/sat_inprocessing.h \
 $(INC_DIR)/ortools/sat/sat_solver.h \
 $(INC_DIR)/ortools/sat/scheduling_constraints.h \
 $(INC_DIR)/ortools/sat/scheduling_cuts.h \
 $(INC_DIR)/ortools/sat/simplification.h \
 $(INC_DIR)/ortools/sat/subsolver.h \
 $(INC_DIR)/ortools/sat/swig_helper.h \
 $(INC_DIR)/ortools/sat/symmetry.h \
 $(INC_DIR)/ortools/sat/symmetry_util.h \
 $(INC_DIR)/ortools/sat/synchronization.h \
 $(INC_DIR)/ortools/sat/table.h \
 $(INC_DIR)/ortools/sat/theta_tree.h \
 $(INC_DIR)/ortools/sat/timetable_edgefinding.h \
 $(INC_DIR)/ortools/sat/timetable.h \
 $(INC_DIR)/ortools/sat/util.h \
 $(INC_DIR)/ortools/sat/var_domination.h \
 $(INC_DIR)/ortools/sat/zero_half_cuts.h \
 $(INC_DIR)/ortools/sat/boolean_problem.pb.h \
 $(INC_DIR)/ortools/sat/cp_model.pb.h \
 $(INC_DIR)/ortools/sat/cp_model_service.pb.h \
 $(INC_DIR)/ortools/sat/sat_parameters.pb.h

PACKING_DEPS = \
 $(INC_DIR)/ortools/packing/arc_flow_builder.h \
 $(INC_DIR)/ortools/packing/arc_flow_solver.h \
 $(INC_DIR)/ortools/packing/binpacking_2d_parser.h \
 $(INC_DIR)/ortools/packing/vector_bin_packing_parser.h \
 $(INC_DIR)/ortools/packing/multiple_dimensions_bin_packing.pb.h \
 $(INC_DIR)/ortools/packing/vector_bin_packing.pb.h

BOP_DEPS = \
 $(INC_DIR)/ortools/bop/bop_base.h \
 $(INC_DIR)/ortools/bop/bop_fs.h \
 $(INC_DIR)/ortools/bop/bop_lns.h \
 $(INC_DIR)/ortools/bop/bop_ls.h \
 $(INC_DIR)/ortools/bop/bop_portfolio.h \
 $(INC_DIR)/ortools/bop/bop_solution.h \
 $(INC_DIR)/ortools/bop/bop_solver.h \
 $(INC_DIR)/ortools/bop/bop_types.h \
 $(INC_DIR)/ortools/bop/bop_util.h \
 $(INC_DIR)/ortools/bop/complete_optimizer.h \
 $(INC_DIR)/ortools/bop/integral_solver.h \
 $(INC_DIR)/ortools/bop/bop_parameters.pb.h

GSCIP_DEPS = \
 $(INC_DIR)/ortools/gscip/gscip_event_handler.h \
 $(INC_DIR)/ortools/gscip/gscip_ext.h \
 $(INC_DIR)/ortools/gscip/gscip.h \
 $(INC_DIR)/ortools/gscip/gscip_message_handler.h \
 $(INC_DIR)/ortools/gscip/gscip_parameters.h \
 $(INC_DIR)/ortools/gscip/legacy_scip_params.h \
 $(INC_DIR)/ortools/gscip/gscip.pb.h

GUROBI_DEPS = \
 $(INC_DIR)/ortools/gurobi/environment.h

LP_DEPS = \
 $(INC_DIR)/ortools/linear_solver/glop_utils.h \
 $(INC_DIR)/ortools/linear_solver/gurobi_proto_solver.h \
 $(INC_DIR)/ortools/linear_solver/linear_expr.h \
 $(INC_DIR)/ortools/linear_solver/linear_solver_callback.h \
 $(INC_DIR)/ortools/linear_solver/linear_solver.h \
 $(INC_DIR)/ortools/linear_solver/model_exporter.h \
 $(INC_DIR)/ortools/linear_solver/model_exporter_swig_helper.h \
 $(INC_DIR)/ortools/linear_solver/model_validator.h \
 $(INC_DIR)/ortools/linear_solver/sat_proto_solver.h \
 $(INC_DIR)/ortools/linear_solver/sat_solver_utils.h \
 $(INC_DIR)/ortools/linear_solver/scip_callback.h \
 $(INC_DIR)/ortools/linear_solver/scip_helper_macros.h \
 $(INC_DIR)/ortools/linear_solver/scip_proto_solver.h \
 $(INC_DIR)/ortools/linear_solver/linear_solver.pb.h

CP_DEPS = \
 $(INC_DIR)/ortools/constraint_solver/constraint_solver.h \
 $(INC_DIR)/ortools/constraint_solver/constraint_solveri.h \
 $(INC_DIR)/ortools/constraint_solver/routing_filters.h \
 $(INC_DIR)/ortools/constraint_solver/routing_flags.h \
 $(INC_DIR)/ortools/constraint_solver/routing.h \
 $(INC_DIR)/ortools/constraint_solver/routing_index_manager.h \
 $(INC_DIR)/ortools/constraint_solver/routing_lp_scheduling.h \
 $(INC_DIR)/ortools/constraint_solver/routing_neighborhoods.h \
 $(INC_DIR)/ortools/constraint_solver/routing_parameters.h \
 $(INC_DIR)/ortools/constraint_solver/routing_search.h \
 $(INC_DIR)/ortools/constraint_solver/routing_types.h \
 $(INC_DIR)/ortools/constraint_solver/assignment.pb.h \
 $(INC_DIR)/ortools/constraint_solver/demon_profiler.pb.h \
 $(INC_DIR)/ortools/constraint_solver/routing_enums.pb.h \
 $(INC_DIR)/ortools/constraint_solver/routing_parameters.pb.h \
 $(INC_DIR)/ortools/constraint_solver/search_limit.pb.h \
 $(INC_DIR)/ortools/constraint_solver/search_stats.pb.h \
 $(INC_DIR)/ortools/constraint_solver/solver_parameters.pb.h
