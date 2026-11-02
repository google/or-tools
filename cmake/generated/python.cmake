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

if(BUILD_CORE)
  ortools_python_copy(
    PACKAGE_DIR linear_solver/python
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/model_builder.py
      ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/model_builder_numbers.py
  )

  ortools_python_copy(
    PACKAGE_DIR sat/colab
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/colab/visualization.py
  )

  ortools_python_copy(
    PACKAGE_DIR sat/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/python/cp_model.py
  )

  ortools_python_copy(
    PACKAGE_DIR util/python
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/util/python/solve_interrupter.py
      ${PROJECT_SOURCE_DIR}/ortools/util/python/sorted_interval_list.pyi
      ${PROJECT_SOURCE_DIR}/ortools/util/python/status_streaming.py
  )

  ortools_cxx_library(
    NAME algorithms_python_knapsack_solver_doc_cc
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME init_python_init_doc_h
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME sat_python_linear_expr_doc_cc
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME sat_python_proto_builder_pybind11
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME sat_python_pybind_constraint
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/pybind_constraint.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/pybind_constraint.h
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      pybind11::module
      absl::flat_hash_map
      absl::check
      absl::strings
      protobuf::libprotobuf
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME sat_python_pybind_linearexpr
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/pybind_linearexpr.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/pybind_linearexpr.h
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      pybind11::module
      ${PROJECT_NAMESPACE}::sat_python_linear_expr_doc_cc
      absl::dynamic_annotations
      absl::any_invocable
      absl::strings
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME sat_python_pybind_solver
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/pybind_solver.cc
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/pybind_solver.h
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      pybind11::module
      absl::strings
      protobuf::libprotobuf
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME util_python_py_solve_interrupter_lib
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/util/python/py_solve_interrupter.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/python/py_solve_interrupter.h
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::core_headers
      absl::nullability
      absl::log
      absl::check
      absl::synchronization
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME util_python_sorted_interval_list_doc_cc
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME util_python_wrappers
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/util/python/wrappers.cc
      ${PROJECT_SOURCE_DIR}/ortools/util/python/wrappers.h
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::algorithm_container
      absl::nullability
      absl::flat_hash_map
      absl::flat_hash_set
      absl::log
      absl::check
      absl::die_if_null
      absl::strings
      absl::span
      protobuf::libprotobuf
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME sat_python_gen_proto_builder_pybind11
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/sat/python/gen_proto_builder_pybind11.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::flags_parse
      absl::flags_usage
      absl::die_if_null
      absl::log_initialize
      absl::str_format
      protobuf::libprotobuf
      ${PROJECT_NAMESPACE}::util_python_wrappers
    NO_INSTALL
  )

  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/ortools/sat/python/proto_builder_pybind11.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/ortools/sat/python
    COMMAND $<TARGET_FILE:sat_python_gen_proto_builder_pybind11> > ${PROJECT_BINARY_DIR}/ortools/sat/python/proto_builder_pybind11.h
    DEPENDS sat_python_gen_proto_builder_pybind11
    COMMENT "Generate C++ ortools/sat/python/proto_builder_pybind11.h"
    VERBATIM
  )

  ortools_python_extension(
    NAME knapsack_solver_pybind11
    MODULE_NAME knapsack_solver
    PACKAGE_DIR algorithms/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/algorithms/python/knapsack_solver.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::algorithms_python_knapsack_solver_doc_cc
  )

  ortools_python_extension(
    NAME linear_sum_assignment_pybind11
    MODULE_NAME linear_sum_assignment
    PACKAGE_DIR graph/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/python/linear_sum_assignment.cc
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
  )

  ortools_python_extension(
    NAME max_flow_pybind11
    MODULE_NAME max_flow
    PACKAGE_DIR graph/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/python/max_flow.cc
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
  )

  ortools_python_extension(
    NAME min_cost_flow_pybind11
    MODULE_NAME min_cost_flow
    PACKAGE_DIR graph/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/python/min_cost_flow.cc
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
  )

  ortools_python_extension(
    NAME dag_shortest_path_pybind11
    MODULE_NAME dag_shortest_path
    PACKAGE_DIR graph/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/graph/python/dag_shortest_path.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::strings
    NO_STUBGEN
  )

  ortools_python_extension(
    NAME init_pybind11
    MODULE_NAME init
    PACKAGE_DIR init/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/init/python/init.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::init_python_init_doc_h
  )

  ortools_python_extension(
    NAME model_builder_helper_pybind11
    MODULE_NAME model_builder_helper
    PACKAGE_DIR linear_solver/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/model_builder_helper.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::dynamic_annotations
      absl::hash
      absl::check
      absl::strings
      absl::span
      Eigen3::Eigen
      protobuf::libprotobuf
      pybind11_native_proto_caster
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  ortools_python_extension(
    NAME pywraplp_pybind11
    MODULE_NAME pywraplp
    PACKAGE_DIR linear_solver/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/pywraplp.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::algorithm_container
      absl::flat_hash_map
      absl::inlined_vector
      absl::log
      absl::check
      absl::status
      absl::strings
      absl::time
      protobuf::libprotobuf
      pybind11_native_proto_caster
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  ortools_python_extension(
    NAME cp_model_helper_pybind11
    MODULE_NAME cp_model_helper
    PACKAGE_DIR sat/python
    SOURCES
      ${PROJECT_BINARY_DIR}/ortools/sat/python/proto_builder_pybind11.h
      ${PROJECT_SOURCE_DIR}/ortools/sat/python/cp_model_helper.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::sat_python_linear_expr_doc_cc
      ${PROJECT_NAMESPACE}::sat_python_proto_builder_pybind11
      ${PROJECT_NAMESPACE}::sat_python_pybind_constraint
      ${PROJECT_NAMESPACE}::sat_python_pybind_linearexpr
      ${PROJECT_NAMESPACE}::sat_python_pybind_solver
      absl::flat_hash_map
      absl::any_invocable
      absl::check
      absl::strings
      protobuf::libprotobuf
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  ortools_python_extension(
    NAME set_cover_pybind11
    MODULE_NAME set_cover
    PACKAGE_DIR set_cover/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/set_cover/python/set_cover.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::time
      absl::span
      pybind11_native_proto_caster
      protobuf::libprotobuf
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  ortools_python_extension(
    NAME sorted_interval_list_pybind11
    MODULE_NAME sorted_interval_list
    PACKAGE_DIR util/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/python/sorted_interval_list.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::util_python_sorted_interval_list_doc_cc
      absl::strings
  )

  ortools_python_extension(
    NAME piecewise_linear_function_pybind11
    MODULE_NAME piecewise_linear_function
    PACKAGE_DIR util/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/python/piecewise_linear_function.cc
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
  )

  ortools_python_extension(
    NAME solve_interrupter_pybind11
    MODULE_NAME pybind_solve_interrupter
    PACKAGE_DIR util/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/python/pybind_solve_interrupter.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::util_python_py_solve_interrupter_lib
  )

  if(BUILD_PYTHON_SAMPLES)
    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/algorithms/samples/simple_knapsack_program.py
      COMPONENT_NAME algorithms
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/algorithms/samples/knapsack.py
      COMPONENT_NAME algorithms
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/graph/samples/assignment_linear_sum_assignment.py
      COMPONENT_NAME graph
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/graph/samples/assignment_min_flow.py
      COMPONENT_NAME graph
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/graph/samples/balance_min_flow.py
      COMPONENT_NAME graph
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/graph/samples/simple_max_flow_program.py
      COMPONENT_NAME graph
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/graph/samples/simple_min_cost_flow_program.py
      COMPONENT_NAME graph
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_mip.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_groups_mip.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_task_sizes_mip.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_teams_mip.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/basic_example.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/bin_packing_mip.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/linear_programming_example.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/integer_programming_example.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/mip_var_array.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/multiple_knapsack_mip.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/simple_lp_program.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/simple_mip_program.py
      COMPONENT_NAME linear_solver
    )

    if(USE_GLOP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/stigler_diet.py
        COMPONENT_NAME linear_solver
      )
    endif()

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/assignment_mb.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/bin_packing_mb.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/clone_model_mb.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/simple_lp_program_mb.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/samples/simple_mip_program_mb.py
      COMPONENT_NAME linear_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/all_different_except_zero_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_groups_sat.py
      COMPONENT_NAME sat
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_task_sizes_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assignment_teams_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/assumptions_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/binpacking_problem_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/bin_packing_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/bool_and_int_var_product_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/bool_or_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/boolean_product_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/channeling_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/clone_model_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/cp_is_fun_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/cp_sat_example.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/cumulative_variable_profile_sample_sat.py
      COMPONENT_NAME sat
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/earliness_tardiness_cost_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/interval_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/interval_relations_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/index_first_boolvar_true_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/minimal_jobshop_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/literal_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/multiple_knapsack_sat.py
      COMPONENT_NAME sat
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/non_linear_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/nqueens_sat.py
      COMPONENT_NAME sat
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/nurses_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/optional_interval_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/no_overlap_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/overlapping_intervals_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/rabbits_and_pheasants_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/ranking_circuit_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/ranking_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/reified_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/schedule_requests_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/scheduling_with_calendar_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/search_for_all_solutions_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/sequences_in_no_overlap_sample_sat.py
      COMPONENT_NAME sat
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/simple_sat_program.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/soft_constraints_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/solution_hinting_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/solve_and_print_intermediate_solutions_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/step_function_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/solve_with_time_limit_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/stop_after_n_solutions_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/samples/transitions_in_no_overlap_sample_sat.py
      COMPONENT_NAME sat
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/set_cover/samples/set_cover.py
      COMPONENT_NAME set_cover
    )
  endif()

  if(BUILD_PYTHON_TESTING)
    ortools_cxx_library(
      NAME util_python_py_solve_interrupter_testing_lib
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/ortools/util/python/py_solve_interrupter_testing.cc
        ${PROJECT_SOURCE_DIR}/ortools/util/python/py_solve_interrupter_testing.h
      LINK_LIBRARIES
        ${PROJECT_NAMESPACE}::ortools
        ${PROJECT_NAMESPACE}::util_python_py_solve_interrupter_lib
        absl::nullability
      NO_INSTALL
    )

    ortools_cxx_library(
      NAME util_python_wrappers_test_message_pybind11
      TYPE INTERFACE
      LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME util_python_gen_wrappers_test_message_pybind11
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/python/gen_wrappers_test_message_pybind11.cc
      LINK_LIBRARIES
        ${PROJECT_NAMESPACE}::ortools
        ${PROJECT_NAMESPACE}::util_python_wrappers
        absl::flags_parse
        absl::flags_usage
        absl::die_if_null
        absl::log_initialize
        absl::strings
        absl::str_format
        ${PROJECT_NAMESPACE}::ortools_test_proto
        protobuf::libprotobuf
      NO_INSTALL
    )

    add_custom_command(
      OUTPUT ${PROJECT_BINARY_DIR}/ortools/util/python/wrappers_test_message_pybind11.h
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/ortools/util/python
      COMMAND $<TARGET_FILE:util_python_gen_wrappers_test_message_pybind11> > ${PROJECT_BINARY_DIR}/ortools/util/python/wrappers_test_message_pybind11.h
      DEPENDS util_python_gen_wrappers_test_message_pybind11
      COMMENT "Generate C++ ortools/util/python/wrappers_test_message_pybind11.h"
      VERBATIM
    )

    ortools_python_extension(
      NAME solve_interrupter_testing_pybind11
      MODULE_NAME pybind_solve_interrupter_testing
      PACKAGE_DIR util/python
      SOURCES ${PROJECT_SOURCE_DIR}/ortools/util/python/pybind_solve_interrupter_testing.cc
      LINK_LIBRARIES
        ${PROJECT_NAMESPACE}::ortools
        ${PROJECT_NAMESPACE}::util_python_py_solve_interrupter_testing_lib
      TESTONLY
    )

    ortools_python_extension(
      NAME wrappers_test_message_extension_pybind11
      MODULE_NAME wrappers_test_message_extension
      PACKAGE_DIR util/python
      SOURCES
        ${PROJECT_BINARY_DIR}/ortools/util/python/wrappers_test_message_pybind11.h
        ${PROJECT_SOURCE_DIR}/ortools/util/python/wrappers_test_message_extension.cc
      LINK_LIBRARIES
        ${PROJECT_NAMESPACE}::ortools
        ${PROJECT_NAMESPACE}::util_python_wrappers_test_message_pybind11
        absl::strings
        ${PROJECT_NAMESPACE}::ortools_test_proto
        protobuf::libprotobuf
      INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
      TESTONLY
    )

    if(USE_SCIP)
      add_python_test(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/algorithms/python/knapsack_solver_test.py
        COMPONENT_NAME algorithms
      )
    endif()

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/graph/python/dag_shortest_path_test.py
      COMPONENT_NAME graph
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/init/python/init_test.py
      COMPONENT_NAME init
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/model_builder_helper_test.py
      COMPONENT_NAME linear_solver
      DISABLED
    )

    if(USE_GLOP)
      add_python_test(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/model_builder_test.py
        COMPONENT_NAME linear_solver
        DISABLED
      )
    endif()

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/pywraplp_test.py
      COMPONENT_NAME linear_solver
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/linear_solver/python/set_starting_basis_test.py
      COMPONENT_NAME linear_solver
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/python/cp_model_helper_test.py
      COMPONENT_NAME sat
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/sat/python/cp_model_test.py
      COMPONENT_NAME sat
      DISABLED
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/set_cover/python/set_cover_test.py
      COMPONENT_NAME set_cover
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/util/python/sorted_interval_list_test.py
      COMPONENT_NAME util
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/util/python/pybind_solve_interrupter_test.py
      COMPONENT_NAME util
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/util/python/solve_interrupter_test.py
      COMPONENT_NAME util
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/util/python/wrappers_test_message_test.py
      COMPONENT_NAME util
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/util/python/status_streaming_test.py
      COMPONENT_NAME util
    )
  endif()
endif()

if(USE_PDLP)
  ortools_python_extension(
    NAME pdlp_pybind11
    MODULE_NAME pdlp
    PACKAGE_DIR pdlp/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/pdlp/python/pdlp.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::status
      absl::statusor
      absl::strings
      Eigen3::Eigen
      protobuf::libprotobuf
      pybind11_native_proto_caster
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  if(BUILD_PYTHON_SAMPLES)
    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/pdlp/samples/simple_pdlp_program.py
      COMPONENT_NAME pdlp
    )
  endif()

  if(BUILD_PYTHON_TESTING)
    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/pdlp/python/pdlp_test.py
      COMPONENT_NAME pdlp
    )
  endif()
endif()

if(BUILD_SCHEDULING)
  ortools_python_extension(
    NAME rcpsp_pybind11
    MODULE_NAME rcpsp
    PACKAGE_DIR scheduling/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/scheduling/python/rcpsp.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      pybind11_native_proto_caster
      protobuf::libprotobuf
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  if(BUILD_PYTHON_TESTING)
    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/scheduling/python/rcpsp_test.py
      COMPONENT_NAME scheduling
    )
  endif()
endif()

if(BUILD_ROUTING)
  ortools_cxx_library(
    NAME constraint_solver_python_constraint_solver_doc_cc
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME routing_python_doc_cc
    TYPE INTERFACE
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools
    NO_INSTALL
  )

  ortools_python_extension(
    NAME constraint_solver_pybind11
    MODULE_NAME constraint_solver
    PACKAGE_DIR constraint_solver/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/python/constraint_solver.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::constraint_solver_python_constraint_solver_doc_cc
      absl::cleanup
      absl::function_ref
      absl::strings
      absl::time
      protobuf::libprotobuf
      pybind11_abseil::absl_casters
      pybind11_native_proto_caster
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  ortools_python_extension(
    NAME routing_pybind11
    MODULE_NAME routing
    PACKAGE_DIR routing/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/routing/python/routing.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      ${PROJECT_NAMESPACE}::routing_python_doc_cc
      protobuf::libprotobuf
      absl::algorithm_container
      absl::flat_hash_set
      absl::span
      pybind11_abseil::absl_casters
      pybind11_native_proto_caster
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
  )

  if(BUILD_PYTHON_SAMPLES)
    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/cp_is_fun_cp.py
      COMPONENT_NAME constraint_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/nqueens_cp.py
      COMPONENT_NAME constraint_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/samples/simple_cp_program.py
      COMPONENT_NAME constraint_solver
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrp_reload.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/cvrptw_break.py
      COMPONENT_NAME routing
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/simple_routing_program.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_node_max.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_tokens.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_circuit_board.py
      COMPONENT_NAME routing
      DISABLED
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_cities.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/tsp_distance_matrix.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_breaks.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_breaks_from_start.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_capacity.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_drop_nodes.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_global_span.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_initial_routes.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_items_to_deliver.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_nodes_indices.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_starts_ends.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_pickup_delivery.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_pickup_delivery_fifo.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_pickup_delivery_lifo.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_resources.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_solution_callback.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_time_windows.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_time_windows_per_vehicles.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrp_with_time_limit.py
      COMPONENT_NAME routing
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/samples/vrptw_store_solution_data.py
      COMPONENT_NAME routing
    )
  endif()

  if(BUILD_PYTHON_TESTING)
    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/python/constraint_solver_test.py
      COMPONENT_NAME constraint_solver
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/python/constraint_solver_extension_test.py
      COMPONENT_NAME constraint_solver
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/constraint_solver/python/local_search_test.py
      COMPONENT_NAME constraint_solver
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/python/routing_test.py
      COMPONENT_NAME routing
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/python/dimension_test.py
      COMPONENT_NAME routing
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/python/index_manager_test.py
      COMPONENT_NAME routing
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/routing/python/sat_test.py
      COMPONENT_NAME routing
      DISABLED
    )
  endif()
endif()

if(BUILD_MATH_OPT)
  ortools_python_copy(
    PACKAGE_DIR math_opt/elemental/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/python/cpp_elemental.pyi
  )

  ortools_python_copy(
    PACKAGE_DIR math_opt/python
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/bounded_expressions.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/callback.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/compute_infeasible_subsystem_result.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/errors.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/expressions.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/from_model.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/indicator_constraints.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/init_arguments.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/linear_constraints.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/mathopt.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/message_callback.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/model.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/model_parameters.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/normalize.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/normalized_inequality.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/objectives.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/parameters.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/quadratic_constraints.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/result.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/solution.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/solve.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/solver_resources.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/sparse_containers.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/statistics.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/variables.py
  )

  ortools_python_copy(
    PACKAGE_DIR math_opt/python/elemental
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/elemental/elemental.py
  )

  ortools_python_copy(
    PACKAGE_DIR math_opt/python/ipc
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/ipc/proto_converter.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/ipc/remote_http_solve.py
  )

  ortools_python_copy(
    PACKAGE_DIR math_opt/python/testing
    SOURCES
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/testing/compare_proto.py
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/testing/proto_matcher.py
  )

  set_property(GLOBAL APPEND PROPERTY ORTOOLS_PYTHON_PURE_DIRS "math_opt/elemental/python")
  if(CMAKE_CROSSCOMPILING AND EXISTS "${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/python/enums.py.in")
    configure_file(
      ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/python/enums.py.in
      ${PYTHON_PROJECT_DIR}/math_opt/elemental/python/enums.py
      COPYONLY
    )
  else()
    add_custom_command(
      OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/elemental/python/enums.py
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PYTHON_PROJECT_DIR}/math_opt/elemental/python
      COMMAND $<TARGET_FILE:math_opt_elemental_codegen> --binding_type=python_enums > ${PYTHON_PROJECT_DIR}/math_opt/elemental/python/enums.py
      DEPENDS math_opt_elemental_codegen
      COMMENT "Generate Python math_opt/elemental/python/enums.py"
      VERBATIM
    )
    set_property(GLOBAL APPEND PROPERTY ORTOOLS_PYTHON_GENERATED_SRCS "${PYTHON_PROJECT_DIR}/math_opt/elemental/python/enums.py")
  endif()

  ortools_python_extension(
    NAME math_opt_core_pybind11
    MODULE_NAME solver
    PACKAGE_DIR math_opt/core/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/core/python/solver.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::nullability
      absl::memory
      absl::statusor
      pybind11_abseil::import_status_module
      pybind11_abseil::status_casters
      pybind11_native_proto_caster
      protobuf::libprotobuf
      ${PROJECT_NAMESPACE}::util_python_py_solve_interrupter_lib
      absl::status_macros
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
    RPATH_ABSEIL
  )

  ortools_python_extension(
    NAME math_opt_elemental_pybind11
    MODULE_NAME cpp_elemental
    PACKAGE_DIR math_opt/elemental/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/python/elemental.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      absl::core_headers
      absl::no_destructor
      absl::flat_hash_map
      absl::flat_hash_set
      absl::check
      absl::status
      absl::status_macros
      absl::statusor
      absl::strings
      absl::span
      pybind11_native_proto_caster
      protobuf::libprotobuf
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
    NO_STUBGEN
  )

  ortools_python_extension(
    NAME math_opt_io_pybind11
    MODULE_NAME mps_converter
    PACKAGE_DIR math_opt/io/python
    SOURCES ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/python/mps_converter.cc
    LINK_LIBRARIES
      ${PROJECT_NAMESPACE}::ortools
      protobuf::libprotobuf
      pybind11_abseil::import_status_module
      pybind11_abseil::status_casters
      pybind11_native_proto_caster
    INCLUDE_DIRS $<$<BOOL:${protobuf_SOURCE_DIR}>:${protobuf_SOURCE_DIR}>
    RPATH_ABSEIL
    NO_STUBGEN
  )

  if(BUILD_PYTHON_SAMPLES)
    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/tsp.py
      COMPONENT_NAME math_opt
      DISABLED
    )

    if(USE_SCIP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/basic_example.py
        COMPONENT_NAME math_opt
      )
    endif()

    if(USE_GLOP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/linear_programming.py
        COMPONENT_NAME math_opt
      )
    endif()

    if(USE_SCIP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/integer_programming.py
        COMPONENT_NAME math_opt
      )
    endif()

    if(USE_GLOP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/advanced_linear_programming.py
        COMPONENT_NAME math_opt
      )

      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/cutting_stock.py
        COMPONENT_NAME math_opt
      )
    endif()

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/facility_lp_benders.py
      COMPONENT_NAME math_opt
      DISABLED
    )

    if(USE_PDLP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/linear_regression.py
        COMPONENT_NAME math_opt
      )
    endif()

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/smallest_circle.py
      COMPONENT_NAME math_opt
      DISABLED
    )

    if(USE_GLOP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/spillover.py
        COMPONENT_NAME math_opt
      )
    endif()

    if(USE_SCIP)
      add_python_sample(
        FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/time_indexed_scheduling.py
        COMPONENT_NAME math_opt
      )
    endif()

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/writing_seminar.py
      COMPONENT_NAME math_opt
    )

    add_python_sample(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/samples/python/hierarchical_objectives.py
      COMPONENT_NAME math_opt
      DISABLED
    )
  endif()

  if(BUILD_PYTHON_TESTING)
    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/python/enums_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/elemental/python/elemental_test.py
      COMPONENT_NAME math_opt
    )
    if(MSVC)
      set_tests_properties(python_math_opt_elemental_test PROPERTIES DISABLED TRUE)
    endif()

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/io/python/mps_converter_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/model_element_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/model_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/linear_expression_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/sparse_containers_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/solution_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/quadratic_constraints_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/indicator_constraints_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/objectives_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/model_objective_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/parameters_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/model_parameters_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/callback_test.py
      COMPONENT_NAME math_opt
      DISABLED
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/compute_infeasible_subsystem_result_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/message_callback_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/statistics_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/bounded_expressions_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/normalized_inequality_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/normalize_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/expressions_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/solver_resources_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/errors_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/init_arguments_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/testing/compare_proto_test.py
      COMPONENT_NAME math_opt
    )

    add_python_test(
      FILE_NAME ${PROJECT_SOURCE_DIR}/ortools/math_opt/python/testing/proto_matcher_test.py
      COMPONENT_NAME math_opt
    )
  endif()
endif()

set(ORTOOLS_PYTHON_TYPED_DIRS
  linear_solver/python
  sat
  sat/colab
  sat/python
)
