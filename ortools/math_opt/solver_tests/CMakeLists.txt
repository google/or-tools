# Copyright 2010-2024 Google LLC
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

set(_PREFIX math_opt)

ortools_cxx_library(
  NAME
    ${_PREFIX}_base_solver_test
  SOURCES
    "base_solver_test.cc"
    "base_solver_test.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_callback_tests
  SOURCES
    "callback_tests.cc"
    "callback_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    absl::status
    absl::strings
    ortools::math_opt_matchers
    ortools::math_opt_base_solver_test
    ortools::math_opt_test_models
  TESTING
)


ortools_cxx_library(
  NAME
    ${_PREFIX}_status_tests
  SOURCES
    "status_tests.cc"
    "status_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_test_models
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_lp_tests
  SOURCES
    "lp_tests.cc"
    "lp_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_base_solver_test
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_lp_incomplete_solve_tests
  SOURCES
    "lp_incomplete_solve_tests.cc"
    "lp_incomplete_solve_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_test_models
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_invalid_input_tests
  SOURCES
    "invalid_input_tests.cc"
    "invalid_input_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_base_solver_test
  TESTING
)

# In CMake or-tools is linked with all enable solvers so this test won't work.
#ortools_cxx_test(
#  NAME
#    ${_PREFIX}_unregistered_solver_test
#  SOURCES
#    "unregistered_solver_test.cc"
#  LINK_LIBRARIES
#    GTest::gmock
#    GTest::gmock_main
#)

ortools_cxx_library(
  NAME
    ${_PREFIX}_mip_tests
  SOURCES
    "mip_tests.cc"
    "mip_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    absl::status
    ortools::math_opt_matchers
    ortools::math_opt_base_solver_test
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_ip_model_solve_parameters_tests
  SOURCES
    "ip_model_solve_parameters_tests.cc"
    "ip_model_solve_parameters_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_base_solver_test
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_ip_multiple_solutions_tests
  SOURCES
    "ip_multiple_solutions_tests.cc"
    "ip_multiple_solutions_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::strings
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_lp_model_solve_parameters_tests
  SOURCES
    "lp_model_solve_parameters_tests.cc"
    "lp_model_solve_parameters_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_base_solver_test
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_lp_parameter_tests
  SOURCES
    "lp_parameter_tests.cc"
    "lp_parameter_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    absl::status
    absl::strings
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_lp_initial_basis_tests
  SOURCES
    "lp_initial_basis_tests.cc"
    "lp_initial_basis_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    absl::status
    ortools::math_opt_base_solver_test
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_ip_parameter_tests
  SOURCES
    "ip_parameter_tests.cc"
    "ip_parameter_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_test_models
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_multi_objective_tests
  SOURCES
    "multi_objective_tests.cc"
    "multi_objective_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_qp_tests
  SOURCES
    "qp_tests.cc"
    "qp_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_qc_tests
  SOURCES
    "qc_tests.cc"
    "qc_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_second_order_cone_tests
  SOURCES
    "second_order_cone_tests.cc"
    "second_order_cone_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_logical_constraint_tests
  SOURCES
    "logical_constraint_tests.cc"
    "logical_constraint_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_test_models
  SOURCES
    "test_models.cc"
    "test_models.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    absl::log
    absl::strings
  TESTING
)

ortools_cxx_test(
  NAME
    ${_PREFIX}_test_models_test
  SOURCES
    "test_models_test.cc"
  LINK_LIBRARIES
    GTest::gmock
    GTest::gmock_main
    ortools::math_opt_test_models
    ortools::math_opt_matchers
    #ortools::math_opt_glop_solver
    #ortools::math_opt_gscipt_solver
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_generic_tests
  SOURCES
    "generic_tests.cc"
    "generic_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    ortools::math_opt_matchers
    ortools::math_opt_test_models
  TESTING
)

ortools_cxx_library(
  NAME
    ${_PREFIX}_infeasible_subsystem_tests
  SOURCES
    "infeasible_subsystem_tests.cc"
    "infeasible_subsystem_tests.h"
  TYPE
    STATIC
  LINK_LIBRARIES
    GTest::gmock
    absl::log
    absl::status
    absl::strings
    absl::time
    ortools::math_opt_matchers
  TESTING
)
