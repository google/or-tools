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

if(BUILD_MATH_OPT)
  ortools_cxx_library(
    NAME ortools_math_opt
    TYPE SHARED
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/gurobi/isv_public/gurobi_isv.cc
      ${PROJECT_SOURCE_DIR}/ortools/gurobi/isv_public/gurobi_isv.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/indicator_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/indicator_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/quadratic_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/quadratic_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/sos1_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/sos1_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/sos2_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/sos2_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/util.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/util/model_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/util/model_util.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/arrow_operator_proxy.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/base_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/base_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/c_api/solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/c_api/solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/concurrent_calls_guard.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/concurrent_calls_guard.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/empty_bounds.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/empty_bounds.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/invalid_indicators.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/invalid_indicators.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/inverted_bounds.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/inverted_bounds.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/math_opt_proto_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/math_opt_proto_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/model_summary.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/model_summary.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/non_streamable_solver_init_arguments.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/non_streamable_solver_init_arguments.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_debug.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_debug.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sorted.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_submatrix.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_submatrix.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_vector.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_vector_view.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/basis_status.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/basis_status.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/callback.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/callback.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/compute_infeasible_subsystem_result.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/compute_infeasible_subsystem_result.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/enums.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/executor_init_args.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/local_solve_executor.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/local_solve_executor.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/solve_executor.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/time_limit_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/time_limit_util.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/formatters.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/incremental_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/incremental_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/key_types.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/linear_constraint.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/map_filter.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/map_filter.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/math_opt.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/message_callback.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/message_callback.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/model.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/model.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/model_solve_parameters.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/model_solve_parameters.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/objective.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/objective.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/parameters.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/parameters.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/remote_streaming_mode.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/remote_streaming_mode.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solution.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solution.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_arguments.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_arguments.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_impl.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_impl.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_result.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_result.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solver_init_arguments.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solver_resources.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solver_resources.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/sparse_containers.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/sparse_containers.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/statistics.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/statistics.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/streamable_solver_init_arguments.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/streamable_solver_init_arguments.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/update_result.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/update_result.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/update_tracker.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/update_tracker.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/variable_and_expressions.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/variable_and_expressions.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/arrays.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_diff.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_key.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attributes.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/gen.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/gen.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/gen_c.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/gen_c.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/gen_python.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/gen_python.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/derived_data.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/diff.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/diff.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_diff.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_ref_tracker.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_differencer.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_differencer.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_export_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_from_proto.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_to_string.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elements.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elements.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/safe_attr_ops.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/symmetry.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/tagged_id.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/thread_safe_id_map.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/lp_model.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/lp_model.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/lp_name.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/lp_name.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/model_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/model_utils.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp_converter.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp_converter.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/mps_converter.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/mps_converter.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/names_removal.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/names_removal.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/proto_converter.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/proto_converter.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/dualizer.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/dualizer.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/general_constraint_to_mip.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/general_constraint_to_mip.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/linear_expr_util.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/linear_expr_util.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/solution_feasibility_checker.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/solution_feasibility_checker.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/solution_improvement.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/solution_improvement.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/cp_sat_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/cp_sat_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/message_callback_data.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/message_callback_data.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/min_cost_flow_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/min_cost_flow_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/atomic_constraint_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/atomic_constraints_v2.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/iterators.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/linear_constraint_storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/linear_constraint_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/linear_expression_data.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_item.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_item.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_types.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_v2.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_v2.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/objective_storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/objective_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/range.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/sparse_coefficient_map.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/sparse_coefficient_map.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/sparse_matrix.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/sparse_matrix.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/update_trackers.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/variable_storage.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/variable_storage.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/testing/stream.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/bounds_and_status_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/bounds_and_status_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/callback_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/callback_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/ids_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/ids_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/infeasible_subsystem_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/infeasible_subsystem_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/linear_expression_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/linear_expression_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_parameters_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_parameters_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/result_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/result_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/scalar_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/scalar_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solution_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solution_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solve_parameters_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solve_parameters_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solve_stats_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solve_stats_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/sparse_matrix_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/sparse_matrix_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/sparse_vector_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/termination_validator.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/termination_validator.h
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_mip.cc
      ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_mip.h
    COMPILE_DEFINITIONS ${OR_TOOLS_COMPILE_DEFINITIONS}
    COMPILE_OPTIONS ${OR_TOOLS_COMPILE_OPTIONS}
    LINK_OPTIONS ${OR_TOOLS_LINK_OPTIONS}
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools_proto
      ${PROJECT_NAMESPACE}::ortools_core
      $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
      $<$<BOOL:${USE_HIGHS}>:highs::highs>
      $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      absl::algorithm_container
      absl::any_invocable
      absl::check
      absl::cleanup
      absl::core_headers
      absl::die_if_null
      absl::flat_hash_map
      absl::flat_hash_set
      absl::internal_optional_ref
      absl::linked_hash_map
      absl::log
      absl::memory
      absl::nullability
      absl::source_location
      absl::span
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
      protobuf::libprotobuf
  )
  if(USE_GLOP)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glop_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glop_solver.h
    )
  endif()
  if(USE_GLPK)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/gap.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/gap.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/glpk_sparse_vector.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/glpk_sparse_vector.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/rays.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/rays.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk_solver.h
    )
  endif()
  if(USE_GUROBI)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi/g_gurobi.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi/g_gurobi.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi_callback.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi_callback.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi_init_arguments.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi_init_arguments.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gurobi_solver.h
    )
  endif()
  if(USE_HIGHS)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/highs_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/highs_solver.h
    )
  endif()
  if(USE_PDLP)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/pdlp_bridge.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/pdlp_bridge.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/pdlp_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/pdlp_solver.h
    )
  endif()
  if(USE_SCIP)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp_parser.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp_parser.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_callback_result.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_callback_result.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_constraint_handler.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_constraint_handler.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_event_handler.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_event_handler.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_ext.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_ext.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_from_mp_model_proto.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_from_mp_model_proto.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_message_handler.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_message_handler.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_parameters.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_parameters.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/math_opt_gscip_solver_constraint_handler.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/math_opt_gscip_solver_constraint_handler.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip_solver.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/tools/file_format_flags.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/tools/file_format_flags.h
    )
  endif()
  if(USE_XPRESS)
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/xpress/g_xpress.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/xpress/g_xpress.h
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/xpress_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/xpress_solver.h
    )
  endif()
  if(USE_GLOP)
    target_link_libraries(ortools_math_opt PUBLIC absl::nullability absl::cleanup absl::flat_hash_map absl::overload absl::log absl::check absl::memory absl::status absl::status_macros absl::statusor absl::strings absl::time absl::span)
  endif()
  if(USE_GLPK)
    target_link_libraries(ortools_math_opt PUBLIC absl::nullability absl::cleanup absl::flat_hash_map absl::log absl::check absl::memory absl::status absl::status_macros absl::statusor absl::strings absl::time absl::span)
  endif()
  if(USE_GUROBI)
    target_link_libraries(ortools_math_opt PUBLIC absl::flat_hash_set absl::linked_hash_map absl::log absl::check absl::status absl::status_macros absl::statusor absl::strings absl::time absl::span absl::algorithm_container absl::nullability absl::flat_hash_map absl::memory protobuf::libprotobuf absl::die_if_null absl::vlog_is_on absl::str_format absl::source_location)
  endif()
  if(USE_HIGHS)
    target_link_libraries(ortools_math_opt PUBLIC absl::algorithm_container absl::nullability absl::cleanup absl::flat_hash_map absl::check absl::memory absl::status absl::status_macros absl::statusor absl::string_view absl::time)
  endif()
  if(USE_PDLP)
    target_link_libraries(ortools_math_opt PUBLIC absl::flat_hash_map absl::status absl::status_macros absl::statusor absl::strings absl::span Eigen3::Eigen absl::nullability absl::log absl::memory absl::time)
  endif()
  if(USE_SCIP)
    target_link_libraries(ortools_math_opt PUBLIC absl::memory absl::status absl::status_macros absl::statusor absl::strings absl::nullability absl::cleanup absl::flat_hash_map absl::flat_hash_set absl::linked_hash_map absl::log absl::check absl::die_if_null absl::time absl::span protobuf::libprotobuf absl::string_view absl::core_headers absl::str_format absl::synchronization absl::algorithm_container)
  endif()
  if(USE_XPRESS)
    target_link_libraries(ortools_math_opt PUBLIC absl::config absl::core_headers absl::nullability absl::flat_hash_set absl::linked_hash_map absl::log absl::check absl::memory absl::status absl::status_macros absl::statusor absl::strings absl::synchronization absl::time absl::span protobuf::libprotobuf absl::die_if_null absl::status_builder)
  endif()
  if(MSVC AND BUILD_SHARED_LIBS)
    set_property(SOURCE
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/base_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/concurrent_calls_guard.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/empty_bounds.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/invalid_indicators.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/inverted_bounds.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/math_opt_proto_utils.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/model_summary.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/non_streamable_solver_init_arguments.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_debug.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface.cc
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_submatrix.cc
      APPEND PROPERTY COMPILE_DEFINITIONS
      OR_EXPORT
    )
  endif()
  if(XCODE)
    file(GENERATE
      OUTPUT ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/math_opt_version.cpp
      CONTENT "namespace {char* math_opt_version = \"${PROJECT_VERSION}\";}}")
    target_sources(ortools_math_opt PRIVATE
      ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/math_opt_version.cpp)
  endif()
  target_link_libraries(ortools PUBLIC ortools_math_opt)

  if(USE_GLOP)
    ortools_cxx_binary(
      NAME math_opt_core_c_api_cpp_example
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/c_api/cpp_example.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::status
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME math_opt_elemental_codegen
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/codegen.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flags
      absl::log
      absl::check
      absl::string_view
    NO_INSTALL
  )

  if(USE_SCIP)
    ortools_cxx_binary(
      NAME math_opt_solvers_gscip_bugs_binary_variable_bounds
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/bugs/binary_variable_bounds.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_solvers_gscip_bugs_fixed_integer_variable_bounds
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/bugs/fixed_integer_variable_bounds.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_solvers_gscip_bugs_integer_to_binary_variable
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/bugs/integer_to_binary_variable.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_solvers_gscip_bugs_integer_to_continuous_variable
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/bugs/integer_to_continuous_variable.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::check
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME mathopt_solve
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/tools/mathopt_solve.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::no_destructor
        absl::nullability
        absl::flags
        absl::log
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
        absl::time
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME mathopt_convert
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/tools/mathopt_convert.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::status
        absl::status_macros
        absl::strings
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME set_cover_set_cover_solve
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_solve.cc
    LINK_LIBRARIES
      ortools::ortools
      absl::flags
      absl::log
      absl::check
      absl::strings
      absl::time
      absl::span
    NO_INSTALL
  )

  if(BUILD_CXX_SAMPLES)
    if(USE_SCIP)
      ortools_cxx_binary(
        NAME math_opt_samples_cpp_basic_example
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/basic_example.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::log
          absl::check
          absl::status
          absl::status_macros
        NO_INSTALL
      )

      ortools_cxx_binary(
        NAME math_opt_samples_cpp_cocktail_hour
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/cocktail_hour.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::flat_hash_map
          absl::flat_hash_set
          absl::flags
          absl::log
          absl::check
          absl::status
          absl::status_macros
          absl::statusor
          absl::strings
          absl::span
        NO_INSTALL
      )
    endif()

    if(USE_GLOP)
      ortools_cxx_binary(
        NAME math_opt_samples_cpp_linear_programming
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/linear_programming.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::log
          absl::check
          absl::status
          absl::status_macros
          absl::strings
          absl::time
        NO_INSTALL
      )

      ortools_cxx_binary(
        NAME math_opt_samples_cpp_advanced_linear_programming
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/advanced_linear_programming.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::log
          absl::check
          absl::status
          absl::status_macros
          absl::strings
          absl::time
        NO_INSTALL
      )
    endif()

    if(USE_SCIP)
      ortools_cxx_binary(
        NAME math_opt_samples_cpp_integer_programming
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/integer_programming.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::log
          absl::check
          absl::status
          absl::status_macros
          absl::time
        NO_INSTALL
      )
    endif()

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_cutting_stock
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/cutting_stock.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::log
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_facility_lp_benders
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/facility_lp_benders.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flat_hash_map
        absl::flags
        absl::log
        absl::check
        absl::memory
        absl::random_random
        absl::random_distributions
        absl::random_seed_sequences
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
        absl::str_format
        absl::time
        absl::span
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_lagrangian_relaxation
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/lagrangian_relaxation.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
        absl::str_format
      NO_INSTALL
    )

    if(USE_PDLP)
      ortools_cxx_binary(
        NAME math_opt_samples_cpp_linear_regression
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/linear_regression.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::algorithm_container
          absl::flags
          absl::log
          absl::check
          absl::random_random
          absl::status
          absl::status_macros
          absl::statusor
          absl::strings
          absl::span
        NO_INSTALL
      )
    endif()

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_mathopt_info
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/mathopt_info.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )

    if(USE_SCIP)
      ortools_cxx_binary(
        NAME math_opt_samples_cpp_tsp
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/tsp.cc
        LINK_LIBRARIES
          ortools::ortools
          absl::flat_hash_set
          absl::flags
          absl::log
          absl::check
          absl::random_random
          absl::status
          absl::status_macros
          absl::statusor
          absl::strings
          absl::span
        NO_INSTALL
      )
    endif()

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_graph_coloring
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/graph_coloring.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_area_socp
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/area_socp.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::check
        absl::status
        absl::status_macros
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_time_indexed_scheduling
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/time_indexed_scheduling.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::algorithm_container
        absl::flags
        absl::log
        absl::random_random
        absl::status
        absl::status_macros
        absl::statusor
        absl::span
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_min_cost_flow
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/min_cost_flow.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::status
        absl::status_macros
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_samples_cpp_matching_by_min_cost_flow
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/cpp/matching_by_min_cost_flow.cc
      LINK_LIBRARIES
        ortools::ortools
        absl::flags
        absl::log
        absl::random_random
        absl::status
        absl::status_macros
        absl::time
      NO_INSTALL
    )
  endif()

  if(BUILD_CXX_TESTING)
    ortools_cxx_library(
      NAME gurobi_gurobi_stdout_matchers
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/gurobi/gurobi_stdout_matchers.cc
        ${PROJECT_SOURCE_DIR}/ortools/gurobi/gurobi_stdout_matchers.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_core_message_callback_testing
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/message_callback_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/message_callback_testing.h
      LINK_LIBRARIES
        ortools::ortools
        absl::nullability
        absl::synchronization
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_core_solver_interface_testing
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface_testing.h
      LINK_LIBRARIES
        ortools::ortools
        absl::nullability
        absl::flat_hash_map
        absl::flat_hash_set
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_core_solver_interface_mock
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface_mock.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface_mock.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_core_solver_interface_testing
        ortools::base_gmock
        absl::core_headers
        absl::nullability
        absl::log
        absl::check
        absl::die_if_null
        absl::random_random
        absl::statusor
        absl::synchronization
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_core_sparse_collection_matchers
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_collection_matchers.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_collection_matchers.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_cpp_enums_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/enums_testing.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::check
        absl::int128
        absl::strings
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_cpp_matchers
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/matchers.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/matchers.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::flat_hash_map
        absl::log
        absl::check
        absl::statusor
        absl::strings
        absl::span
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_cpp_executor_solve_executor_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/solve_executor_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/solve_executor_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::check
        absl::status
        absl::statusor
        absl::strings
        absl::time
        ortools::math_opt_cpp_matchers
        ortools::port_scoped_std_stream_capture
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_cpp_solve_impl_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_impl_testing.h
      LINK_LIBRARIES
        ortools::ortools
        absl::core_headers
        absl::nullability
        absl::function_ref
        absl::status
        absl::status_macros
        absl::statusor
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_elemental_codegen_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/codegen/testing.h
      LINK_LIBRARIES ortools::ortools
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_elemental_elemental_matcher
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_matcher.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_matcher.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::core_headers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_elemental_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/testing.h
      LINK_LIBRARIES ortools::ortools
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_io_lp_matchers
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/matchers.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/matchers.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_base_solver_test
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/base_solver_test.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/base_solver_test.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::log
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_infeasible_subsystem_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/infeasible_subsystem_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/infeasible_subsystem_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::status
        absl::statusor
        absl::strings
        absl::time
        ortools::gurobi_gurobi_stdout_matchers
        ortools::math_opt_cpp_matchers
        ortools::port_scoped_std_stream_capture
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_ip_model_solve_parameters_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::base_gmock
        absl::status_macros
        absl::statusor
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_ip_multiple_solutions_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/ip_multiple_solutions_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/ip_multiple_solutions_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_logical_constraint_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/logical_constraint_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/logical_constraint_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::status
        absl::statusor
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_lp_initial_basis_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_initial_basis_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_initial_basis_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::base_gmock
        absl::check
        absl::statusor
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_lp_parameter_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_parameter_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_parameter_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::flat_hash_set
        absl::log
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_lp_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::base_gmock
        absl::check
        absl::statusor
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_min_cost_flow_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/min_cost_flow_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/min_cost_flow_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::log
        absl::status
        absl::status_matchers
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_mip_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/mip_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/mip_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::base_gmock
        absl::check
        absl::statusor
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_multi_objective_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/multi_objective_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/multi_objective_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::core_headers
        absl::status
        absl::status_macros
        absl::statusor
        absl::string_view
        absl::time
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_qc_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/qc_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/qc_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::status
        absl::statusor
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_qp_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/qp_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/qp_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::log
        absl::check
        absl::status
        absl::statusor
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_second_order_cone_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/second_order_cone_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/second_order_cone_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::status
        absl::statusor
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_test_models
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/test_models.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/test_models.h
      LINK_LIBRARIES
        ortools::ortools
        absl::nullability
        absl::log
        absl::check
        absl::random_random
        absl::strings
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_callback_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/callback_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/callback_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        absl::flat_hash_set
        absl::log
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
        absl::synchronization
        absl::span
        ortools::gurobi_gurobi_stdout_matchers
        ortools::math_opt_cpp_matchers
        ortools::port_scoped_std_stream_capture
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_generic_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/generic_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/generic_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        absl::core_headers
        absl::nullability
        absl::flat_hash_set
        absl::log
        absl::check
        absl::statusor
        absl::strings
        absl::synchronization
        absl::time
        ortools::gurobi_gurobi_stdout_matchers
        ortools::math_opt_cpp_matchers
        ortools::port_scoped_std_stream_capture
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_invalid_input_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/invalid_input_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/invalid_input_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        absl::status
        absl::statusor
        absl::strings
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_ip_parameter_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/ip_parameter_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/ip_parameter_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        absl::flat_hash_set
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
        absl::time
        absl::span
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_lp_incomplete_solve_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_incomplete_solve_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_incomplete_solve_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        absl::log
        absl::check
        absl::strings
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_lp_model_solve_parameters_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_base_solver_test
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_solver_tests_status_tests
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/status_tests.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/status_tests.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_solver_tests_test_models
        ortools::base_gmock
        absl::check
        absl::status_macros
        absl::statusor
        absl::strings
        absl::time
        ortools::math_opt_cpp_matchers
      TESTING
      NO_INSTALL
    )

    if(USE_SCIP)
      ortools_cxx_library(
        NAME math_opt_solvers_gscip_gscip_testing
        TYPE STATIC
        SOURCES
          ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_testing.cc
          ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_testing.h
        LINK_LIBRARIES
          ortools::ortools
          ortools::base_gmock
          absl::strings
        TESTING
        NO_INSTALL
      )
    endif()

    ortools_cxx_library(
      NAME math_opt_storage_atomic_constraint_testing
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/atomic_constraint_testing.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
        absl::status
        absl::statusor
        absl::strings
        protobuf::libprotobuf
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_constraints_indicator_storage_test_helper
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/storage_test_helper.h
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
        absl::span
        ortools::math_opt_storage_atomic_constraint_testing
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_constraints_quadratic_storage_test_helper
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/storage_test_helper.h
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
        absl::span
        ortools::math_opt_storage_atomic_constraint_testing
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_constraints_second_order_cone_storage_test_helper
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/storage_test_helper.h
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
        absl::span
        ortools::math_opt_storage_atomic_constraint_testing
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_constraints_sos_storage_test_helper
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/storage_test_helper.h
      LINK_LIBRARIES
        ortools::ortools
        absl::strings
        absl::span
        ortools::math_opt_storage_atomic_constraint_testing
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_testing_param_name
      TYPE INTERFACE
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/testing/param_name.h
      LINK_LIBRARIES
        ortools::ortools
        ortools::base_gmock
      TESTING
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME math_opt_validators_enum_sets
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/enum_sets.cc
        ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/enum_sets.h
      LINK_LIBRARIES ortools::ortools
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_elemental_elemental_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_elemental_testing
        benchmark::benchmark_main
        absl::check
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_elemental_element_storage_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_storage_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_elemental_attr_storage_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_storage_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        benchmark::benchmark_main
      TESTING
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME math_opt_elemental_attr_key_benchmark
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_key_benchmark.cc
      LINK_LIBRARIES
        ortools::ortools
        ortools::math_opt_elemental_testing
        benchmark::benchmark_main
        absl::algorithm_container
        absl::flat_hash_map
        absl::flat_hash_set
      TESTING
      NO_INSTALL
    )

    ortools_cxx_test(
      NAME graph_dag_constrained_shortest_path_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/dag_constrained_shortest_path_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::check
        absl::random_random
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_constraints_indicator_indicator_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/indicator_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_constraints_indicator_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/storage_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_constraints_indicator_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/indicator/validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_constraints_quadratic_quadratic_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/quadratic_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_constraints_quadratic_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/storage_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_constraints_quadratic_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/quadratic/validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_constraints_second_order_cone_second_order_cone_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_constraints_second_order_cone_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/storage_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
    )

    ortools_cxx_test(
      NAME math_opt_constraints_second_order_cone_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/second_order_cone/validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_constraints_sos_sos1_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/sos1_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        ortools::util_fp_roundtrip_conv_testing
    )

    ortools_cxx_test(
      NAME math_opt_constraints_sos_sos2_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/sos2_constraint_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
        ortools::util_fp_roundtrip_conv_testing
    )

    ortools_cxx_test(
      NAME math_opt_constraints_sos_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/storage_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
    )

    ortools_cxx_test(
      NAME math_opt_constraints_sos_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/sos/validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_constraints_util_model_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/constraints/util/model_util_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_core_math_opt_proto_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/math_opt_proto_utils_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_core_sparse_vector_view_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_vector_view_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::flat_hash_map
        absl::check
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_core_message_callback_testing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/message_callback_testing_test.cc
      LINK_LIBRARIES
        ortools::math_opt_core_message_callback_testing
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_core_model_summary_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/model_summary_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_core_sparse_collection_matchers_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_collection_matchers_test.cc
      LINK_LIBRARIES
        ortools::math_opt_core_sparse_collection_matchers
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_core_non_streamable_solver_init_arguments_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/non_streamable_solver_init_arguments_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
    )

    ortools_cxx_test(
      NAME math_opt_core_sparse_submatrix_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sparse_submatrix_test.cc
      LINK_LIBRARIES
        ortools::math_opt_core_sparse_collection_matchers
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_core_solver_interface_testing_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/solver_interface_testing_test.cc
      LINK_LIBRARIES
        ortools::math_opt_core_solver_interface_testing
        absl::nullability
        absl::status
        absl::statusor
    )

    ortools_cxx_test(
      NAME math_opt_core_inverted_bounds_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/inverted_bounds_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_core_invalid_indicators_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/invalid_indicators_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_core_concurrent_calls_guard_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/concurrent_calls_guard_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::status_macros
        absl::statusor
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_core_empty_bounds_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/empty_bounds_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_core_sorted_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/sorted_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        protobuf::libprotobuf
    )

    if(USE_GLOP)
      ortools_cxx_test(
        NAME math_opt_core_c_api_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/c_api/solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::cleanup
          absl::status
          absl::status_builder
          absl::statusor
          absl::string_view
      )

      ortools_cxx_test(
        NAME math_opt_core_c_api_solver_port_c_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/c_api/solver_port_c_test.c
      )

      ortools_cxx_bintest(
        NAME math_opt_core_c_api_cpp_example_test
        SCRIPT ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/c_api/cpp_example_test.bintest
        ENVIRONMENT "BINTEST_cpp_example=$<TARGET_FILE:ortools::math_opt_core_c_api_cpp_example>"
      )
    endif()

    ortools_cxx_test(
      NAME math_opt_cpp_basis_status_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/basis_status_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_enums_testing
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_cpp_sparse_containers_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/sparse_containers_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::flat_hash_map
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_cpp_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/model_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::status
        absl::strings
        ortools::util_fp_roundtrip_conv_testing
      DISABLED
    )

    ortools_cxx_test(
      NAME math_opt_cpp_variable_and_expressions_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/variable_and_expressions_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::flat_hash_map
        absl::strings
        absl::str_format
        ortools::util_fp_roundtrip_conv_testing
    )

    ortools_cxx_test(
      NAME math_opt_cpp_objective_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/objective_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_cpp_linear_constraint_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/linear_constraint_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_cpp_solution_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solution_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_enums_testing
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_cpp_map_filter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/map_filter_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_cpp_key_types_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/key_types_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::flat_hash_set
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_cpp_solve_arguments_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solve_arguments_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_cpp_streamable_solver_init_arguments_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/streamable_solver_init_arguments_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_cpp_matchers_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/matchers_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::flat_hash_map
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_cpp_enums_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/enums_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_enums_testing
        GTest::gtest_main
        absl::check
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_cpp_statistics_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/statistics_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_cpp_update_result_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/update_result_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_cpp_compute_infeasible_subsystem_result_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/compute_infeasible_subsystem_result_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::flat_hash_map
        absl::flat_hash_set
        absl::status
        absl::statusor
    )

    ortools_cxx_test(
      NAME math_opt_cpp_solver_resources_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/solver_resources_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_cpp_incremental_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/incremental_solver_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_matchers
        GTest::gtest_main
        absl::status
        absl::statusor
        absl::string_view
    )

    ortools_cxx_test(
      NAME math_opt_cpp_remote_streaming_mode_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/remote_streaming_mode_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flags_marshalling
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_cpp_executor_time_limit_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/time_limit_util_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::time
    )

    ortools_cxx_test(
      NAME math_opt_cpp_executor_local_solve_executor_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/cpp/executor/local_solve_executor_test.cc
      LINK_LIBRARIES
        ortools::math_opt_cpp_executor_solve_executor_tests
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_attributes_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attributes_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_test.cc
      LINK_LIBRARIES
        ortools::math_opt_elemental_elemental_matcher
        GTest::gtest_main
        absl::check
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_elemental_derived_data_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/derived_data_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_element_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_storage_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_elemental_element_diff_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_diff_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_diff_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/diff_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_elemental_attr_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_storage_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_attr_diff_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_diff_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_attr_key_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/attr_key_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::hash_testing
        absl::type_traits
        absl::status
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_differencer_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_differencer_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
    )

    ortools_cxx_test(
      NAME math_opt_elemental_arrays_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/arrays_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::strings
        absl::string_view
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_export_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_export_model_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_to_string_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_to_string_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_elemental_safe_attr_ops_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/safe_attr_ops_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_elemental_tagged_id_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/tagged_id_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::hash_testing
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elements_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elements_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::hash_testing
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_matcher_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_matcher_test.cc
      LINK_LIBRARIES
        ortools::math_opt_elemental_elemental_matcher
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_element_ref_tracker_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/element_ref_tracker_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_thread_safe_id_map_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/thread_safe_id_map_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_from_proto_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_from_proto_test.cc
      LINK_LIBRARIES
        ortools::math_opt_elemental_elemental_matcher
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_elemental_elemental_update_from_proto_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/elemental_update_from_proto_test.cc
      LINK_LIBRARIES
        ortools::math_opt_elemental_elemental_matcher
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_io_proto_converter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/proto_converter_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::status
        absl::statusor
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_io_mps_converter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/mps_converter_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_io_names_removal_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/names_removal_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_io_lp_converter_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp_converter_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    if(USE_SCIP)
      ortools_cxx_test(
        NAME math_opt_io_lp_parser_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp_parser_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
      )
    endif()

    ortools_cxx_test(
      NAME math_opt_io_lp_lp_model_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/lp_model_test.cc
      LINK_LIBRARIES
        ortools::math_opt_io_lp_matchers
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_io_lp_matchers_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/matchers_test.cc
      LINK_LIBRARIES
        ortools::math_opt_io_lp_matchers
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_io_lp_lp_name_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/lp_name_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_io_lp_model_utils_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/lp/model_utils_test.cc
      LINK_LIBRARIES
        ortools::math_opt_io_lp_matchers
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_labs_general_constraint_to_mip_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/general_constraint_to_mip_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        ortools::math_opt_cpp_matchers
    )

    ortools_cxx_test(
      NAME math_opt_labs_linear_expr_util_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/linear_expr_util_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_labs_solution_feasibility_checker_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/solution_feasibility_checker_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::flat_hash_set
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_labs_solution_improvement_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/solution_improvement_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_map
        absl::check
        absl::status
        absl::status_macros
        absl::statusor
        absl::span
        ortools::math_opt_cpp_matchers
    )

    if(USE_GLOP)
      ortools_cxx_test(
        NAME math_opt_labs_dualizer_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/labs/dualizer_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::strings
          ortools::math_opt_cpp_matchers
      )
    endif()

    ortools_cxx_test(
      NAME math_opt_solver_tests_test_models_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solver_tests/test_models_test.cc
      LINK_LIBRARIES
        ortools::math_opt_solver_tests_test_models
        GTest::gtest_main
        ortools::math_opt_cpp_matchers
    )

    if(USE_GLOP)
      ortools_cxx_test(
        NAME math_opt_solvers_glop_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glop_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_infeasible_subsystem_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_invalid_input_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_logical_constraint_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_incomplete_solve_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_initial_basis_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_min_cost_flow_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_multi_objective_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qc_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_second_order_cone_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_status_tests>"
          ortools::math_opt_solver_tests_test_models
          ortools::math_opt_testing_param_name
      )
    endif()

    ortools_cxx_test(
      NAME math_opt_solvers_cp_sat_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/cp_sat_solver_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        ortools::math_opt_cpp_matchers
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_infeasible_subsystem_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_invalid_input_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_model_solve_parameters_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_multiple_solutions_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_parameter_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_logical_constraint_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_mip_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_multi_objective_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qc_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qp_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_second_order_cone_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_status_tests>"
        ortools::math_opt_solver_tests_test_models
        ortools::math_opt_testing_param_name
      DISABLED
    )

    ortools_cxx_test(
      NAME math_opt_solvers_message_callback_data_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/message_callback_data_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::cleanup
        absl::synchronization
        absl::span
    )

    if(USE_PDLP)
      ortools_cxx_test(
        NAME math_opt_solvers_pdlp_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/pdlp_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_infeasible_subsystem_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_invalid_input_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_logical_constraint_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_incomplete_solve_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_min_cost_flow_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_multi_objective_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qc_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_second_order_cone_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_status_tests>"
          ortools::math_opt_solver_tests_test_models
          ortools::math_opt_testing_param_name
      )
    endif()

    ortools_cxx_test(
      NAME math_opt_solvers_min_cost_flow_solver_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/min_cost_flow_solver_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::core_headers
        absl::status
        absl::strings
        absl::string_view
        absl::time
        ortools::math_opt_cpp_matchers
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_invalid_input_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_logical_constraint_tests>"
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_min_cost_flow_tests>"
        ortools::math_opt_solver_tests_test_models
        ortools::math_opt_testing_param_name
      DISABLED
    )

    if(USE_GLPK)
      ortools_cxx_test(
        NAME math_opt_solvers_glpk_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          absl::statusor
          absl::time
          ortools::math_opt_cpp_matchers
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_infeasible_subsystem_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_invalid_input_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_logical_constraint_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_incomplete_solve_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_min_cost_flow_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_mip_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_multi_objective_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qc_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_second_order_cone_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_status_tests>"
          ortools::math_opt_solver_tests_test_models
          ortools::math_opt_testing_param_name
      )
    endif()

    if(USE_HIGHS)
      ortools_cxx_test(
        NAME math_opt_solvers_highs_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/highs_solver_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::status
          ortools::math_opt_cpp_matchers
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_infeasible_subsystem_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_min_cost_flow_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_mip_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_multi_objective_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_status_tests>"
          ortools::math_opt_solver_tests_test_models
          ortools::math_opt_testing_param_name
        DISABLED
      )
    endif()

    if(USE_XPRESS)
      ortools_cxx_test(
        NAME math_opt_solvers_xpress_solver_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/xpress_solver_test.cc
        LINK_LIBRARIES
          absl::flat_hash_set
          absl::log
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_callback_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_generic_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_infeasible_subsystem_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_invalid_input_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_ip_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_logical_constraint_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_incomplete_solve_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_model_solve_parameters_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_parameter_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_lp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_mip_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_multi_objective_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qc_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_qp_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_second_order_cone_tests>"
          "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_solver_tests_status_tests>"
          ortools::math_opt_solver_tests_test_models
      )
    endif()

    if(USE_GLPK)
      ortools_cxx_test(
        NAME math_opt_solvers_glpk_glpk_sparse_vector_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/glpk_sparse_vector_test.cc
        LINK_LIBRARIES GTest::gtest_main
      )

      ortools_cxx_test(
        NAME math_opt_solvers_glpk_gap_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/glpk/gap_test.cc
        LINK_LIBRARIES GTest::gtest_main
      )
    endif()

    if(USE_SCIP)
      ortools_cxx_test(
        NAME math_opt_solvers_gscip_gscip_parameters_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_parameters_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::time
      )

      ortools_cxx_test(
        NAME math_opt_solvers_gscip_gscip_io_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_io_test.cc
        LINK_LIBRARIES
          ortools::math_opt_solvers_gscip_gscip_testing
          GTest::gtest_main
      )

      ortools_cxx_test(
        NAME math_opt_solvers_gscip_gscip_ext_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_ext_test.cc
        LINK_LIBRARIES
          ortools::math_opt_solvers_gscip_gscip_testing
          GTest::gtest_main
          absl::flat_hash_map
          $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      )

      ortools_cxx_test(
        NAME math_opt_solvers_gscip_gscip_event_handler_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_event_handler_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          absl::log
          absl::check
          absl::status
          absl::statusor
          absl::strings
          $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      )

      ortools_cxx_test(
        NAME math_opt_solvers_gscip_gscip_constraint_handler_test
        SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/solvers/gscip/gscip_constraint_handler_test.cc
        LINK_LIBRARIES
          ortools::math_opt_solvers_gscip_gscip_testing
          GTest::gtest_main
          absl::check
          absl::status
          absl::status_macros
          absl::statusor
          absl::strings
          $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
      )
    endif()

    ortools_cxx_test(
      NAME math_opt_storage_model_storage_item_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_item_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::hash_testing
        absl::string_view
    )

    ortools_cxx_test(
      NAME math_opt_storage_range_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/range_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_iterators_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/iterators_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_sparse_coefficient_map_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/sparse_coefficient_map_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_sparse_matrix_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/sparse_matrix_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_storage_linear_expression_data_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/linear_expression_data_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_update_trackers_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/update_trackers_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_variable_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/variable_storage_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_objective_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/objective_storage_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_storage_linear_constraint_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/linear_constraint_storage_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_storage_atomic_constraint_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/atomic_constraint_storage_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::flat_hash_set
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_storage_model_storage_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_test.cc
      LINK_LIBRARIES
        ortools::math_opt_storage_atomic_constraint_testing
        GTest::gtest_main
        absl::algorithm_container
        absl::status
        absl::statusor
        ortools::math_opt_constraints_indicator_storage_test_helper
        ortools::math_opt_constraints_quadratic_storage_test_helper
        ortools::math_opt_constraints_second_order_cone_storage_test_helper
        ortools::math_opt_constraints_sos_storage_test_helper
    )

    ortools_cxx_test(
      NAME math_opt_storage_model_storage_v2_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/storage/model_storage_v2_test.cc
      LINK_LIBRARIES
        ortools::math_opt_storage_atomic_constraint_testing
        GTest::gtest_main
        absl::status
        absl::strings
        ortools::math_opt_constraints_indicator_storage_test_helper
        ortools::math_opt_constraints_quadratic_storage_test_helper
        ortools::math_opt_constraints_second_order_cone_storage_test_helper
        ortools::math_opt_constraints_sos_storage_test_helper
    )

    ortools_cxx_test(
      NAME math_opt_testing_param_name_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/testing/param_name_test.cc
      LINK_LIBRARIES
        ortools::math_opt_testing_param_name
        GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_testing_stream_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/testing/stream_test.cc
      LINK_LIBRARIES GTest::gtest_main
    )

    ortools_cxx_test(
      NAME math_opt_validators_ids_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/ids_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_validators_scalar_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/scalar_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_validators_sparse_matrix_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/sparse_matrix_validator_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::check
        absl::status
        absl::strings
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_validators_sparse_vector_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/sparse_vector_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_validators_model_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::strings
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME math_opt_validators_model_update_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_update_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::algorithm_container
        absl::check
        absl::status
        absl::statusor
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_validators_solve_stats_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solve_stats_validator_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_validators_result_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/result_validator_test.cc
      LINK_LIBRARIES
        ortools::math_opt_validators_enum_sets
        GTest::gtest_main
        absl::check
        absl::status
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_validators_solution_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solution_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::statusor
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_validators_solve_parameters_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/solve_parameters_validator_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_validators_callback_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/callback_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::check
        absl::status
        absl::statusor
        absl::strings
        protobuf::libprotobuf
    )

    ortools_cxx_test(
      NAME math_opt_validators_model_parameters_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/model_parameters_validator_test.cc
      LINK_LIBRARIES
        protobuf::libprotobuf
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_validators_linear_expression_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/linear_expression_validator_test.cc
      LINK_LIBRARIES
        ortools::base_parse_test_proto
        GTest::gtest_main
        absl::check
        absl::status
        absl::span
    )

    ortools_cxx_test(
      NAME math_opt_validators_termination_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/termination_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME math_opt_validators_bounds_and_status_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/bounds_and_status_validator_test.cc
      LINK_LIBRARIES
        ortools::math_opt_validators_enum_sets
        GTest::gtest_main
        absl::status
        absl::strings
    )

    ortools_cxx_test(
      NAME math_opt_validators_infeasible_subsystem_validator_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/validators/infeasible_subsystem_validator_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::status
    )

    ortools_cxx_test(
      NAME set_cover_set_cover_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/set_cover_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::time
      DISABLED
    )

    ortools_cxx_test(
      NAME set_cover_knights_test
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/knights_test.cc
      LINK_LIBRARIES
        GTest::gtest_main
        absl::log
        absl::check
        absl::strings
        absl::time
      DISABLED
    )
  endif()
endif()
