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

#include "ortools/math_opt/solver_tests/lp_incomplete_solve_tests.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solver_tests/test_models.h"

namespace operations_research {
namespace math_opt {

LpIncompleteSolveTestParams::LpIncompleteSolveTestParams(
    const SolverType solver_type, const std::optional<LPAlgorithm> lp_algorithm,
    const bool supports_iteration_limit, const bool supports_initial_basis,
    const bool supports_incremental_solve, const bool supports_basis,
    const bool supports_presolve, const bool check_primal_objective,
    const bool primal_solution_status_always_set,
    const bool dual_solution_status_always_set)
    : solver_type(solver_type),
      lp_algorithm(lp_algorithm),
      supports_iteration_limit(supports_iteration_limit),
      supports_initial_basis(supports_initial_basis),
      supports_incremental_solve(supports_incremental_solve),
      supports_basis(supports_basis),
      supports_presolve(supports_presolve),
      check_primal_objective(check_primal_objective),
      primal_solution_status_always_set(primal_solution_status_always_set),
      dual_solution_status_always_set(dual_solution_status_always_set) {}

std::ostream& operator<<(std::ostream& out,
                         const LpIncompleteSolveTestParams& params) {
  out << "{ solver_type: " << params.solver_type
      << " lp_algorithm: " << params.lp_algorithm
      << " supports_iteration_limit: " << params.supports_iteration_limit
      << " supports_initial_basis: " << params.supports_initial_basis
      << " supports_incremental_solve:" << params.supports_incremental_solve
      << " supports_basis:" << params.supports_basis
      << " supports_presolve:" << params.supports_presolve
      << " check_primal_objective:" << params.check_primal_objective
      << " primal_solution_status_always_set:"
      << params.primal_solution_status_always_set
      << " dual_solution_status_always_set:"
      << params.dual_solution_status_always_set;

  return out;
}
using ::testing::AnyOf;
using ::testing::ElementsAreArray;
using ::testing::Gt;
using ::testing::Lt;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kTolerance = 1e-6;

namespace {

// This tests only assumes that there is a non-optimal primal-dual pair of
// appropriate dimensions and hence should work for most algorithms.
TEST_P(LpIncompleteSolveTest, SimpleTest) {
  if (!GetParam().supports_iteration_limit) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for iteration limit.";
  }

  const int n = 10;
  const std::unique_ptr<Model> model =
      IndependentSetCompleteGraph(/*integer=*/false, n);

  SolveArguments args;
  args.parameters.threads = 1;
  args.parameters.lp_algorithm = GetParam().lp_algorithm;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }
  args.parameters.iteration_limit = 2;

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, TestedSolver(), args));
  ASSERT_THAT(result, TerminatesWithLimit(Limit::kIteration,
                                          /*allow_limit_undetermined=*/true));
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  EXPECT_THAT(SortedKeys(result.solutions[0].primal_solution->variable_values),
              ElementsAreArray(model->SortedVariables()));
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  EXPECT_THAT(SortedKeys(result.solutions[0].dual_solution->reduced_costs),
              ElementsAreArray(model->SortedVariables()));
  EXPECT_EQ(SortedKeys(result.solutions[0].dual_solution->dual_values),
            model->SortedLinearConstraints());
}

bool ApproxEq(const double first_double, const double second_double,
              double tolerance = kTolerance) {
  return std::abs(first_double - second_double) <= tolerance;
}

// The following detailed simplex tests require parameters that may not be
// supported by all simplex solvers.

// Algorithm: Dual simplex.
// Start: Primal/dual infeasible basis with feasible dual solution
// End: Primal infeasible and dual feasible basis.
//
// Primal model:
// min     x[0] + ... + x[n - 1]
// s.t.
// Constraints:             -1 <= x[i] <= 1  (y[i])   for all i in {0,...,n - 1}
// Variable bounds:         -2 <= x[i] <= 2  (r[i])   for all i in {0,...,n - 1}
//
// Dual model (go/mathopt-dual):
//
// max -|y[0]| + ... + -|y[n- 1]| + -2|r[0]| + ... + -2|r[n - 1]|
//
//        y[i] + r[i] == 1 for all i in {0,...,n - 1}
//
// Optimal solution:
//
// The unique primal/dual optimal pair is
//   * x[i] = -1 for all i in {0,...,n - 1}
//   * y[i] =  1 for all i in {0,...,n - 1}
//   * r[i] =  0 for all i in {0,...,n - 1}
//
// All basis can be described by disjoint subsets N1, P1, N2, P2 of
// {0,...,n - 1} that describes the basis and solutions as follows (The sets
// indicate the variables fixed at -1, 1, -2 and 2 respectively):
//    * x[i] = -1 for all i in N1, x[i] = 1 for all i in P1, x[i] = -2 for all i
//      in N2, and x[i] = 2 for all i in N2.
//    * r[i] = 0 for all i in N1 or P1, r[i] = 1 for all i in N2 or P2.
//    * y[i] = 1 for all i in N1 or P1, y[i] = 0 for all i in N2 or P2.
//    * x[i] is BASIC for all i in N1 or P1, x[i] is AT_UPPER_BOUND for all i in
//       P2, and x[i] is AT_LOWER_BOUND for all i in N2.
//    * the constraint associated to y[i] is BASIC for all i in N2 or P2,
//      AT_UPPER_BOUND for all i in P1, and AT_LOWER_BOUND for all i in N1.
//
// We have the following feasibility conditions:
//    * A basis is primal feasible if and only if both N2 and P2 are empty,
//    * a basis is dual feasible if both P2 and P1 are empty, but
//    * the dual solution associated to any basis is feasible.
//
// Test:
//
// We initialize the solver to start at solution x[i] = 2 for all i in
// {0,...,n - 1} using initial basis (i.e. P2 = {0, ..., n - 1}). We then set
// an iteration limit that should allow at least one pivot away from this
// solution, but which is not long enough to reach a primal feasible solution.
// Finally, we check that the primal and dual solution (and basis if supported)
// obtained under this iteration limit corresponts to a basis with empty P1 and
// P2 and with 1 < |N1|, |N2| < n.
//
// Note: this test assumes the dual simplex algorithms implements dual
// feasibility correction that (in the first iteration) switches all the x[i]
// AT_UPPER_BOUND to AT_LOWER_BOUND to match the sign of r[i].
//
// TODO(b/208230589): Simplify tests by adding a matcher function that checks
// basic consistency of the primal, dual and basis.
TEST_P(LpIncompleteSolveTest, DualSimplexInfeasibleBasis) {
  if (GetParam().lp_algorithm != LPAlgorithm::kDualSimplex ||
      !GetParam().supports_iteration_limit ||
      !GetParam().supports_initial_basis) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for dual simplex, "
           "iteration limit and initial basis.";
  }
  int n = 10;
  Model model("DualSimplexInfeasibleBasis");
  std::vector<Variable> x;
  std::vector<LinearConstraint> c;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(-2.0, 2.0));
    c.push_back(model.AddLinearConstraint(-1.0 <= x[i] <= 1.0));
  }
  model.Minimize(Sum(x));
  SolveArguments args;
  args.parameters.lp_algorithm = GetParam().lp_algorithm;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }
  Basis initial_basis;
  for (int i = 0; i < n; ++i) {
    initial_basis.variable_status.emplace(x[i], BasisStatus::kAtUpperBound);
    initial_basis.constraint_status.emplace(c[i], BasisStatus::kBasic);
  }
  args.model_parameters.initial_basis = initial_basis;

  args.parameters.iteration_limit = 3;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), args));
  ASSERT_THAT(result, TerminatesWithReasonNoSolutionFound(
                          Limit::kIteration,
                          /*allow_limit_undetermined=*/true));
  if (GetParam().supports_basis) {
    ASSERT_TRUE(result.has_basis());
    if (GetParam().dual_solution_status_always_set) {
      EXPECT_EQ(result.solutions[0].basis->basic_dual_feasibility,
                SolutionStatus::kFeasible);
    } else {
      EXPECT_NE(result.solutions[0].basis->basic_dual_feasibility,
                SolutionStatus::kInfeasible);
    }
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  int n1_variables = 0;
  int p1_variables = 0;
  int n2_variables = 0;
  int p2_variables = 0;
  for (int i = 0; i < n; ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    const double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    const double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    const double dual_value =
        result.solutions[0].dual_solution->dual_values.at(c[i]);
    BasisStatus expected_variable_status;
    BasisStatus expected_constraint_status;
    if (ApproxEq(variable_value, -1.0)) {
      ++n1_variables;
      EXPECT_NEAR(reduced_cost, 0.0, kTolerance);
      EXPECT_NEAR(dual_value, 1.0, kTolerance);
      expected_variable_status = BasisStatus::kBasic;
      expected_constraint_status = BasisStatus::kAtLowerBound;
    } else if (ApproxEq(variable_value, 1.0)) {
      ++p1_variables;
      EXPECT_NEAR(reduced_cost, 0.0, kTolerance);
      EXPECT_NEAR(dual_value, 1.0, kTolerance);
      expected_variable_status = BasisStatus::kBasic;
      expected_constraint_status = BasisStatus::kAtUpperBound;
    } else if (ApproxEq(variable_value, -2.0)) {
      ++n2_variables;
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
      EXPECT_NEAR(dual_value, 0.0, kTolerance);
      expected_variable_status = BasisStatus::kAtLowerBound;
      expected_constraint_status = BasisStatus::kBasic;
    } else {
      EXPECT_NEAR(variable_value, 2.0, kTolerance);
      ++p2_variables;
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
      EXPECT_NEAR(dual_value, 0.0, kTolerance);
      expected_variable_status = BasisStatus::kAtUpperBound;
      expected_constraint_status = BasisStatus::kBasic;
    }
    if (GetParam().supports_basis) {
      ASSERT_TRUE(result.has_basis());
      EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                expected_variable_status);
      EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                expected_constraint_status);
    }
  }
  EXPECT_EQ(p1_variables, 0);
  EXPECT_EQ(p2_variables, 0);
  EXPECT_GT(n2_variables, 1);
  EXPECT_GT(n1_variables, 1);
  EXPECT_LT(n2_variables, n);
  EXPECT_LT(n1_variables, n);
  if (GetParam().primal_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  }
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  if (GetParam().dual_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].dual_solution->feasibility_status,
              SolutionStatus::kFeasible);
  } else {
    EXPECT_NE(result.solutions[0].dual_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  }
  if (GetParam().check_primal_objective) {
    EXPECT_NEAR(-1 * n1_variables - 2 * n2_variables,
                result.solutions[0].primal_solution->objective_value,
                kTolerance);
    if (GetParam().supports_basis &&
        GetParam().dual_solution_status_always_set) {
      // Here we know that the basis is dual feasible as checked above, so the
      // primal and dual objective values match. See
      // go/mathopt-basis-advanced#cs-obj-dual-feasible-dual-feasible-basis
      ASSERT_TRUE(
          result.solutions[0].dual_solution->objective_value.has_value());
      EXPECT_NEAR(-1 * n1_variables - 2 * n2_variables,
                  *result.solutions[0].dual_solution->objective_value,
                  kTolerance);
    }
  } else {
    LOG(INFO) << "Skipping primal objective check as solver does not "
                 "reliably support it for incomplete solves.";
  }
}

// Algorithm: Primal simplex.
// Start: Primal/dual infeasible basis with feasible dual solution.
// End: Primal/dual infeasible basis with feasible dual solution.
//
// Primal model:
// min     x[0] + ... + x[n - 1]
// s.t.
// Constraints:   x[0] + ... + x[n - 1] <= 1  (y)
// Variable bounds:          0 <= x[i] <= 2  (r[i])   for all i in {0,...,n - 1}
//
// Dual model (go/mathopt-dual):
//
// max    {y : y < 0} + 2 {r[0] : r[0] < 0} + ... + 2 {r[n - 1] : r[n - 1] < 0}
//
//        y + r[i] == 1 for all i in {0,...,n - 1}
//               y <= 0
//
// Optimal solution:
//
// The unique primal/dual optimal pair is
//   * x[i] = 0 for all i in {0,...,n - 1}
//   * y    = 0
//   * r[i] = 1 for all i in {0,...,n - 1}
//
// Basic solutions defined by bounds:
//
// All basis with a basic y can be described by a subset I of {0,...,n - 1} that
// describes the basis and solutions as follows (I indicates variables at their
// upper bounds of 2):
//    * x[i] = 2 for all i in I, x[i] = 0 for all i not in I.
//    * r[i] = 1 for all i in {0, ..., n - 1}.
//    * x[i] is AT_UPPER_BOUND for all i in I, x[i] is AT_LOWER_BOUND for all i
//      not in I.
//    * y = 0.
//    * the constraint associated to y is BASIC.
//
// All basis with a basic y are primal and dual infeasible, except for the one
// associated to an empty I, which is optimal. However, all basis with a basic y
// yield the same dual solution, which is dual feasible.
//
// Test:
//
// We initialize the solver to start at solution x[i] = 2 for all i in
// {0,...,n - 1} using initial basis (i.e. I = {0, ..., n}). We then set an
// iteration limit that should allow at least one pivot away from this solution,
// but which is not long enough to reach a primal feasile solution. Finally, we
// check that the primal and dual solution (and basis if supported) obtained
// under this iteration limit corresponts to a basis I with 1 < |I| < n (i.e.
// with k = |I| variables at 2 for 0 < k < n).
TEST_P(LpIncompleteSolveTest, PrimalSimplexInfeasibleBasis) {
  if (GetParam().lp_algorithm != LPAlgorithm::kPrimalSimplex ||
      !GetParam().supports_iteration_limit ||
      !GetParam().supports_initial_basis) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for primal simplex,"
           " iteration limit and initial basis.";
  }
  int n = 10;
  Model model("PrimalSimplexInfeasibleBasis");
  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, 2.0));
  }
  LinearConstraint c = model.AddLinearConstraint(Sum(x) <= 1);
  model.Minimize(Sum(x));
  SolveArguments args;
  args.parameters.lp_algorithm = GetParam().lp_algorithm;

  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }
  Basis initial_basis;
  for (int i = 0; i < n; ++i) {
    initial_basis.variable_status.emplace(x[i], BasisStatus::kAtUpperBound);
  }
  initial_basis.constraint_status.emplace(c, BasisStatus::kBasic);
  args.model_parameters.initial_basis = initial_basis;

  args.parameters.iteration_limit = 3;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), args));
  ASSERT_THAT(result, TerminatesWithReasonNoSolutionFound(
                          Limit::kIteration,
                          /*allow_limit_undetermined=*/true));
  if (GetParam().supports_basis) {
    ASSERT_TRUE(result.has_basis());
    EXPECT_NE(result.solutions[0].basis->basic_dual_feasibility,
              SolutionStatus::kFeasible);
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  int variable_values_at_two = 0;
  const double dual_value =
      result.solutions[0].dual_solution->dual_values.at(c);
  EXPECT_NEAR(dual_value, 0.0, kTolerance);
  if (GetParam().supports_basis) {
    EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c),
              BasisStatus::kBasic);
  }
  for (int i = 0; i < n; ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    const double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    const double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    // Gurobi returns a value of -999,999 or 999,999 for these reduced costs.
    // TODO(b/195295177): Create a simple example to file a bug with Gurobi.
    if (TestedSolver() != SolverType::kGurobi) {
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
    }
    BasisStatus expected_status;
    if (ApproxEq(variable_value, 2.0)) {
      ++variable_values_at_two;
      expected_status = BasisStatus::kAtUpperBound;
    } else {
      expected_status = BasisStatus::kAtLowerBound;
    }
    if (GetParam().supports_basis) {
      EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                expected_status);
    }
  }
  EXPECT_GT(variable_values_at_two, 1);
  EXPECT_LT(variable_values_at_two, n);
  if (GetParam().primal_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  }
  // Dual solution is feasible, but basis is dual infeasible so most solvers
  // will return kUndetermined instead of kFeasible
  EXPECT_NE(result.solutions[0].dual_solution->feasibility_status,
            SolutionStatus::kInfeasible);

  if (GetParam().check_primal_objective) {
    EXPECT_NEAR(2 * variable_values_at_two,
                result.solutions[0].primal_solution->objective_value,
                kTolerance);
  } else {
    LOG(INFO) << "Skipping primal objective check as solver does not "
                 "reliably support it for incomplete solves.";
  }
}

// Algorithm: Primal simplex.
// Start: Primal feasible and dual infeasible basis.
// End: Primal feasible and dual infeasible basis.
//
// Primal model:
// max     x[0] + ... + x[n - 1]
// s.t.
// Constraints:                 x[i] <= 1  (y[i])   for all i in {0,...,n - 1}
// Variable bounds:        0 <= x[i]       (r[i])   for all i in {0,...,n - 1}
//
// Dual model (go/mathopt-dual):
//
// min    y[0] + ... + y[n - 1]
//
//        y[i] + r[i] == 1 for all i in {0,...,n - 1}
//               y[i] >= 0 for all i in {0,...,n - 1}
//               r[i] <= 0 for all i in {0,...,n - 1}
//
// Optimal solution:
//
// The unique primal/dual optimal pair is
//   * x[i] = 1 for all i in {0,...,n - 1}
//   * y[i] = 1 for all i in {0,...,n - 1}
//   * r[i] = 0 for all i in {0,...,n - 1}
//
// Basic solutions:
//
// All basis can be described by a subset I of {0,...,n  - 1} that describes the
// basis and solutions as follows (I indicates variables at their upper bounds):
//    * x[i] = 1 for all i in I, x[i] = 0 for all i not in I.
//    * r[i] = 0 for all i in I, r[i] = 1 for all i not in I.
//    * x[i] is BASIC for all i in I, x[i] is AT_LOWER_BOUND for all i not in I.
//    * y[i] = 1 for all i in I, y[i] = 0 for all i not in I.
//    * the constraint associated to y[i] is AT_UPPER_BOUND for all i in I, and
//      BASIC for all i not in I.
//
// All basis are primal feasible, but only I = {0,...,n - 1} is dual feasible.
//
// Test:
//
// We initialize the solver to start at solution x[i] = 0 for all i in
// {0,...,n - 1} using initial basis or by minimizing the objective. We then set
// an iteration limit that should allow at least one pivot away from this
// solution, but which is not long enough to reach the optimal solution x[i] = 1
// for all i. Finally, we check that the primal and dual solution (and basis if
// supported) obtained under this iteration limit corresponts to a basis I with
// 0 < |I| < n (i.e. with k variables at 1 for 0 < k < n).
TEST_P(LpIncompleteSolveTest, PrimalSimplexAlgorithm) {
  if (GetParam().lp_algorithm != LPAlgorithm::kPrimalSimplex ||
      !GetParam().supports_iteration_limit ||
      !(GetParam().supports_incremental_solve ||
        GetParam().supports_initial_basis)) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for primal simplex, "
           "iteration limit and either incremental solve or initial basis.";
  }
  int n = 10;
  Model model("Primal Feasible Incomplete Solve LP");
  std::vector<Variable> x;
  std::vector<LinearConstraint> c;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, kInf));
    c.push_back(model.AddLinearConstraint(x[i] <= 1));
  }

  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<IncrementalSolver> incremental_solver,
      NewIncrementalSolver(&model, TestedSolver()));
  SolveArguments args;
  args.parameters.lp_algorithm = GetParam().lp_algorithm;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }

  if (GetParam().supports_initial_basis) {
    Basis initial_basis;
    for (int i = 0; i < n; ++i) {
      initial_basis.variable_status.emplace(x[i], BasisStatus::kAtLowerBound);
      initial_basis.constraint_status.emplace(c[i], BasisStatus::kBasic);
    }
    args.model_parameters.initial_basis = initial_basis;
  } else {
    model.Minimize(Sum(x));
    ASSERT_OK(incremental_solver->Solve(args));
  }

  model.Maximize(Sum(x));
  args.parameters.iteration_limit = 3;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       incremental_solver->Solve(args));
  if (GetParam().primal_solution_status_always_set) {
    ASSERT_THAT(result, TerminatesWithReasonFeasible(
                            Limit::kIteration,
                            /*allow_limit_undetermined=*/true));
  } else {
    ASSERT_THAT(result, TerminatesWithLimit(Limit::kIteration,
                                            /*allow_limit_undetermined=*/true));
  }
  if (GetParam().supports_basis) {
    ASSERT_TRUE(result.has_basis());
    if (GetParam().dual_solution_status_always_set) {
      EXPECT_EQ(result.solutions[0].basis->basic_dual_feasibility,
                SolutionStatus::kInfeasible);
    } else {
      EXPECT_NE(result.solutions[0].basis->basic_dual_feasibility,
                SolutionStatus::kFeasible);
    }
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  int variable_values_at_one = 0;
  for (int i = 0; i < n; ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    double dual_value = result.solutions[0].dual_solution->dual_values.at(c[i]);
    if (std::abs(variable_value - 1.0) <= kTolerance) {
      ++variable_values_at_one;
      EXPECT_NEAR(reduced_cost, 0.0, kTolerance);
      EXPECT_NEAR(dual_value, 1.0, kTolerance);
      if (GetParam().supports_basis) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kBasic);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kAtUpperBound);
      }
    } else {
      EXPECT_NEAR(variable_value, 0.0, kTolerance);
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
      EXPECT_NEAR(dual_value, 0.0, kTolerance);
      if (GetParam().supports_basis) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtLowerBound);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kBasic);
      }
    }
  }
  EXPECT_GT(variable_values_at_one, 0);
  EXPECT_LT(variable_values_at_one, n);
  if (GetParam().primal_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  }
  EXPECT_NE(result.solutions[0].dual_solution->feasibility_status,
            SolutionStatus::kFeasible);
  if (GetParam().check_primal_objective) {
    EXPECT_NEAR(variable_values_at_one,
                result.solutions[0].primal_solution->objective_value,
                kTolerance);
  } else {
    LOG(INFO) << "Skipping primal objective check as solver does not "
                 "reliably support it for incomplete solves.";
  }
}

// Primal model:
// max     x[0] + ... + x[n]
// s.t.
// Constraints:            x[0] + ... + x[n] >= 1  (y)
// Variable bounds:                0 <= x[i] <= 2  (r[i]) for all i in {0,...,n}
//
// Dual model (go/mathopt-dual):
//
// min    y + 2 * r[1] + ... + 2 * r[n]
//
//        y + r[i] == 1 for all i in {1,...,n}
//               y <= 0
//
// Basic solutions:
//
// All basis can be described by disjoint subsets I, J of {1,...,n} such that
// 0 <= |J| <= 1 (I indicates variables at their upper bounds, and J indicates a
// possible basic variable).
//
//   If |J| = 0 then the basis corresponds to
//    * x[i] = 2 for all i in I, x[i] = 0 for all i not in I.
//    * r[i] = 1 for all i in {1,...,n}.
//    * x[i] is AT_UPPER_BOUND for all i in I, and x[i] is AT_LOWER_BOUND for
//      all i not in I.
//    * y = 0.
//    * the constraint associated to y is BASIC.
//    * this basis is primal feasible if the associated primal solution
//      satisfies the constraint associated to y.
//
//   If |J| = 1 then the basis corresponds to
//    * x[i] = 2 for all i in I, x[i] = 0 for all i not in I or J, and x[i] for
//      i in J is obtained by enforcing equality in the constraint associated to
//      y.
//    * r[i] = 0 for all i in {1,...,n}.
//    * x[i] is BASIC for all i in J, x[i] is AT_UPPER_BOUND for all i in I, and
//      x[i] is AT_LOWER_BOUND for all i not in I or J.
//    * y = 1.
//    * the constraint associated to y is AT_LOWER_BOUND.
//    * this basis is primal feasible if the value of 0 <= x[i] <= 2 for i in J.
//
// The only dual-feasible basis is I = {1,...,n} (with |J| = 0). However, the
// dual solutions for all basis with |J| = 0 are feasible (for more details on
// this apparent contradiction see go/mathopt-basis#dual and
// go/mathopt-basis-advanced).
// Test:
//
// We initialize the solver to start at an arbitrary solution with x[i] in
// {0, 1} and x[1] + ... + x[n] = 1 using initial basis or by minimizing the
// objective. We then set an iteration limit that should allow at least one
// pivot away from this solution, but which is not long enough to reach the
// optimal solution x[i] = 2 for all i. Finally, we check that the primal and
// dual solution (and basis if supported) obtained under this iteration limit
// corresponts to a basis with |J| = 0, and 0 < |I| < n (i.e. with k variables
// at 2 for 0 < k < n).
TEST_P(LpIncompleteSolveTest, PrimalSimplexAlgorithmRanged) {
  if (GetParam().lp_algorithm != LPAlgorithm::kPrimalSimplex ||
      !GetParam().supports_iteration_limit ||
      !(GetParam().supports_incremental_solve ||
        GetParam().supports_initial_basis)) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for primal simplex, "
           "iteration limit and either incremental solve or initial basis.";
  }
  int n = 10;
  Model model("Primal Feasible Incomplete Solve LP");
  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, 2.0));
  }
  LinearConstraint c = model.AddLinearConstraint(Sum(x) >= 1);
  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<IncrementalSolver> incremental_solver,
      NewIncrementalSolver(&model, TestedSolver()));

  SolveArguments args;
  args.parameters.lp_algorithm = LPAlgorithm::kPrimalSimplex;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }

  if (GetParam().supports_initial_basis) {
    Basis initial_basis;
    initial_basis.variable_status.emplace(x[0], BasisStatus::kBasic);
    for (int i = 1; i < n; ++i) {
      initial_basis.variable_status.emplace(x[i], BasisStatus::kAtLowerBound);
    }
    initial_basis.constraint_status.emplace(c, BasisStatus::kAtLowerBound);
    args.model_parameters.initial_basis = initial_basis;
  } else {
    model.Minimize(Sum(x));
    ASSERT_OK(incremental_solver->Solve(args));
  }
  model.Maximize(Sum(x));
  args.parameters.iteration_limit = 3;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       incremental_solver->Solve(args));
  if (GetParam().primal_solution_status_always_set) {
    ASSERT_THAT(result, TerminatesWithReasonFeasible(
                            Limit::kIteration,
                            /*allow_limit_undetermined=*/true));
  } else {
    ASSERT_THAT(result, TerminatesWithLimit(Limit::kIteration,
                                            /*allow_limit_undetermined=*/true));
  }
  if (GetParam().supports_basis) {
    EXPECT_TRUE(result.has_basis());
    EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c),
              BasisStatus::kBasic);
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  int variable_values_at_two = 0;
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  EXPECT_NEAR(result.solutions[0].dual_solution->dual_values.at(c), 0.0,
              kTolerance);
  for (int i = 0; i < n; ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    const double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    const double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    // Gurobi is not consistent with reduced cost signs in this test. For some
    // variables AT_UPPER_BOUND with value 2.0 it returns a reduced cost of
    // -1.0 and for some it returns 1.0.
    // TODO(b/195295177): Create a simple example to file a bug with Gurobi.
    if (TestedSolver() != SolverType::kGurobi) {
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
    }
    if (std::abs(variable_value - 2.0) <= kTolerance) {
      ++variable_values_at_two;
      if (GetParam().supports_basis && result.has_basis()) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtUpperBound);
      }
    } else {
      if (GetParam().supports_basis && result.has_basis()) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtLowerBound);
      }
    }
  }
  EXPECT_GT(variable_values_at_two, 0);
  EXPECT_LT(variable_values_at_two, n);
  // We only check the primal feasibility status. As noted above, while the
  // expected basis is not dual-feasible, the expected dual solution is
  // feasible. Most solvers evaluate dual feasibility with respect to the
  // basis and hence return an infeasible status for the dual solution.
  if (GetParam().primal_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  }
  if (GetParam().check_primal_objective) {
    EXPECT_NEAR(2 * variable_values_at_two,
                result.solutions[0].primal_solution->objective_value,
                kTolerance);
  } else {
    LOG(INFO) << "Skipping primal objective check as solver does not "
                 "reliably support it for incomplete solves.";
  }
}

// Primal model:
// max     x[0] + ... + x[n]
// s.t.
// Constraints:                 x[i] <= 1  (y[i])   for all i in {0,...,n}
// Variable bounds:        0 <= x[i] <= 2  (r[i])   for all i in {0,...,n}
//
// Dual model (go/mathopt-dual):
//
// min    y[0] + ... + y[n]
//
//        y[i] + r[i] == 1 for all i in {1,...,n}
//               y[i] >= 0 for all i in {1,...,n}
//
// Basic solutions:
//
// All basis can be described by a subset I1, I2 of {1,...,n} that describes the
// basis and solutions as follows (I1 indicates variables at 1 and I2 indicates
// variables at 2):
//    * x[i] = 1 for all i in I1, x[i] = 2 for all i in I2, x[i] = 0 for all i
//      not in I1 or I2.
//    * r[i] = 0 for all i in I1, r[i] = 1 for all i not in I1.
//    * x[i] is BASIC for all i in I1, x[i] is AT_UPPER_BOUND for all i in I2,
//      x[i] is AT_LOWER_BOUND for all i not in I1 or I2.
//    * y[i] = 1 for all i in I1, y[i] = 0 for all i not in I1.
//    * the constraint associated to y[i] is AT_UPPER_BOUND for all i in I1, and
//      BASIC for all i not in I1.
//
// All basis are dual feasible, but only basis with empty I2 are primal
// feasible.
//
// Test:
//
// We initialize the solver to start at solution x[i] = 2 for all i in {1,...,n}
// using initial basis (I2 = {1,..,n}). We then set an iteration limit that
// should allow at least one pivot away from this solution, but which is not
// long enough to reach the optimal solution x[i] = 1 for all i. Finally, we
// check that the primal and dual solution (and basis if supported) obtained
// under this iteration limit corresponts to a basis (I1,I2) with 0 < |I2| < n
// and |I1| + |I2| = n (i.e. with k variables at 2 and n-k variables at 1 for
// 0 < k < n).
TEST_P(LpIncompleteSolveTest, DualSimplexAlgorithmInitialBasis) {
  if (GetParam().lp_algorithm != LPAlgorithm::kDualSimplex ||
      !GetParam().supports_iteration_limit ||
      !GetParam().supports_initial_basis) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for dual simplex, "
           "iteration limit and initial basis.";
  }
  int n = 10;
  Model model("Dual Feasible Incomplete Solve LP");
  std::vector<Variable> x;
  std::vector<LinearConstraint> c;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, 2.0));
    c.push_back(model.AddLinearConstraint(x[i] <= 1));
  }
  model.Maximize(Sum(x));

  SolveArguments args;
  args.parameters.lp_algorithm = LPAlgorithm::kDualSimplex;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }
  Basis initial_basis;
  for (int i = 0; i < n; ++i) {
    initial_basis.variable_status.emplace(x[i], BasisStatus::kAtUpperBound);
    initial_basis.constraint_status.emplace(c[i], BasisStatus::kBasic);
  }
  args.model_parameters.initial_basis = initial_basis;
  args.parameters.iteration_limit = 3;

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), args));
  ASSERT_THAT(result, TerminatesWithReasonNoSolutionFound(
                          Limit::kIteration,
                          /*allow_limit_undetermined=*/true));
  if (GetParam().supports_basis) {
    EXPECT_TRUE(result.has_basis());
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  int variable_values_at_two = 0;
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  for (int i = 0; i < n; ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    const double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    const double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    const double dual_value =
        result.solutions[0].dual_solution->dual_values.at(c[i]);
    if (std::abs(variable_value - 2.0) <= kTolerance) {
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
      EXPECT_NEAR(dual_value, 0.0, kTolerance);
      ++variable_values_at_two;
      if (GetParam().supports_basis && result.has_basis()) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtUpperBound);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kBasic);
      }
    } else {
      EXPECT_NEAR(variable_value, 1.0, kTolerance);
      EXPECT_NEAR(reduced_cost, 0.0, kTolerance);
      EXPECT_NEAR(dual_value, 1.0, kTolerance);
      if (GetParam().supports_basis && result.has_basis()) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kBasic);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kAtUpperBound);
      }
    }
  }
  EXPECT_GT(variable_values_at_two, 0);
  EXPECT_LT(variable_values_at_two, n);
  if (GetParam().primal_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  }
  if (GetParam().dual_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].dual_solution->feasibility_status,
              SolutionStatus::kFeasible);
  } else {
    EXPECT_NE(result.solutions[0].dual_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  }
  if (GetParam().check_primal_objective) {
    EXPECT_NEAR(2 * variable_values_at_two + 1 * (n - variable_values_at_two),
                result.solutions[0].primal_solution->objective_value,
                kTolerance);
  } else {
    LOG(INFO) << "Skipping primal objective check as solver does not "
                 "reliably support it for incomplete solves.";
  }
}

// This test is identical to DetailedDualSimplexAlgorithmInitialBasis, but
// instead of using initial basis to set the starting dual-feasible and
// primal-infeasible basis it use incremental solve to get the desired effect.
// This is achieved by first solving the problem without the x[i] <= 2
// constraints, adding those constraints and re-solving using and incremental
// solve.
TEST_P(LpIncompleteSolveTest, DualSimplexAlgorithmIncrementalCut) {
  if (GetParam().lp_algorithm != LPAlgorithm::kDualSimplex ||
      !GetParam().supports_iteration_limit ||
      !GetParam().supports_incremental_solve) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for dual simplex, "
           "iteration limit and incremental solves.";
  }
  int n = 10;
  Model model("dual Feasible Incomplete Solve LP");
  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, 2.0));
  }
  model.Maximize(Sum(x));
  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<IncrementalSolver> incremental_solver,
      NewIncrementalSolver(&model, TestedSolver()));

  ASSERT_OK(incremental_solver->Solve());

  std::vector<LinearConstraint> c;
  for (int i = 0; i < n; ++i) {
    c.push_back(model.AddLinearConstraint(x[i] <= 1));
  }
  SolveArguments args;
  args.parameters.lp_algorithm = LPAlgorithm::kDualSimplex;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }
  args.parameters.iteration_limit = 3;

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       incremental_solver->Solve(args));
  ASSERT_THAT(result, TerminatesWithReasonNoSolutionFound(
                          Limit::kIteration,
                          /*allow_limit_undetermined=*/true));
  if (GetParam().supports_basis) {
    EXPECT_TRUE(result.has_basis());
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  // TODO(b/195295177): reduce duplicated code using matchers.
  int variable_values_at_two = 0;
  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  for (int i = 0; i < n; ++i) {
    double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    double dual_value = result.solutions[0].dual_solution->dual_values.at(c[i]);
    if (std::abs(variable_value - 2.0) <= kTolerance) {
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
      EXPECT_NEAR(dual_value, 0.0, kTolerance);
      ++variable_values_at_two;
      if (GetParam().supports_basis && result.has_basis()) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtUpperBound);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kBasic);
      }
    } else {
      EXPECT_NEAR(variable_value, 1.0, kTolerance);
      EXPECT_NEAR(reduced_cost, 0.0, kTolerance);
      EXPECT_NEAR(dual_value, 1.0, kTolerance);
      if (GetParam().supports_basis && result.has_basis()) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kBasic);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kAtUpperBound);
      }
    }
  }
  EXPECT_GT(variable_values_at_two, 0);
  EXPECT_LT(variable_values_at_two, n);
  if (GetParam().primal_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  }
  if (GetParam().dual_solution_status_always_set) {
    EXPECT_EQ(result.solutions[0].dual_solution->feasibility_status,
              SolutionStatus::kFeasible);
  } else {
    EXPECT_NE(result.solutions[0].dual_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  }
  if (GetParam().check_primal_objective) {
    EXPECT_NEAR(2 * variable_values_at_two + 1 * (n - variable_values_at_two),
                result.solutions[0].primal_solution->objective_value,
                kTolerance);
  } else {
    LOG(INFO) << "Skipping primal objective check as solver does not "
                 "reliably support it for incomplete solves.";
  }
}

// Algorithm: Dual simplex.
// Start: Primal feasible and dual infeasible basis.
// End: Primal feasible and dual infeasible basis.
//
// Primal model:
// max     x[0] + ... + x[n - 1]
// s.t.
// Constraints:                 x[i] <= 1  (y[i])   for all i in {0,...,n - 1}
// Variable bounds:        0 <= x[i]       (r[i])   for all i in {0,...,n - 1}
//
// Dual model (go/mathopt-dual):
//
// min    y[0] + ... + y[n - 1]
//
//        y[i] + r[i] == 1 for all i in {0,...,n - 1}
//               y[i] >= 0 for all i in {0,...,n - 1}
//               r[i] <= 0 for all i in {0,...,n - 1}
//
// Optimal solution:
//
// The unique primal/dual optimal pair is
//   * x[i] = 1 for all i in {0,...,n - 1}
//   * y[i] = 1 for all i in {0,...,n - 1}
//   * r[i] = 0 for all i in {0,...,n - 1}
//
// Basic solutions:
//
// All basis can be described by a subset I of {0,...,n  - 1} that describes the
// basis and solutions as follows (I indicates variables at their upper bounds):
//    * x[i] = 1 for all i in I, x[i] = 0 for all i not in I.
//    * r[i] = 0 for all i in I, r[i] = 1 for all i not in I.
//    * x[i] is BASIC for all i in I, x[i] is AT_LOWER_BOUND for all i not in I.
//    * y[i] = 1 for all i in I, y[i] = 0 for all i not in I.
//    * the constraint associated to y[i] is AT_UPPER_BOUND for all i in I, and
//      BASIC for all i not in I.
//
// All basis are primal feasible, but only I = {0,...,n - 1} is dual feasible.
//
// Test:
//
// We initialize the solver to start at solution x[i] = 0 for all i in
// {0,...,n - 1} using initial basis. We then set an iteration limit that may
// prevent phase I of dual simplex to terminate.
TEST_P(LpIncompleteSolveTest, PhaseIDualSimplexAlgorithm) {
  if (GetParam().lp_algorithm != LPAlgorithm::kDualSimplex ||
      !GetParam().supports_iteration_limit ||
      !GetParam().supports_initial_basis) {
    GTEST_SKIP()
        << "Ignoring this test as it requires support for dual simplex, "
           "iteration limit and initial basis.";
  }
  const int n = 10;
  Model model("Dual Phase I Incomplete Solve LP");
  std::vector<Variable> x;
  std::vector<LinearConstraint> c;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, kInf));
    c.push_back(model.AddLinearConstraint(x[i] <= 1));
  }

  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<IncrementalSolver> incremental_solver,
      NewIncrementalSolver(&model, TestedSolver()));
  SolveArguments args;
  args.parameters.lp_algorithm = GetParam().lp_algorithm;
  if (GetParam().supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }

  Basis initial_basis;
  for (int i = 0; i < n; ++i) {
    initial_basis.variable_status.emplace(x[i], BasisStatus::kAtLowerBound);
    initial_basis.constraint_status.emplace(c[i], BasisStatus::kBasic);
  }
  args.model_parameters.initial_basis = initial_basis;

  model.Maximize(Sum(x));
  args.parameters.iteration_limit = 3;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       incremental_solver->Solve(args));
  ASSERT_THAT(result, TerminatesWithLimit(Limit::kIteration,
                                          /*allow_limit_undetermined=*/true));

  ASSERT_FALSE(result.solutions.empty());
  ASSERT_TRUE(result.solutions[0].primal_solution.has_value());
  ASSERT_TRUE(result.solutions[0].dual_solution.has_value());
  bool primal_feasible = true;
  bool dual_feasible = true;
  int variable_values_at_one = 0;
  for (int i = 0; i < n; ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    const double variable_value =
        result.solutions[0].primal_solution->variable_values.at(x[i]);
    const double reduced_cost =
        result.solutions[0].dual_solution->reduced_costs.at(x[i]);
    const double dual_value =
        result.solutions[0].dual_solution->dual_values.at(c[i]);
    if (std::abs(variable_value - 1.0) <= kTolerance) {
      ++variable_values_at_one;
      EXPECT_NEAR(reduced_cost, 0.0, kTolerance);
      EXPECT_NEAR(dual_value, 1.0, kTolerance);
      if (GetParam().supports_basis) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kBasic);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kAtUpperBound);
      }
    } else if (std::abs(variable_value) <= kTolerance) {
      dual_feasible = false;
      EXPECT_NEAR(reduced_cost, 1.0, kTolerance);
      EXPECT_NEAR(dual_value, 0.0, kTolerance);
      if (GetParam().supports_basis) {
        ASSERT_TRUE(result.has_basis());
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtLowerBound);
        EXPECT_EQ(result.solutions[0].basis->constraint_status.at(c[i]),
                  BasisStatus::kBasic);
      }
    } else {
      EXPECT_THAT(variable_value, AnyOf(Lt(0.0), Gt(1.0)));
      primal_feasible = false;
      if (reduced_cost > kTolerance || dual_value < -kTolerance) {
        dual_feasible = false;
      }
      // TODO(b/195295177): Gurobi's dual simplex returns a value of
      // AT_UPPER_BOUND here. This was thought to be a bug, but it is actually
      // consistent with Gurobi's phase I dual simplex and the issue described
      // in b/201099290. Need to explore more.
      if (TestedSolver() == SolverType::kGurobi) {
        EXPECT_EQ(result.solutions[0].basis->variable_status.at(x[i]),
                  BasisStatus::kAtUpperBound);
      }
    }
  }
  EXPECT_FALSE(dual_feasible);
  EXPECT_GT(variable_values_at_one, 0);
  EXPECT_LT(variable_values_at_one, n);
  if (GetParam().supports_basis) {
    ASSERT_TRUE(result.has_basis());
    if (!dual_feasible) {
      if (GetParam().dual_solution_status_always_set) {
        EXPECT_EQ(result.solutions[0].basis->basic_dual_feasibility,
                  SolutionStatus::kInfeasible);
      } else {
        EXPECT_NE(result.solutions[0].basis->basic_dual_feasibility,
                  SolutionStatus::kFeasible);
      }
    }
  } else {
    LOG(INFO) << "Skipping basis check as solver does not return a basis.";
  }
  if (primal_feasible) {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kInfeasible);
  } else {
    EXPECT_NE(result.solutions[0].primal_solution->feasibility_status,
              SolutionStatus::kFeasible);
  }
  EXPECT_NE(result.solutions[0].dual_solution->feasibility_status,
            SolutionStatus::kFeasible);
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
