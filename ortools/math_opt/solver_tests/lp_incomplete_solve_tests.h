// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_INCOMPLETE_SOLVE_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_INCOMPLETE_SOLVE_TESTS_H_

#include <optional>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

// TODO(b/195295177): Consider splitting LpIncompleteSolveTest into simplex and
// non-simplex tests to avoid repetition and need to input irrelevant bools from
// LpIncompleteSolveTestParams when instantiating tests.

// Parameters for the LpIncompleteSolveTest suite below.
struct LpIncompleteSolveTestParams {
  LpIncompleteSolveTestParams(SolverType solver_type,
                              std::optional<LPAlgorithm> lp_algorithm,
                              bool supports_iteration_limit,
                              bool supports_initial_basis,
                              bool supports_incremental_solve,
                              bool supports_basis, bool supports_presolve,
                              bool check_primal_objective,
                              bool primal_solution_status_always_set,
                              bool dual_solution_status_always_set);

  // The tested solver.
  SolverType solver_type;

  // The tested algorithm.
  std::optional<LPAlgorithm> lp_algorithm;

  // Indicates if the solver supports iteration limit.
  bool supports_iteration_limit;

  // Indicates if the solver supports initial basis.
  bool supports_initial_basis;

  // Indicates if the solver supports incremental solves.
  bool supports_incremental_solve;

  // Indicates if the solver supports returning a basis.
  bool supports_basis;

  // Indicates if the solver supports setting the presolve emphasis.
  bool supports_presolve;

  // Indicates if we should check primal objective values.
  bool check_primal_objective;

  // Indicates if solver always sets a precise primal feasibility status
  // (i.e. never returns an unspecified status).
  bool primal_solution_status_always_set;

  // Indicates if solver always sets a precise dual feasibility status
  // (i.e. never returns an unspecified status).
  bool dual_solution_status_always_set;
};

// A suite of unit tests to show that an LP solver handles incomplete solves
// correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>LpIncompleteSolveTest,
//   LpIncompleteSolveTest,
//     testing::Values(lp_incomplete_solve_test_params));
//
// Note: If supports_presolve == True, presolve will be turned off in all tests.
class LpIncompleteSolveTest
    : public ::testing::TestWithParam<LpIncompleteSolveTestParams> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_INCOMPLETE_SOLVE_TESTS_H_
