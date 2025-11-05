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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_LP_MODEL_SOLVE_PARAMETERS_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_LP_MODEL_SOLVE_PARAMETERS_TESTS_H_

#include <ostream>
#include <utility>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"

namespace operations_research {
namespace math_opt {

struct LpModelSolveParametersTestParameters {
  SolverType solver_type;
  // If true, we EXPECT that the solver to return a value of exactly 0.0 for
  // decisions variables >= 0 that take zero at the optimum on a very small
  // problem. In general, simplex solvers are more likely to do this, but very
  // few solvers actually guarantee this. All tests relying on this behavior are
  // brittle and we should try to eliminate them.
  bool exact_zeros;
  bool supports_duals;
  // True if the solver supports warm starts on the primal solution only.
  bool supports_primal_only_warm_starts;
  SolveParameters parameters;

  LpModelSolveParametersTestParameters(
      const SolverType solver_type, const bool exact_zeros,
      const bool supports_duals, const bool supports_primal_only_warm_starts,
      SolveParameters parameters = {})
      : solver_type(solver_type),
        exact_zeros(exact_zeros),
        supports_duals(supports_duals),
        supports_primal_only_warm_starts(supports_primal_only_warm_starts),
        parameters(std::move(parameters)) {}

  friend std::ostream& operator<<(
      std::ostream& out, const LpModelSolveParametersTestParameters& params);
};

// Parameterized test suite that validates that an implementation of
// SolverInterface::Solve() for a LP solver takes into account correctly the
// input ModelSolveParametersProto.
//
// Usage:
//
//   INSTANTIATE_TEST_SUITE_P(<Solver>LpModelSolveParametersTest,
//                            LpModelSolveParametersTest,
//                            testing::Values(
//                              LpModelSolveParametersTestParameters(
//                                SolverType::k<Solver>)));
//
class LpModelSolveParametersTest
    : public ::testing::TestWithParam<LpModelSolveParametersTestParameters> {};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_LP_MODEL_SOLVE_PARAMETERS_TESTS_H_
