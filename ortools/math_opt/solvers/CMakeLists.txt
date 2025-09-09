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

set(NAME ${PROJECT_NAME}_math_opt_solvers)
add_library(${NAME} OBJECT)

file(GLOB_RECURSE _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")

if(NOT USE_GLPK)
  list(FILTER _SRCS EXCLUDE REGEX "/glpk/")
  list(FILTER _SRCS EXCLUDE REGEX "/glpk_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/glpk_.*.cc$")
endif()
if(NOT USE_GUROBI)
  list(FILTER _SRCS EXCLUDE REGEX "/gurobi/")
  list(FILTER _SRCS EXCLUDE REGEX "/gurobi_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/gurobi_.*.cc$")
endif()
if(NOT USE_HIGHS)
  list(FILTER _SRCS EXCLUDE REGEX "/highs_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/highs_.*.cc$")
endif()
if(NOT USE_PDLP)
  list(FILTER _SRCS EXCLUDE REGEX "/pdlp_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/pdlp_.*.cc$")
endif()
if(NOT USE_SCIP)
  list(FILTER _SRCS EXCLUDE REGEX "/gscip/")
  list(FILTER _SRCS EXCLUDE REGEX "/gscip_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/gscip_.*.cc$")
else()
  list(FILTER _SRCS EXCLUDE REGEX "/gscip/bugs/")
  list(FILTER _SRCS EXCLUDE REGEX "/gscip/gscip_testing")
endif()
if(NOT USE_XPRESS)
  list(FILTER _SRCS EXCLUDE REGEX "/xpress/")
  list(FILTER _SRCS EXCLUDE REGEX "/xpress_.*.h$")
  list(FILTER _SRCS EXCLUDE REGEX "/xpress_.*.cc$")
endif()

target_sources(${NAME} PRIVATE ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::strings
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
  $<$<BOOL:${USE_HIGHS}>:highs::highs>
  $<$<BOOL:${USE_PDLP}>:Eigen3::Eigen>
  $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
  ${PROJECT_NAMESPACE}::math_opt_proto)

if(USE_SCIP)
  ortools_cxx_test(
    NAME
      math_opt_solvers_gscip_solver_test
    SOURCES
      "gscip_solver_test.cc"
    LINK_LIBRARIES
      ortools::base_gmock
      GTest::gmock_main
      absl::status
      ortools::math_opt_matchers
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_invalid_input_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_multiple_solutions_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_mip_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qc_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_second_order_cone_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
  )
endif()

if(USE_GLOP)
  ortools_cxx_test(
    NAME
      math_opt_solvers_glop_solver_test
    SOURCES
      "glop_solver_test.cc"
    LINK_LIBRARIES
      ortools::base_gmock
      GTest::gmock_main
      absl::status
      ortools::math_opt_matchers
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_invalid_input_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_incomplete_solve_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_initial_basis_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qc_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_second_order_cone_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
  )
endif()

ortools_cxx_test(
  NAME
    math_opt_solvers_cp_sat_solver_test
  SOURCES
    "cp_sat_solver_test.cc"
  LINK_LIBRARIES
    ortools::base_gmock
    GTest::gmock_main
    absl::status
    ortools::math_opt_matchers
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_invalid_input_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_model_solve_parameters_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_multiple_solutions_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_parameter_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_mip_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qc_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qp_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_second_order_cone_tests>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
)
# This test is too long so we currently disable it.
set_tests_properties(cxx_math_opt_solvers_cp_sat_solver_test
  PROPERTIES DISABLED TRUE)

ortools_cxx_test(
  NAME
    math_opt_solvers_message_callback_data_test
  SOURCES
    "message_callback_data_test.cc"
  LINK_LIBRARIES
    GTest::gmock_main
    absl::cleanup
    absl::synchronization
)

if(USE_PDLP)
  ortools_cxx_test(
    NAME
      math_opt_solvers_pdlp_solver_test
    SOURCES
      "pdlp_solver_test.cc"
    LINK_LIBRARIES
      ortools::base_gmock
      GTest::gmock_main
      absl::status
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_invalid_input_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_incomplete_solve_tests>"
      #"$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_initial_basis_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qc_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_second_order_cone_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
  )
endif()

if(USE_GLPK)
  ortools_cxx_test(
    NAME
      math_opt_solvers_glpk_solver_test
    SOURCES
      "glpk_solver_test.cc"
    LINK_LIBRARIES
      ortools::base_gmock
      GTest::gmock_main
      absl::status
      absl::time
      ortools::math_opt_matchers
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_invalid_input_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_incomplete_solve_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_mip_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qc_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_second_order_cone_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
  )
endif()

if(USE_HIGHS)
  ortools_cxx_test(
    NAME
      math_opt_solvers_highs_solver_test
    SOURCES
      "highs_solver_test.cc"
    LINK_LIBRARIES
      ortools::base_gmock
      GTest::gmock_main
      absl::status
      ortools::math_opt_matchers
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_parameter_tests>"
      #"$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
      #"$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_incomplete_solve_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_mip_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
  )
endif()

if(USE_XPRESS)
  ortools_cxx_test(
    NAME
      math_opt_solvers_xpress_solver_test
    SOURCES
      "xpress_solver_test.cc"
    LINK_LIBRARIES
      ortools::base_gmock
      GTest::gmock_main
      absl::status
      ortools::math_opt_matchers
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_callback_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_invalid_input_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_generic_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_infeasible_subsystem_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_ip_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_logical_constraint_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_incomplete_solve_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_model_solve_parameters_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_parameter_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_lp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_mip_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_multi_objective_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_status_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qp_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_second_order_cone_tests>"
      "$<LINK_LIBRARY:WHOLE_ARCHIVE,ortools::math_opt_qc_tests>"
  )
endif()
