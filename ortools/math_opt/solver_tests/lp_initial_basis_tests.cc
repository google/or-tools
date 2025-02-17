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

#include "ortools/math_opt/solver_tests/lp_initial_basis_tests.h"

#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solution.pb.h"

namespace operations_research {
namespace math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

// Set threads=1 so that the underlying solver doesn't use an ensemble of LP
// algorithms.
LpBasisStartTest::LpBasisStartTest()
    : model_("Box LP"),
      params_({.threads = 1, .lp_algorithm = LPAlgorithm::kPrimalSimplex}) {}

SolveStats LpBasisStartTest::SolveWithWarmStart(
    const bool is_maximize, const bool starting_basis_max_opt) {
  model_.set_is_maximize(is_maximize);
  model_.AddToObjective(objective_expression_);
  const SolveArguments args = {
      .parameters = params_,
      .model_parameters = {.initial_basis = starting_basis_max_opt
                                                ? max_optimal_basis_
                                                : min_optimal_basis_}};
  const SolveResult result = Solve(model_, TestedSolver(), args).value();
  CHECK_OK(result.termination.EnsureIsOptimal());
  return result.solve_stats;
}

SolveStats LpBasisStartTest::RoundTripSolve() {
  model_.Maximize(objective_expression_);
  const std::unique_ptr<IncrementalSolver> solver =
      NewIncrementalSolver(&model_, TestedSolver()).value();
  const SolveResult max_result = solver->Solve({.parameters = params_}).value();
  CHECK_OK(max_result.termination.EnsureIsOptimal());
  ModelSolveParameters max_model_parameters;
  max_model_parameters.initial_basis = max_result.solutions[0].basis;
  const SolveResult min_result = solver->Solve({.parameters = params_}).value();
  CHECK_OK(min_result.termination.EnsureIsOptimal());

  model_.Maximize(objective_expression_);
  const SolveResult max_result_second =
      solver
          ->Solve(
              {.parameters = params_, .model_parameters = max_model_parameters})
          .value();
  CHECK_OK(max_result_second.termination.EnsureIsOptimal());
  return max_result_second.solve_stats;
}

////////////////////////////////////////////////////////////////////////////////
// Model Blocks:
//   * Each function builds a simple model with an objective that can be
//     minimized or maximized. In both cases the model has a unique solution.
//     The functions also creates the basis for these two unique optimal
//     solutions.
//   * All functions return the distance in pivots between the maximizing and
//     minimizing basis. Models are constructed specifically so this distance
//     is the same for any pivoting rule (Those with distance > 0 have feasible
///    regions that are boxes).
//   * Models are composable: Because variables and constraints are pair-wise
//     disjoint, calling multiple functions maintains validity of models and
//     basis. The basis distance for combined models is the sum of the basis
//     distances for the models.
//   * For some models the unique basic optimal solution for maximization and
//     minimization are the same. These models need to be composed with another
//     one for testing.
////////////////////////////////////////////////////////////////////////////////

// Sets up the 2-variable/0-constraint optimization problem:
//   {min/max} x1 - x2
//   s.t. variable bounds:
//             0  <= x1 <= 1
//             0  <= x2 <= 1
//   s.t. constraints:
//             none
//
// Note that for maximizing, this problem has the unique optimal solution
//
//    x1 = 1, x2 = 0
//
// and for minimizing, this problem has the unique optimal solution
//
//    x1 = 0, x2 = 1
//
// Further, the optimal basis for maximizing and minimizing are unique as well,
// and are:
//
//   For maximizing:
//
//     {x1, BasisStatus::kAtUpperBound},
//     {x2, BasisStatus::kAtLowerBound},
//
//   For minimizing:
//
//     {x1, BasisStatus::kAtLowerBound}
//     {x2, BasisStatus::kAtUpperBound}
//
// This model covers variables at bounds statuses.
int LpBasisStartTest::SetUpVariableBoundBoxModel() {
  Variable x1 = model_.AddContinuousVariable(0, 1, "x1_variable_box");
  Variable x2 = model_.AddContinuousVariable(0, 1, "x2_variable_box");
  objective_expression_ += x1 - x2;

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kAtUpperBound);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kAtLowerBound);

  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kAtLowerBound);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kAtUpperBound);

  return 2;
}

// Sets up the 2-variable/4-constraint optimization problem:
//   {min/max}    x1 - x2
//   s.t. variable bounds:
//           -inf <= x1 <= inf
//           -inf <= x2 <= inf
//   s.t. constraints:
//                   x1 >= 0 (c1)
//                   x1 <= 1 (c2)
//                   x1 >= 0 (c3)
//                   x1 <= 1 (c4)
//
// Note that for maximizing, this problem has the unique optimal solution
//
//    x1 = 1, x2 = 0
//
// and for minimizing, this problem has the unique optimal solution
//
//    x1 = 0, x2 = 1
//
// Further, the optimal basis for maximizing and minimizing are unique as well,
// and are:
//
//   For maximizing:
//
//     {x1, BasisStatus::kBasic},
//     {x2, BasisStatus::kBasic},
//     {c1, BasisStatus::kBasic},
//     {c2, BasisStatus::kAtUpperBound},
//     {c3, BasisStatus::kAtLowerBound},
//     {c4, BasisStatus::kBasic},
//
//   For minimizing:
//
//     {x1, BasisStatus::kBasic},
//     {x2, BasisStatus::kBasic},
//     {c1, BasisStatus::kAtLowerBound},
//     {c2, BasisStatus::kBasic},
//     {c3, BasisStatus::kBasic},
//     {c4, BasisStatus::kAtUpperBound},
//
// This model covers basic variables, basic non-ranged constraints and
// non-ranged constraints at bounds statuses.
int LpBasisStartTest::SetUpConstraintBoxModel() {
  Variable x1 = model_.AddContinuousVariable(-kInf, kInf, "x1_constraint_box");
  Variable x2 = model_.AddContinuousVariable(-kInf, kInf, "x2_constraint_box");
  LinearConstraint c1 =
      model_.AddLinearConstraint(x1 >= 0, "c1_constraint_box");
  LinearConstraint c2 =
      model_.AddLinearConstraint(x1 <= 1, "c2_constraint_box");
  LinearConstraint c3 =
      model_.AddLinearConstraint(x2 >= 0, "c3_constraint_box");
  LinearConstraint c4 =
      model_.AddLinearConstraint(x2 <= 1, "c4_constraint_box");
  objective_expression_ += x1 - x2;

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kBasic);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  max_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kBasic);
  max_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kAtUpperBound);
  max_optimal_basis_.constraint_status.emplace(c3, BasisStatus::kAtLowerBound);
  max_optimal_basis_.constraint_status.emplace(c4, BasisStatus::kBasic);

  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kBasic);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  min_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kAtLowerBound);
  min_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kBasic);
  min_optimal_basis_.constraint_status.emplace(c3, BasisStatus::kBasic);
  min_optimal_basis_.constraint_status.emplace(c4, BasisStatus::kAtUpperBound);

  return 2;
}

// Sets up the 2-variable/2-constraint optimization problem:
//   {min/max}    x1 - x2
//   s.t. variable bounds:
//           -inf <= x1 <= inf
//           -inf <= x2 <= inf
//   s.t. constraints:
//              0 <= x2 <= 1 (c1)
//              0 <= x2 <= 1 (c2)
//
// Note that for maximizing, this problem has the unique optimal solution
//
//    x1 = 1, x2 = 0
//
// and for minimizing, this problem has the unique optimal solution
//
//    x1 = 0, x2 = 1
//
// Further, the optimal basis for maximizing and minimizing are unique as well,
// and are:
//
//   For maximizing:
//
//     {x1, BasisStatus::kBasic},
//     {x2, BasisStatus::kBasic},
//     {c1, BasisStatus::kAtUpperBound},
//     {c2, BasisStatus::kAtLowerBound},
//
//   For minimizing:
//
//     {x1, BasisStatus::kBasic},
//     {x2, BasisStatus::kBasic},
//     {c1, BasisStatus::kAtLowerBound},
//     {c2, BasisStatus::kAtUpperBound},
//
// This model covers basic variables and ranged constraints at bounds statuses.
int LpBasisStartTest::SetUpRangedConstraintBoxModel() {
  Variable x1 =
      model_.AddContinuousVariable(-kInf, kInf, "x1_ranged_constraint_box");
  Variable x2 =
      model_.AddContinuousVariable(-kInf, kInf, "x2_ranged_constraint_box");
  LinearConstraint c1 =
      model_.AddLinearConstraint(0 <= x1 <= 1, "c1_ranged_constraint_box");
  LinearConstraint c2 =
      model_.AddLinearConstraint(0 <= x2 <= 1, "c2_ranged_constraint_box");
  objective_expression_ += x1 - x2;

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kBasic);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  max_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kAtUpperBound);
  max_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kAtLowerBound);

  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kBasic);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  min_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kAtLowerBound);
  min_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kAtUpperBound);

  return 2;
}

// Sets up the 2-variable/1-constraint optimization problem:
//   {min/max}    x1 - x2
//   s.t. variable bounds:
//              0 <= x1 <= 1
//              0 <= x2 <= 1
//   s.t. constraints:
//        -1 <= x1 + x2 <= 3 (c1)
//
// Note that for maximizing, this problem has the unique optimal solution
//
//    x1 = 1, x2 = 0
//
// and for minimizing, this problem has the unique optimal solution
//
//    x1 = 0, x2 = 1
//
// Further, the optimal basis for maximizing and minimizing are unique as well,
// and are:
//
//   For maximizing:
//
//     {x1, BasisStatus::kAtUpperBound},
//     {x2, BasisStatus::kAtLowerBound},
//     {c1, BasisStatus::kBasic},
//
//   For minimizing:
//
//     {x1, BasisStatus::kAtLowerBound},
//     {x2, BasisStatus::kAtUpperBound},
//     {c1, BasisStatus::kBasic},
//
// This model is used to cover basic ranged constraints.
int LpBasisStartTest::SetUpBasicRangedConstraintModel() {
  Variable x1 = model_.AddContinuousVariable(0, 1, "x1_basic_ranged");
  Variable x2 = model_.AddContinuousVariable(0, 1, "x2_basic_ranged");
  LinearConstraint c1 =
      model_.AddLinearConstraint(-1 <= x1 + x2 <= 3, "c1_basic_ranged");
  objective_expression_ += x1 - x2;

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kAtUpperBound);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kAtLowerBound);
  max_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kBasic);

  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kAtLowerBound);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kAtUpperBound);
  min_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kBasic);

  return 2;
}

// Sets up the 2-variable/1-constraint optimization problem:
//   {min/max}    0
//   s.t. variable bounds:
//           -inf <= x1 <= inf
//           -inf <= x2 <= inf
//           -inf <= x3 <= inf
//   s.t. constraints:
//      -inf <= x1 + x2 <= inf (c1)
//      -inf <= x2 + x3 <= inf (c2)
//
// Note that the unique basic feasible solition for this problem is
//
//    x1 = x2 = x3 = 0
//
// Further, this solution has multiple basis. We pick the following basis for
// both directions to cover free and basic statuses for unbounded variables and
// constraints.
//
//     {x1, BasisStatus::kFree},
//     {x2, BasisStatus::kFree},
//     {x3, BasisStatus::kBasic},
//     {c1, BasisStatus::kBasic},
//     {c2, BasisStatus::kFree},
int LpBasisStartTest::SetUpUnboundedVariablesAndConstraintsModel() {
  Variable x1 = model_.AddContinuousVariable(-kInf, kInf, "x1_unbounded");
  Variable x2 = model_.AddContinuousVariable(-kInf, kInf, "x2_unbounded");
  Variable x3 = model_.AddContinuousVariable(-kInf, kInf, "x3_unbounded");
  LinearConstraint c1 =
      model_.AddLinearConstraint(-kInf <= x1 + x2 <= kInf, "c1_unbounded");
  LinearConstraint c2 =
      model_.AddLinearConstraint(-kInf <= x2 + x3 <= kInf, "c2_unbounded");

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kFree);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kFree);
  max_optimal_basis_.variable_status.emplace(x3, BasisStatus::kBasic);
  max_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kBasic);
  max_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kFree);

  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kFree);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  min_optimal_basis_.variable_status.emplace(x3, BasisStatus::kBasic);
  min_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kFree);
  min_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kFree);

  return 0;
}

// Sets up the 2-variable/0-constraint optimization problem:
//   {min/max}    0
//   s.t. variable bounds:
//           0 <= x1 <= 0
//           0 <= x2 <= 0
//           0 <= x3 <= 0
//   s.t. constraints:
//           none
//
// Note that the unique feasible solition for this problem is
//
//    x1 = x2 = x3 = 0
//
// Further, this solution has multiple basis (we can pick FIXED, AT_LOWER_BOUND,
// or AT_UPPER_BOUND for each variable). We pick the following basis for both
// directions to cover all three possible status choices.
//
//     {x1, BasisStatus::kFixedValue},
//     {x2, BasisStatus::kAtLowerBound},
//     {x2, BasisStatus::kAtUpperBound},
int LpBasisStartTest::SetUpFixedVariablesModel() {
  Variable x1 = model_.AddContinuousVariable(0, 0, "x1_fixed_variable");
  Variable x2 = model_.AddContinuousVariable(0, 0, "x2_fixed_variable");
  Variable x3 = model_.AddContinuousVariable(0, 0, "x3_fixed_variable");

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kFixedValue);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kAtLowerBound);
  max_optimal_basis_.variable_status.emplace(x3, BasisStatus::kAtLowerBound);
  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kFixedValue);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kAtLowerBound);
  min_optimal_basis_.variable_status.emplace(x3, BasisStatus::kAtUpperBound);

  return 0;
}

// Sets up the 2-variable/1-constraint optimization problem:
//   {min/max}    0
//   s.t. variable bounds:
//           -1 <= x1 <= 1
//           -1 <= x2 <= 1
//           -1 <= x3 <= 1
//   s.t. constraints:
//            x1 + x2 == 0 (c1)
//            x2 + x3 == 0 (c2)
//            x3 + x1 == 0 (c3)
//       x1 + x2 + x3 == 0 (c4)
//
// Note that the unique feasible solition for this problem is
//
//    x1 = x2 = x3 = 0
//
// Further, this solution has multiple basis (e.g. note that c4 is a redundant
// constraint). We pick the following basis for both directions to cover all
// four possible status choices for equality constraints
//
//     {x1, BasisStatus::kBasic},
//     {x2, BasisStatus::kBasic},
//     {x3, BasisStatus::kBasic},
//     {c1, BasisStatus::kFixedValue},
//     {c1, BasisStatus::kAtLowerBound},
//     {c1, BasisStatus::kAtUpperBound},
//     {c1, BasisStatus::kBasic},
int LpBasisStartTest::SetUpEqualitiesModel() {
  Variable x1 = model_.AddContinuousVariable(-1, 1, "x1_equality");
  Variable x2 = model_.AddContinuousVariable(-1, 1, "x2_equality");
  Variable x3 = model_.AddContinuousVariable(-1, 1, "x3_equality");
  LinearConstraint c1 = model_.AddLinearConstraint(x1 + x2 == 0, "c1_equality");
  LinearConstraint c2 = model_.AddLinearConstraint(x2 + x3 == 0, "c2_equality");
  LinearConstraint c3 = model_.AddLinearConstraint(x3 + x1 == 0, "c3_equality");
  LinearConstraint c4 =
      model_.AddLinearConstraint(x1 + x2 + x3 == 0, "c4_equality");

  max_optimal_basis_.variable_status.emplace(x1, BasisStatus::kBasic);
  max_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  max_optimal_basis_.variable_status.emplace(x3, BasisStatus::kBasic);
  max_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kFixedValue);
  max_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kAtLowerBound);
  max_optimal_basis_.constraint_status.emplace(c3, BasisStatus::kAtUpperBound);
  max_optimal_basis_.constraint_status.emplace(c4, BasisStatus::kBasic);

  min_optimal_basis_.variable_status.emplace(x1, BasisStatus::kBasic);
  min_optimal_basis_.variable_status.emplace(x2, BasisStatus::kBasic);
  min_optimal_basis_.variable_status.emplace(x3, BasisStatus::kBasic);
  min_optimal_basis_.constraint_status.emplace(c1, BasisStatus::kFixedValue);
  min_optimal_basis_.constraint_status.emplace(c2, BasisStatus::kAtLowerBound);
  min_optimal_basis_.constraint_status.emplace(c3, BasisStatus::kAtUpperBound);
  min_optimal_basis_.constraint_status.emplace(c4, BasisStatus::kBasic);

  return 0;
}

namespace {
TEST_P(LpBasisStartTest, EmptyModelAndBasis) {
  model_.Maximize(objective_expression_);
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, ModelWithoutVariables) {
  LinearConstraint c = model_.AddLinearConstraint("trivial equality");
  min_optimal_basis_.constraint_status.emplace(c, BasisStatus::kBasic);
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Basis distance test for individual models and full combined model:
//    * Set minimize basis
//    * Solve maximize problem
//    * Check that the number of simplex iterations is equal to the distance
//      between the maximize and minimize basis
////////////////////////////////////////////////////////////////////////////////

TEST_P(LpBasisStartTest, VariableBoundBoxModel) {
  const int basis_distance = SetUpVariableBoundBoxModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, ConstraintBoxModel) {
  const int basis_distance = SetUpConstraintBoxModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, RangedConstraintBoxModel) {
  const int basis_distance = SetUpRangedConstraintBoxModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, BasicRangedConstraintModel) {
  const int basis_distance = SetUpBasicRangedConstraintModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, UnboundedVariablesAndConstraintsModel) {
  // UnboundedVariablesAndConstraintsModel has the same optimal basic solution
  // for max and min so we compose it with SingleVariableModel.
  int basis_distance = SetUpVariableBoundBoxModel();
  basis_distance += SetUpUnboundedVariablesAndConstraintsModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, FixedVariablesModel) {
  // FixedVariablesModel has the same optimal basic solution for max and min
  // so we compose it with SingleVariableModel.
  int basis_distance = SetUpVariableBoundBoxModel();
  basis_distance += SetUpFixedVariablesModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, EqualitiesModel) {
  // EqualitiesModel has the same optimal basic solution for max and min
  // so we compose it with SingleVariableModel.
  int basis_distance = SetUpVariableBoundBoxModel();
  basis_distance += SetUpEqualitiesModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

TEST_P(LpBasisStartTest, CombinedModels) {
  // EqualitiesModel has the same optimal basic solution for max and min
  // so we compose it with SingleVariableModel.
  int basis_distance = SetUpVariableBoundBoxModel();
  basis_distance += SetUpConstraintBoxModel();
  basis_distance += SetUpRangedConstraintBoxModel();
  basis_distance += SetUpBasicRangedConstraintModel();
  basis_distance += SetUpUnboundedVariablesAndConstraintsModel();
  basis_distance += SetUpFixedVariablesModel();
  basis_distance += SetUpEqualitiesModel();
  const SolveStats stats = SolveWithWarmStart(
      /*is_maximize=*/true, /*starting_basis_max_opt=*/false);
  EXPECT_EQ(stats.simplex_iterations, basis_distance);
}

////////////////////////////////////////////////////////////////////////////////
// Roundtrip for individual models and full combined model:
//    * Solve maximize problem
//    * Save optimal basis
//    * Solve minimize problem
//    * Set saved basis
//    * Solve maximize problem
//    * Check that simplex takes zero iterations
//
// Note: The minimization solve in the middle aims to leave the solver's
// internal status at the minimization basis before setting the basis for the
// last maximization solve. If setting this basis fails (i.e. the solver
// rejects the basis), then the solver "should" start that last maximization
// solve from the minimization basis, hence taking at least one pivot and
// failing the test (this assumes the solver does not re-run preprocessing
// for this last maximization problem if the basis is rejected).
////////////////////////////////////////////////////////////////////////////////

TEST_P(LpBasisStartTest, VariableBoundBoxModelOptimalRoundtrip) {
  SetUpVariableBoundBoxModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, ConstraintBoxModelOptimalRoundtrip) {
  SetUpConstraintBoxModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, RangedConstraintBoxModelOptimalRoundtrip) {
  SetUpRangedConstraintBoxModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, BasicRangedConstraintModelOptimalRoundtrip) {
  SetUpBasicRangedConstraintModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest,
       UnboundedVariablesAndConstraintsModelOptimalRoundtrip) {
  // UnboundedVariablesAndConstraintsModel has the same optimal basic solution
  // for max and min so we compose it with SingleVariableModel.
  SetUpVariableBoundBoxModel();
  SetUpUnboundedVariablesAndConstraintsModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, FixedVariablesModelOptimalRoundtrip) {
  // FixedVariablesModel has the same optimal basic solution for max and min
  // so we compose it with SingleVariableModel.
  SetUpVariableBoundBoxModel();
  SetUpFixedVariablesModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, EqualitiesModelOptimalRoundtrip) {
  // EqualitiesModel has the same optimal basic solution for max and min
  // so we compose it with SingleVariableModel.
  SetUpVariableBoundBoxModel();
  SetUpEqualitiesModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

TEST_P(LpBasisStartTest, CombinedModelsOptimalRoundtrip) {
  // EqualitiesModel has the same optimal basic solution for max and min
  // so we compose it with SingleVariableModel.
  SetUpVariableBoundBoxModel();
  SetUpConstraintBoxModel();
  SetUpRangedConstraintBoxModel();
  SetUpBasicRangedConstraintModel();
  SetUpUnboundedVariablesAndConstraintsModel();
  SetUpFixedVariablesModel();
  SetUpEqualitiesModel();
  const SolveStats stats = RoundTripSolve();
  EXPECT_EQ(stats.simplex_iterations, 0);
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
