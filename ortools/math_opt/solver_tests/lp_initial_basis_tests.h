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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_INITIAL_BASIS_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_INITIAL_BASIS_TESTS_H_

#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"

namespace operations_research {
namespace math_opt {

// A suite of unit tests to show that an LP solver handles basis start
// correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>LpBasisStartTest, LpBasisStartTest,
//     testing::Values(SolverType::k<Solver>));
class LpBasisStartTest : public BaseSolverTest {
 protected:
  LpBasisStartTest();
  SolveStats SolveWithWarmStart(bool is_maximize, bool starting_basis_max_opt);
  SolveStats RoundTripSolve();

  int SetUpVariableBoundBoxModel();
  int SetUpConstraintBoxModel();
  int SetUpRangedConstraintBoxModel();
  int SetUpBasicRangedConstraintModel();
  int SetUpUnboundedVariablesAndConstraintsModel();
  int SetUpFixedVariablesModel();
  int SetUpEqualitiesModel();

  Model model_;
  LinearExpression objective_expression_;

  Basis max_optimal_basis_;
  Basis min_optimal_basis_;

  const SolveParameters params_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_INITIAL_BASIS_TESTS_H_
