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

set(ORTOOLS_PROTOS_CORE
  ortools/bop/boolean_problem.proto
  ortools/bop/bop_parameters.proto
  ortools/glop/parameters.proto
  ortools/graph/flow_problem.proto
  ortools/sat/cp_model.proto
  ortools/sat/lrat.proto
  ortools/sat/routes_support_graph.proto
  ortools/sat/sat_parameters.proto
  ortools/sat/solution_crush.proto
  ortools/set_cover/capacity.proto
  ortools/set_cover/set_cover.proto
  ortools/util/optional_boolean.proto
  ortools/linear_solver/linear_solver.proto
  ortools/util/status.proto
  ortools/linear_solver/iis.proto
)

set(ORTOOLS_PROTOS_PDLP
  ortools/pdlp/solvers.proto
  ortools/pdlp/solve_log.proto
)

set(ORTOOLS_PROTOS_PACKING
  ortools/packing/multiple_dimensions_bin_packing.proto
  ortools/packing/vector_bin_packing.proto
)

set(ORTOOLS_PROTOS_ROUTING
  ortools/constraint_solver/assignment.proto
  ortools/constraint_solver/search_limit.proto
  ortools/constraint_solver/search_stats.proto
  ortools/constraint_solver/solver_parameters.proto
  ortools/routing/enums.proto
  ortools/routing/heuristic_parameters.proto
  ortools/routing/ils.proto
  ortools/routing/parsers/capacity_planning.proto
  ortools/routing/parameters.proto
)

set(ORTOOLS_PROTOS_SCHEDULING
  ortools/scheduling/course_scheduling.proto
  ortools/scheduling/jobshop_scheduling.proto
  ortools/scheduling/rcpsp.proto
)

set(ORTOOLS_PROTOS_MATH_OPT
  ortools/math_opt/solvers/glpk.proto
  ortools/math_opt/solvers/gscip/gscip.proto
  ortools/math_opt/solvers/gurobi.proto
  ortools/math_opt/solvers/highs.proto
  ortools/math_opt/solvers/osqp.proto
  ortools/math_opt/solvers/xpress.proto
  ortools/math_opt/sparse_containers.proto
  ortools/math_opt/callback.proto
  ortools/math_opt/model.proto
  ortools/math_opt/model_update.proto
  ortools/math_opt/solution.proto
  ortools/math_opt/model_parameters.proto
  ortools/math_opt/result.proto
  ortools/math_opt/infeasible_subsystem.proto
  ortools/math_opt/parameters.proto
  ortools/service/v1/mathopt/parameters.proto
  ortools/service/v1/mathopt/solver_resources.proto
  ortools/service/v1/mathopt/sparse_containers.proto
  ortools/service/v1/mathopt/model.proto
  ortools/service/v1/mathopt/solution.proto
  ortools/service/v1/mathopt/model_parameters.proto
  ortools/service/v1/mathopt/result.proto
  ortools/service/v1/optimization.proto
  ortools/math_opt/rpc.proto
)

set(ORTOOLS_CPP_PROTOS)
if(BUILD_CORE)
  list(APPEND ORTOOLS_CPP_PROTOS ${ORTOOLS_PROTOS_CORE})
endif()
if(USE_PDLP OR BUILD_MATH_OPT)
  list(APPEND ORTOOLS_CPP_PROTOS ${ORTOOLS_PROTOS_PDLP})
endif()
if(BUILD_PACKING)
  list(APPEND ORTOOLS_CPP_PROTOS ${ORTOOLS_PROTOS_PACKING})
endif()
if(BUILD_ROUTING)
  list(APPEND ORTOOLS_CPP_PROTOS ${ORTOOLS_PROTOS_ROUTING})
endif()
if(BUILD_SCHEDULING)
  list(APPEND ORTOOLS_CPP_PROTOS ${ORTOOLS_PROTOS_SCHEDULING})
endif()
if(BUILD_MATH_OPT)
  list(APPEND ORTOOLS_CPP_PROTOS ${ORTOOLS_PROTOS_MATH_OPT})
endif()

set(ORTOOLS_PYTHON_PROTOS)
if(BUILD_CORE)
  list(APPEND ORTOOLS_PYTHON_PROTOS ${ORTOOLS_PROTOS_CORE})
endif()
if(USE_PDLP OR BUILD_MATH_OPT)
  list(APPEND ORTOOLS_PYTHON_PROTOS ${ORTOOLS_PROTOS_PDLP})
endif()
if(BUILD_PACKING)
  list(APPEND ORTOOLS_PYTHON_PROTOS ${ORTOOLS_PROTOS_PACKING})
endif()
if(BUILD_ROUTING)
  list(APPEND ORTOOLS_PYTHON_PROTOS ${ORTOOLS_PROTOS_ROUTING})
endif()
if(BUILD_SCHEDULING)
  list(APPEND ORTOOLS_PYTHON_PROTOS ${ORTOOLS_PROTOS_SCHEDULING})
endif()
if(BUILD_MATH_OPT)
  list(APPEND ORTOOLS_PYTHON_PROTOS ${ORTOOLS_PROTOS_MATH_OPT})
endif()

set(ORTOOLS_CPP_TEST_PROTOS_CORE
  ortools/util/file_util_test.proto
  ortools/util/testdata/wrappers_test_message.proto
)

set(ORTOOLS_CPP_TEST_PROTOS_MATH_OPT
  ortools/math_opt/cpp/enums_test.proto
  ortools/math_opt/storage/atomic_constraint_storage_test.proto
)

set(ORTOOLS_CPP_TEST_PROTOS)
if(BUILD_CORE)
  list(APPEND ORTOOLS_CPP_TEST_PROTOS ${ORTOOLS_CPP_TEST_PROTOS_CORE})
endif()
if(BUILD_MATH_OPT)
  list(APPEND ORTOOLS_CPP_TEST_PROTOS ${ORTOOLS_CPP_TEST_PROTOS_MATH_OPT})
endif()

set(ORTOOLS_JAVA_PROTOS_CORE
  ortools/bop/boolean_problem.proto
  ortools/bop/bop_parameters.proto
  ortools/glop/parameters.proto
  ortools/graph/flow_problem.proto
  ortools/sat/sat_parameters.proto
  ortools/util/optional_boolean.proto
  ortools/linear_solver/linear_solver.proto
  ortools/util/status.proto
  ortools/linear_solver/iis.proto
)

set(ORTOOLS_JAVA_PROTOS_PDLP
  ortools/pdlp/solvers.proto
  ortools/pdlp/solve_log.proto
)

set(ORTOOLS_JAVA_PROTOS_ROUTING
  ortools/constraint_solver/search_limit.proto
  ortools/constraint_solver/search_stats.proto
  ortools/constraint_solver/solver_parameters.proto
  ortools/routing/enums.proto
  ortools/routing/heuristic_parameters.proto
  ortools/routing/ils.proto
  ortools/routing/parsers/capacity_planning.proto
  ortools/routing/parameters.proto
)

set(ORTOOLS_JAVA_PROTOS_MATH_OPT
  ortools/math_opt/solvers/glpk.proto
  ortools/math_opt/solvers/gscip/gscip.proto
  ortools/math_opt/solvers/gurobi.proto
  ortools/math_opt/solvers/highs.proto
  ortools/math_opt/solvers/osqp.proto
  ortools/math_opt/solvers/xpress.proto
  ortools/math_opt/sparse_containers.proto
  ortools/math_opt/callback.proto
  ortools/math_opt/model.proto
  ortools/math_opt/model_update.proto
  ortools/math_opt/solution.proto
  ortools/math_opt/model_parameters.proto
  ortools/math_opt/result.proto
  ortools/math_opt/infeasible_subsystem.proto
  ortools/math_opt/parameters.proto
  ortools/math_opt/rpc.proto
)

set(ORTOOLS_JAVA_PROTOS)
if(BUILD_CORE)
  list(APPEND ORTOOLS_JAVA_PROTOS ${ORTOOLS_JAVA_PROTOS_CORE})
endif()
if(USE_PDLP OR BUILD_MATH_OPT)
  list(APPEND ORTOOLS_JAVA_PROTOS ${ORTOOLS_JAVA_PROTOS_PDLP})
endif()
if(BUILD_ROUTING)
  list(APPEND ORTOOLS_JAVA_PROTOS ${ORTOOLS_JAVA_PROTOS_ROUTING})
endif()
if(BUILD_MATH_OPT)
  list(APPEND ORTOOLS_JAVA_PROTOS ${ORTOOLS_JAVA_PROTOS_MATH_OPT})
endif()

set(ORTOOLS_DOTNET_PROTOS_CORE
  ortools/bop/boolean_problem.proto
  ortools/bop/bop_parameters.proto
  ortools/glop/parameters.proto
  ortools/graph/flow_problem.proto
  ortools/sat/cp_model.proto
  ortools/sat/lrat.proto
  ortools/sat/routes_support_graph.proto
  ortools/sat/sat_parameters.proto
  ortools/sat/solution_crush.proto
  ortools/util/optional_boolean.proto
  ortools/linear_solver/linear_solver.proto
  ortools/util/status.proto
  ortools/linear_solver/iis.proto
)

set(ORTOOLS_DOTNET_PROTOS_PDLP
  ortools/pdlp/solvers.proto
  ortools/pdlp/solve_log.proto
)

set(ORTOOLS_DOTNET_PROTOS_ROUTING
  ortools/constraint_solver/search_limit.proto
  ortools/constraint_solver/search_stats.proto
  ortools/constraint_solver/solver_parameters.proto
  ortools/routing/enums.proto
  ortools/routing/heuristic_parameters.proto
  ortools/routing/ils.proto
  ortools/routing/parsers/capacity_planning.proto
  ortools/routing/parameters.proto
)

set(ORTOOLS_DOTNET_PROTOS)
if(BUILD_CORE)
  list(APPEND ORTOOLS_DOTNET_PROTOS ${ORTOOLS_DOTNET_PROTOS_CORE})
endif()
if(USE_PDLP)
  list(APPEND ORTOOLS_DOTNET_PROTOS ${ORTOOLS_DOTNET_PROTOS_PDLP})
endif()
if(BUILD_ROUTING)
  list(APPEND ORTOOLS_DOTNET_PROTOS ${ORTOOLS_DOTNET_PROTOS_ROUTING})
endif()
