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

if(BUILD_CXX_EXAMPLES)
  if(BUILD_ROUTING)
    ortools_cxx_library(
      NAME bzl_cc_example_cgc
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/examples/cpp/cgc.cc
        ${PROJECT_SOURCE_DIR}/examples/cpp/cgc.h
        ${PROJECT_SOURCE_DIR}/examples/cpp/cgc_data.h
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  if(BUILD_SCHEDULING)
    ortools_cxx_library(
      NAME bzl_cc_example_jobshop_sat_model
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/examples/cpp/jobshop_sat_model.cc
        ${PROJECT_SOURCE_DIR}/examples/cpp/jobshop_sat_model.h
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  ortools_cxx_library(
    NAME bzl_cc_example_print_dimacs_assignment
    TYPE INTERFACE
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/print_dimacs_assignment.h
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME bzl_cc_example_parse_dimacs_assignment
    TYPE INTERFACE
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/parse_dimacs_assignment.h
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME bzl_cc_example_fap_parser
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/examples/cpp/fap_parser.cc
      ${PROJECT_SOURCE_DIR}/examples/cpp/fap_parser.h
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_library(
    NAME bzl_cc_example_fap_model_printer
    TYPE STATIC
    SOURCES
      ${PROJECT_SOURCE_DIR}/examples/cpp/fap_model_printer.cc
      ${PROJECT_SOURCE_DIR}/examples/cpp/fap_model_printer.h
    LINK_LIBRARIES
      ortools::ortools
      bzl_cc_example_fap_parser
    NO_INSTALL
  )

  if(BUILD_ROUTING)
    ortools_cxx_library(
      NAME bzl_cc_example_fap_utilities
      TYPE STATIC
      SOURCES
        ${PROJECT_SOURCE_DIR}/examples/cpp/fap_utilities.cc
        ${PROJECT_SOURCE_DIR}/examples/cpp/fap_utilities.h
      LINK_LIBRARIES
        ortools::ortools
        bzl_cc_example_fap_parser
      NO_INSTALL
    )
  endif()

  if(BUILD_PACKING)
    ortools_cxx_binary(
      NAME bzl_cc_example_binpacking_2d_sat
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/binpacking_2d_sat.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  if(BUILD_ROUTING)
    ortools_cxx_binary(
      NAME bzl_cc_example_cgc_main
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/cgc_main.cc
      LINK_LIBRARIES
        ortools::ortools
        bzl_cc_example_cgc
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME bzl_cc_example_constraint_programming_cp
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/constraint_programming_cp.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME bzl_cc_example_costas_array_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/costas_array_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_cryptarithm_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/cryptarithm_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  if(BUILD_ROUTING)
    ortools_cxx_binary(
      NAME bzl_cc_example_dobble_ls
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/dobble_ls.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME bzl_cc_example_golomb_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/golomb_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  if(BUILD_PACKING)
    ortools_cxx_binary(
      NAME bzl_cc_example_knapsack_2d_sat
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/knapsack_2d_sat.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  if(BUILD_SCHEDULING)
    ortools_cxx_binary(
      NAME bzl_cc_example_jobshop_sat
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/jobshop_sat.cc
      LINK_LIBRARIES
        ortools::ortools
        bzl_cc_example_jobshop_sat_model
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME bzl_cc_example_magic_sequence_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/magic_sequence_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_multi_knapsack_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/multi_knapsack_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_shift_minimization_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/shift_minimization_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_weighted_tardiness_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/weighted_tardiness_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_magic_square_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/magic_square_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_network_routing_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/network_routing_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  if(BUILD_ROUTING)
    ortools_cxx_binary(
      NAME bzl_cc_example_nqueens
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/nqueens.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME bzl_cc_example_sports_scheduling_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/sports_scheduling_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  if(BUILD_ROUTING)
    ortools_cxx_binary(
      NAME bzl_cc_example_pdptw
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/pdptw.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )

    ortools_cxx_binary(
      NAME bzl_cc_example_random_tsp
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/random_tsp.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME bzl_cc_example_integer_programming
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/integer_programming.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_linear_programming
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/linear_programming.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_linear_solver_protocol_buffers
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/linear_solver_protocol_buffers.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_strawberry_fields_with_column_generation
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/strawberry_fields_with_column_generation.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_dimacs_assignment
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/dimacs_assignment.cc
    LINK_LIBRARIES
      ortools::ortools
      bzl_cc_example_parse_dimacs_assignment
      bzl_cc_example_print_dimacs_assignment
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_mps_driver
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/mps_driver.cc
    COMPILE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:Clang,AppleClang,GNU>,$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<PLATFORM_ID:Android>>>:-Wfloat-conversion>
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_linear_assignment_api
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/linear_assignment_api.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_flow_api
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/flow_api.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_max_flow
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/max_flow.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_min_cost_flow
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/min_cost_flow.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  if(BUILD_ROUTING)
    ortools_cxx_binary(
      NAME bzl_cc_example_frequency_assignment_problem
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/frequency_assignment_problem.cc
      LINK_LIBRARIES
        ortools::ortools
        bzl_cc_example_fap_model_printer
        bzl_cc_example_fap_parser
        bzl_cc_example_fap_utilities
      NO_INSTALL
    )
  endif()

  ortools_cxx_binary(
    NAME bzl_cc_example_qap_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/qap_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_slitherlink_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/slitherlink_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_uncapacitated_facility_location
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/uncapacitated_facility_location.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  ortools_cxx_binary(
    NAME bzl_cc_example_variable_intervals_sat
    SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/variable_intervals_sat.cc
    LINK_LIBRARIES ortools::ortools
    NO_INSTALL
  )

  if(USE_PDLP)
    ortools_cxx_binary(
      NAME bzl_cc_example_pdlp_solve
      SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/pdlp_solve.cc
      LINK_LIBRARIES ortools::ortools
      NO_INSTALL
    )
  endif()

  if(BUILD_CXX_TESTING)
    if(BUILD_PACKING)
      ortools_cxx_bintest(
        NAME bzl_cc_example_binpacking_2d_sat_class01_instance2_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/binpacking_2d_sat_class01_instance2_test.bintest
        ENVIRONMENT
          "BINTEST_binpacking_2d_sat=$<TARGET_FILE:bzl_cc_example_binpacking_2d_sat>"
          "BINTEST_Class_01.2bp=${PROJECT_SOURCE_DIR}/ortools/packing/testdata/Class_01.2bp"
      )
    endif()

    if(BUILD_ROUTING)
      ortools_cxx_test(
        NAME bzl_cc_example_cgc_test
        SOURCES ${PROJECT_SOURCE_DIR}/examples/cpp/cgc_test.cc
        LINK_LIBRARIES
          GTest::gtest_main
          bzl_cc_example_cgc
      )

      ortools_cxx_bintest(
        NAME bzl_cc_example_cgc_test_solution
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/cgc_test_solution.bintest
        ENVIRONMENT
          "BINTEST_cgc_main=$<TARGET_FILE:bzl_cc_example_cgc_main>"
          "BINTEST_1.in=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/cgc/1.in"
          "BINTEST_2.in=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/cgc/2.in"
          "BINTEST_3.in=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/cgc/3.in"
          "BINTEST_cgcut1.in=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/cgc/cgcut1.in"
          "BINTEST_cgcut2.in=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/cgc/cgcut2.in"
          "BINTEST_cgcut3.in=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/cgc/cgcut3.in"
      )

      ortools_cxx_bintest(
        NAME bzl_cc_example_constraint_programming_cp_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/constraint_programming_cp_test.bintest
        ENVIRONMENT "BINTEST_constraint_programming_cp=$<TARGET_FILE:bzl_cc_example_constraint_programming_cp>"
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_cc_example_costas_array_sat_model1_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/costas_array_sat_model1_test.bintest
      ENVIRONMENT "BINTEST_costas_array_sat=$<TARGET_FILE:bzl_cc_example_costas_array_sat>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_costas_array_sat_model2_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/costas_array_sat_model2_test.bintest
      ENVIRONMENT "BINTEST_costas_array_sat=$<TARGET_FILE:bzl_cc_example_costas_array_sat>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_costas_array_sat_model3_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/costas_array_sat_model3_test.bintest
      ENVIRONMENT "BINTEST_costas_array_sat=$<TARGET_FILE:bzl_cc_example_costas_array_sat>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_cryptarithm_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/cryptarithm_sat_test.bintest
      ENVIRONMENT "BINTEST_cryptarithm_sat=$<TARGET_FILE:bzl_cc_example_cryptarithm_sat>"
    )

    if(BUILD_ROUTING)
      ortools_cxx_bintest(
        NAME bzl_cc_example_dobble_ls_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/dobble_ls_test.bintest
        ENVIRONMENT "BINTEST_dobble_ls=$<TARGET_FILE:bzl_cc_example_dobble_ls>"
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_cc_example_golomb_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/golomb_sat_test.bintest
      ENVIRONMENT "BINTEST_golomb_sat=$<TARGET_FILE:bzl_cc_example_golomb_sat>"
    )

    if(BUILD_PACKING)
      ortools_cxx_bintest(
        NAME bzl_cc_example_knapsack_2d_sat_class01_instance2_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/knapsack_2d_sat_class01_instance2_test.bintest
        ENVIRONMENT
          "BINTEST_knapsack_2d_sat=$<TARGET_FILE:bzl_cc_example_knapsack_2d_sat>"
          "BINTEST_Class_01.2bp=${PROJECT_SOURCE_DIR}/ortools/packing/testdata/Class_01.2bp"
      )
    endif()

    if(BUILD_SCHEDULING)
      ortools_cxx_bintest(
        NAME bzl_cc_example_jobshop_sat_ft06
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/jobshop_sat_ft06.bintest
        ENVIRONMENT
          "BINTEST_jobshop_sat=$<TARGET_FILE:bzl_cc_example_jobshop_sat>"
          "BINTEST_ft06=${PROJECT_SOURCE_DIR}/ortools/scheduling/testdata/ft06"
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_cc_example_magic_sequence_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/magic_sequence_sat_test.bintest
      ENVIRONMENT "BINTEST_magic_sequence_sat=$<TARGET_FILE:bzl_cc_example_magic_sequence_sat>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_multi_knapsack_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/multi_knapsack_sat_test.bintest
      ENVIRONMENT "BINTEST_multi_knapsack_sat=$<TARGET_FILE:bzl_cc_example_multi_knapsack_sat>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_shift_minimization_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/shift_minimization_sat_test.bintest
      ENVIRONMENT
        "BINTEST_shift_minimization_sat=$<TARGET_FILE:bzl_cc_example_shift_minimization_sat>"
        "BINTEST_shift_minimization.dat=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/shift_minimization.dat"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_weighted_tardiness_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/weighted_tardiness_sat_test.bintest
      ENVIRONMENT
        "BINTEST_weighted_tardiness_sat=$<TARGET_FILE:bzl_cc_example_weighted_tardiness_sat>"
        "BINTEST_wt40.txt=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/wt40.txt"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_magic_square_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/magic_square_sat_test.bintest
      ENVIRONMENT "BINTEST_magic_square_sat=$<TARGET_FILE:bzl_cc_example_magic_square_sat>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_network_routing_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/network_routing_sat_test.bintest
      ENVIRONMENT "BINTEST_network_routing_sat=$<TARGET_FILE:bzl_cc_example_network_routing_sat>"
    )

    if(BUILD_ROUTING)
      ortools_cxx_bintest(
        NAME bzl_cc_example_nqueens_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/nqueens_test.bintest
        ENVIRONMENT "BINTEST_nqueens=$<TARGET_FILE:bzl_cc_example_nqueens>"
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_cc_example_sports_scheduling_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/sports_scheduling_sat_test.bintest
      ENVIRONMENT "BINTEST_sports_scheduling_sat=$<TARGET_FILE:bzl_cc_example_sports_scheduling_sat>"
    )

    if(BUILD_ROUTING)
      ortools_cxx_bintest(
        NAME bzl_cc_example_pdptw_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/pdptw_test.bintest
        ENVIRONMENT
          "BINTEST_pdptw=$<TARGET_FILE:bzl_cc_example_pdptw>"
          "BINTEST_lc102.txt=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/lc102.txt"
      )

      ortools_cxx_bintest(
        NAME bzl_cc_example_pdptw_non_homogenous_fleet_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/pdptw_non_homogenous_fleet_test.bintest
        ENVIRONMENT
          "BINTEST_pdptw=$<TARGET_FILE:bzl_cc_example_pdptw>"
          "BINTEST_lc102.txt=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/lc102.txt"
        DISABLED
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_cc_example_integer_programming_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/integer_programming_test.bintest
      ENVIRONMENT "BINTEST_integer_programming=$<TARGET_FILE:bzl_cc_example_integer_programming>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_linear_programming_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/linear_programming_test.bintest
      ENVIRONMENT "BINTEST_linear_programming=$<TARGET_FILE:bzl_cc_example_linear_programming>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_linear_solver_protocol_buffers_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/linear_solver_protocol_buffers_test.bintest
      ENVIRONMENT "BINTEST_linear_solver_protocol_buffers=$<TARGET_FILE:bzl_cc_example_linear_solver_protocol_buffers>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_strawberry_fields_with_column_generation_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/strawberry_fields_with_column_generation_test.bintest
      ENVIRONMENT "BINTEST_strawberry_fields_with_column_generation=$<TARGET_FILE:bzl_cc_example_strawberry_fields_with_column_generation>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_dimacs_assignment_min_cost_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/dimacs_assignment_min_cost_test.bintest
      ENVIRONMENT
        "BINTEST_dimacs_assignment=$<TARGET_FILE:bzl_cc_example_dimacs_assignment>"
        "BINTEST_dimacs_example.txt=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/dimacs_example.txt"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_dimacs_assignment_max_cost_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/dimacs_assignment_max_cost_test.bintest
      ENVIRONMENT
        "BINTEST_dimacs_assignment=$<TARGET_FILE:bzl_cc_example_dimacs_assignment>"
        "BINTEST_dimacs_example.txt=${PROJECT_SOURCE_DIR}/examples/cpp/testdata/dimacs_example.txt"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_mps_driver_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/mps_driver_test.bintest
      ENVIRONMENT
        "BINTEST_mps_driver=$<TARGET_FILE:bzl_cc_example_mps_driver>"
        "BINTEST_maximization.mps=${PROJECT_SOURCE_DIR}/ortools/linear_solver/testdata/maximization.mps"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_linear_assignment_api_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/linear_assignment_api_test.bintest
      ENVIRONMENT "BINTEST_linear_assignment_api=$<TARGET_FILE:bzl_cc_example_linear_assignment_api>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_flow_api_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/flow_api_test.bintest
      ENVIRONMENT "BINTEST_flow_api=$<TARGET_FILE:bzl_cc_example_flow_api>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_max_flow_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/max_flow_test.bintest
      ENVIRONMENT "BINTEST_max_flow=$<TARGET_FILE:bzl_cc_example_max_flow>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_min_cost_flow_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/min_cost_flow_test.bintest
      ENVIRONMENT "BINTEST_min_cost_flow=$<TARGET_FILE:bzl_cc_example_min_cost_flow>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_slitherlink_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/slitherlink_sat_test.bintest
      ENVIRONMENT "BINTEST_slitherlink_sat=$<TARGET_FILE:bzl_cc_example_slitherlink_sat>"
    )

    ortools_cxx_bintest(
      NAME bzl_cc_example_variable_intervals_sat_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/cpp/variable_intervals_sat_test.bintest
      ENVIRONMENT "BINTEST_variable_intervals_sat=$<TARGET_FILE:bzl_cc_example_variable_intervals_sat>"
    )
  endif()
endif()

if(BUILD_PYTHON_EXAMPLES AND NOT WIN32)
  add_python_binary(
    NAME bzl_py_example_arc_flow_cutting_stock_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/arc_flow_cutting_stock_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_assignment_with_constraints_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/assignment_with_constraints_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_balance_group_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/balance_group_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_bus_driver_scheduling_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/bus_driver_scheduling_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_bus_driver_scheduling_flow_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/bus_driver_scheduling_flow_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_car_sequencing_optimization_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/car_sequencing_optimization_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_chemical_balance_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/chemical_balance_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_clustering_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/clustering_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_cover_rectangle_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/cover_rectangle_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_flexible_job_shop_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/flexible_job_shop_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_gate_scheduling_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/gate_scheduling_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_golomb_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/golomb_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_hidato_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/hidato_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_jobshop_ft06_distance_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/jobshop_ft06_distance_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_jobshop_ft06_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/jobshop_ft06_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_jobshop_with_maintenance_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/jobshop_with_maintenance_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_knapsack_2d_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/knapsack_2d_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_line_balancing_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/line_balancing_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_maximize_combinations_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/maximize_combinations_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_maze_escape_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/maze_escape_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_music_playlist_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/music_playlist_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_no_wait_baking_scheduling_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/no_wait_baking_scheduling_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_pell_equation_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/pell_equation_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_pentominoes_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/pentominoes_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_prize_collecting_tsp_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_tsp_sat.py
  )

  if(BUILD_ROUTING)
    add_python_binary(
      NAME bzl_py_example_prize_collecting_tsp_py3
      FILE ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_tsp.py
    )

    add_python_binary(
      NAME bzl_py_example_prize_collecting_vrp_py3
      FILE ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_vrp.py
    )
  endif()

  add_python_binary(
    NAME bzl_py_example_prize_collecting_vrp_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_vrp_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_qubo_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/qubo_sat.py
  )

  if(BUILD_SCHEDULING)
    add_python_binary(
      NAME bzl_py_example_rcpsp_sat_py3
      FILE ${PROJECT_SOURCE_DIR}/examples/python/rcpsp_sat.py
    )
  endif()

  add_python_binary(
    NAME bzl_py_example_shift_scheduling_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/shift_scheduling_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_single_machine_scheduling_with_setup_release_due_dates_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/single_machine_scheduling_with_setup_release_due_dates_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_spread_robots_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/spread_robots_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_steel_mill_slab_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/steel_mill_slab_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_sudoku_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/sudoku_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_task_allocation_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/task_allocation_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_tree_ensemble_optimization_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/tree_ensemble_optimization_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_tasks_and_workers_assignment_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/tasks_and_workers_assignment_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_test_scheduling_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/test_scheduling_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_tsp_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/tsp_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_vendor_scheduling_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/vendor_scheduling_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_wedding_optimal_chart_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/wedding_optimal_chart_sat.py
  )

  add_python_binary(
    NAME bzl_py_example_zebra_sat_py3
    FILE ${PROJECT_SOURCE_DIR}/examples/python/zebra_sat.py
  )

  if(BUILD_MATH_OPT)
    add_python_binary(
      NAME bzl_py_service_example_solve_math_opt_model_via_http
      FILE ${PROJECT_SOURCE_DIR}/examples/service/solve_math_opt_model_via_http.py
    )
  endif()

  if(BUILD_PYTHON_TESTING)
    ortools_cxx_bintest(
      NAME bzl_py_example_assignment_with_constraints_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/assignment_with_constraints_sat_py_test.bintest
      ENVIRONMENT "BINTEST_assignment_with_constraints_sat_py3=$<TARGET_FILE:bzl_py_example_assignment_with_constraints_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_balance_group_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/balance_group_sat_py_test.bintest
      ENVIRONMENT "BINTEST_balance_group_sat_py3=$<TARGET_FILE:bzl_py_example_balance_group_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_bus_driver_scheduling_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/bus_driver_scheduling_sat_py_test.bintest
      ENVIRONMENT "BINTEST_bus_driver_scheduling_sat_py3=$<TARGET_FILE:bzl_py_example_bus_driver_scheduling_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_bus_driver_scheduling_flow_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/bus_driver_scheduling_flow_sat_py_test.bintest
      ENVIRONMENT "BINTEST_bus_driver_scheduling_flow_sat_py3=$<TARGET_FILE:bzl_py_example_bus_driver_scheduling_flow_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_car_sequencing_optimization_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/car_sequencing_optimization_sat_py_test.bintest
      ENVIRONMENT "BINTEST_car_sequencing_optimization_sat_py3=$<TARGET_FILE:bzl_py_example_car_sequencing_optimization_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_chemical_balance_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/chemical_balance_sat_py_test.bintest
      ENVIRONMENT "BINTEST_chemical_balance_sat_py3=$<TARGET_FILE:bzl_py_example_chemical_balance_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_clustering_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/clustering_sat_py_test.bintest
      ENVIRONMENT "BINTEST_clustering_sat_py3=$<TARGET_FILE:bzl_py_example_clustering_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_cover_rectangle_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/cover_rectangle_sat_py_test.bintest
      ENVIRONMENT "BINTEST_cover_rectangle_sat_py3=$<TARGET_FILE:bzl_py_example_cover_rectangle_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_flexible_job_shop_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/flexible_job_shop_sat_py_test.bintest
      ENVIRONMENT "BINTEST_flexible_job_shop_sat_py3=$<TARGET_FILE:bzl_py_example_flexible_job_shop_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_gate_scheduling_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/gate_scheduling_sat_py_test.bintest
      ENVIRONMENT "BINTEST_gate_scheduling_sat_py3=$<TARGET_FILE:bzl_py_example_gate_scheduling_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_golomb_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/golomb_sat_py_test.bintest
      ENVIRONMENT "BINTEST_golomb_sat_py3=$<TARGET_FILE:bzl_py_example_golomb_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_hidato_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/hidato_sat_py_test.bintest
      ENVIRONMENT "BINTEST_hidato_sat_py3=$<TARGET_FILE:bzl_py_example_hidato_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_jobshop_ft06_distance_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/jobshop_ft06_distance_sat_py_test.bintest
      ENVIRONMENT "BINTEST_jobshop_ft06_distance_sat_py3=$<TARGET_FILE:bzl_py_example_jobshop_ft06_distance_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_jobshop_ft06_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/jobshop_ft06_sat_py_test.bintest
      ENVIRONMENT "BINTEST_jobshop_ft06_sat_py3=$<TARGET_FILE:bzl_py_example_jobshop_ft06_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_jobshop_with_maintenance_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/jobshop_with_maintenance_sat_py_test.bintest
      ENVIRONMENT "BINTEST_jobshop_with_maintenance_sat_py3=$<TARGET_FILE:bzl_py_example_jobshop_with_maintenance_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_knapsack_2d_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/knapsack_2d_sat_py_test.bintest
      ENVIRONMENT "BINTEST_knapsack_2d_sat_py3=$<TARGET_FILE:bzl_py_example_knapsack_2d_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_line_balancing_sat_salbp_20_1_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/line_balancing_sat_salbp_20_1_py_test.bintest
      ENVIRONMENT
        "BINTEST_line_balancing_sat_py3=$<TARGET_FILE:bzl_py_example_line_balancing_sat_py3>"
        "BINTEST_salbp_20_1.alb=${PROJECT_SOURCE_DIR}/examples/python/testdata/salbp_20_1.alb"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_maximize_combinations_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/maximize_combinations_sat_py_test.bintest
      ENVIRONMENT "BINTEST_maximize_combinations_sat_py3=$<TARGET_FILE:bzl_py_example_maximize_combinations_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_maze_escape_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/maze_escape_sat_py_test.bintest
      ENVIRONMENT "BINTEST_maze_escape_sat_py3=$<TARGET_FILE:bzl_py_example_maze_escape_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_music_playlist_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/music_playlist_sat_py_test.bintest
      ENVIRONMENT "BINTEST_music_playlist_sat_py3=$<TARGET_FILE:bzl_py_example_music_playlist_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_no_wait_baking_scheduling_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/no_wait_baking_scheduling_sat_py_test.bintest
      ENVIRONMENT "BINTEST_no_wait_baking_scheduling_sat_py3=$<TARGET_FILE:bzl_py_example_no_wait_baking_scheduling_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_pell_equation_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/pell_equation_sat_py_test.bintest
      ENVIRONMENT "BINTEST_pell_equation_sat_py3=$<TARGET_FILE:bzl_py_example_pell_equation_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_pentominoes_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/pentominoes_sat_py_test.bintest
      ENVIRONMENT "BINTEST_pentominoes_sat_py3=$<TARGET_FILE:bzl_py_example_pentominoes_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_prize_collecting_tsp_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_tsp_sat_py_test.bintest
      ENVIRONMENT "BINTEST_prize_collecting_tsp_sat_py3=$<TARGET_FILE:bzl_py_example_prize_collecting_tsp_sat_py3>"
      DISABLED
    )

    if(BUILD_ROUTING)
      ortools_cxx_bintest(
        NAME bzl_py_example_prize_collecting_tsp_py_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_tsp_py_test.bintest
        ENVIRONMENT "BINTEST_prize_collecting_tsp_py3=$<TARGET_FILE:bzl_py_example_prize_collecting_tsp_py3>"
      )

      ortools_cxx_bintest(
        NAME bzl_py_example_prize_collecting_vrp_py_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_vrp_py_test.bintest
        ENVIRONMENT "BINTEST_prize_collecting_vrp_py3=$<TARGET_FILE:bzl_py_example_prize_collecting_vrp_py3>"
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_py_example_prize_collecting_vrp_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/prize_collecting_vrp_sat_py_test.bintest
      ENVIRONMENT "BINTEST_prize_collecting_vrp_sat_py3=$<TARGET_FILE:bzl_py_example_prize_collecting_vrp_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_qubo_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/qubo_sat_py_test.bintest
      ENVIRONMENT "BINTEST_qubo_sat_py3=$<TARGET_FILE:bzl_py_example_qubo_sat_py3>"
      DISABLED
    )

    if(BUILD_SCHEDULING)
      ortools_cxx_bintest(
        NAME bzl_py_example_rcpsp_sat_c1510_1_py_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/rcpsp_sat_c1510_1_py_test.bintest
        ENVIRONMENT
          "BINTEST_rcpsp_sat_py3=$<TARGET_FILE:bzl_py_example_rcpsp_sat_py3>"
          "BINTEST_c1510_1.mm.txt=${PROJECT_SOURCE_DIR}/ortools/scheduling/testdata/c1510_1.mm.txt"
      )

      ortools_cxx_bintest(
        NAME bzl_py_example_rcpsp_sat_j301_1_py_test
        SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/rcpsp_sat_j301_1_py_test.bintest
        ENVIRONMENT
          "BINTEST_rcpsp_sat_py3=$<TARGET_FILE:bzl_py_example_rcpsp_sat_py3>"
          "BINTEST_j301_1.sm=${PROJECT_SOURCE_DIR}/ortools/scheduling/testdata/j301_1.sm"
      )
    endif()

    ortools_cxx_bintest(
      NAME bzl_py_example_shift_scheduling_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/shift_scheduling_sat_py_test.bintest
      ENVIRONMENT "BINTEST_shift_scheduling_sat_py3=$<TARGET_FILE:bzl_py_example_shift_scheduling_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_single_machine_scheduling_with_setup_release_due_dates_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/single_machine_scheduling_with_setup_release_due_dates_sat_py_test.bintest
      ENVIRONMENT "BINTEST_single_machine_scheduling_with_setup_release_due_dates_sat_py3=$<TARGET_FILE:bzl_py_example_single_machine_scheduling_with_setup_release_due_dates_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_spread_robots_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/spread_robots_sat_py_test.bintest
      ENVIRONMENT "BINTEST_spread_robots_sat_py3=$<TARGET_FILE:bzl_py_example_spread_robots_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_steel_mill_slab_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/steel_mill_slab_sat_py_test.bintest
      ENVIRONMENT "BINTEST_steel_mill_slab_sat_py3=$<TARGET_FILE:bzl_py_example_steel_mill_slab_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_sudoku_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/sudoku_sat_py_test.bintest
      ENVIRONMENT "BINTEST_sudoku_sat_py3=$<TARGET_FILE:bzl_py_example_sudoku_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_task_allocation_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/task_allocation_sat_py_test.bintest
      ENVIRONMENT "BINTEST_task_allocation_sat_py3=$<TARGET_FILE:bzl_py_example_task_allocation_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_tree_ensemble_optimization_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/tree_ensemble_optimization_sat_py_test.bintest
      ENVIRONMENT "BINTEST_tree_ensemble_optimization_sat_py3=$<TARGET_FILE:bzl_py_example_tree_ensemble_optimization_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_tasks_and_workers_assignment_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/tasks_and_workers_assignment_sat_py_test.bintest
      ENVIRONMENT "BINTEST_tasks_and_workers_assignment_sat_py3=$<TARGET_FILE:bzl_py_example_tasks_and_workers_assignment_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_test_scheduling_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/test_scheduling_sat_py_test.bintest
      ENVIRONMENT "BINTEST_test_scheduling_sat_py3=$<TARGET_FILE:bzl_py_example_test_scheduling_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_tsp_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/tsp_sat_py_test.bintest
      ENVIRONMENT "BINTEST_tsp_sat_py3=$<TARGET_FILE:bzl_py_example_tsp_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_vendor_scheduling_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/vendor_scheduling_sat_py_test.bintest
      ENVIRONMENT "BINTEST_vendor_scheduling_sat_py3=$<TARGET_FILE:bzl_py_example_vendor_scheduling_sat_py3>"
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_wedding_optimal_chart_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/wedding_optimal_chart_sat_py_test.bintest
      ENVIRONMENT "BINTEST_wedding_optimal_chart_sat_py3=$<TARGET_FILE:bzl_py_example_wedding_optimal_chart_sat_py3>"
      DISABLED
    )

    ortools_cxx_bintest(
      NAME bzl_py_example_zebra_sat_py_test
      SCRIPT ${PROJECT_SOURCE_DIR}/examples/python/zebra_sat_py_test.bintest
      ENVIRONMENT "BINTEST_zebra_sat_py3=$<TARGET_FILE:bzl_py_example_zebra_sat_py3>"
    )
  endif()
endif()
