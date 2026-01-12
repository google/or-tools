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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_LP_PARAMETER_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_LP_PARAMETER_TESTS_H_

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

// Parameters for the LpParameterTest suite below.
struct LpParameterTestParams {
  LpParameterTestParams(
      const SolverType solver_type, const bool supports_simplex,
      const bool supports_barrier, const bool supports_first_order,
      const bool supports_random_seed, const bool supports_presolve,
      const bool supports_cutoff, const bool supports_objective_limit,
      const bool supports_best_bound_limit, const bool reports_limits)
      : solver_type(solver_type),
        supports_simplex(supports_simplex),
        supports_barrier(supports_barrier),
        supports_first_order(supports_first_order),
        supports_random_seed(supports_random_seed),
        supports_presolve(supports_presolve),
        supports_cutoff(supports_cutoff),
        supports_objective_limit(supports_objective_limit),
        supports_best_bound_limit(supports_best_bound_limit),
        reports_limits(reports_limits) {}

  // The tested solver.
  SolverType solver_type;

  // Indicates if the solver supports simplex as an algorithm (primal and dual).
  bool supports_simplex;

  // Indicates if the solver supports barrier as an algorithm.
  bool supports_barrier;

  // Indicates if the solver supports first-order methods.
  bool supports_first_order;

  // Indicates if the solver supports setting the random seed.
  bool supports_random_seed;

  // Indicates if the solver supports setting the presolve emphasis.
  bool supports_presolve;

  // Indicates if the solver supports a cutoff value.
  bool supports_cutoff;

  // Indicates if the solver supports setting a limit on the primal objective.
  bool supports_objective_limit;

  // Indicates if the solver supports setting a limit on the best bound.
  bool supports_best_bound_limit;

  // Indicates if the SolveResult returned will say which limit was reached.
  bool reports_limits;
};

// A suite of unit tests to show that an LP solver handles parameters correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>LpParameterTest, LpParameterTest,
//     testing::Values(lp_parameter_test_params));
class LpParameterTest : public ::testing::TestWithParam<LpParameterTestParams> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  bool SupportsSimplex() const { return GetParam().supports_simplex; }
  bool SupportsBarrier() const { return GetParam().supports_barrier; }
  bool SupportsFirstOrder() const { return GetParam().supports_first_order; }
  bool SupportsRandomSeed() const { return GetParam().supports_random_seed; }
  bool SupportsPresolve() const { return GetParam().supports_presolve; }
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_LP_PARAMETER_TESTS_H_
