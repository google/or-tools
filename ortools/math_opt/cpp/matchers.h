// Copyright 2010-2022 Google LLC
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

// Matchers for MathOpt types, specifically SolveResult and nested fields.
//
// The matchers defined here are useful for writing unit tests checking that the
// result of Solve(), absl::StatusOr<SolveResult>, meets expectations. We give
// some examples below. All code is assumed with the following setup:
//
//   namespace operations_research::math_opt {
//   using ::testing::status::IsOkAndHolds;
//
//   Model model;
//   const Variable x = model.AddContinuousVariable(0.0, 1.0);
//   const Variable y = model.AddContinuousVariable(0.0, 1.0);
//   const LinearConstraint c = model.AddLinearConstraint(x + y <= 1);
//   model.Maximize(2*x + y);
//
// Example 1.a: result is OK, optimal, and objective value approximately 42.
//   EXPECT_THAT(Solve(model, SOLVER_TYPE_GLOP), IsOkAndHolds(IsOptimal(42)));
//
// Example 1.b: equivalent to 1.a.
//   ASSERT_OK_AND_ASSIGN(const SolveResult result,
//                        Solve(model, SOLVER_TYPE_GLOP));
//   EXPECT_THAT(result, IsOptimal(42));
//
// Example 2: result is OK, optimal, and best solution is x=1, y=0.
//   ASSERT_OK_AND_ASSIGN(const SolveResult result,
//                        Solve(model, SOLVER_TYPE_GLOP));
//   ASSERT_THAT(result, IsOptimal());
//   EXPECT_THAT(result.variable_value(), IsNear({{x, 1}, {y, 0}});
// Note: the second ASSERT ensures that if the solution is not optimal, then
// result.variable_value() will not run (the function will crash if the solver
// didn't find a solution). Further, MathOpt guarantees there is a solution
// when the termination reason is optimal.
//
// Example 3: result is OK, check the solution without specifying termination.
//   ASSERT_OK_AND_ASSIGN(const SolveResult result,
//                        Solve(model, SOLVER_TYPE_GLOP));
//   EXPECT_THAT(result, HasBestSolution({{x, 1}, {y, 0}}));
//
// Example 4: multiple possible termination reason, primal ray optional:
//   ASSERT_OK_AND_ASSIGN(const SolveResult result,
//                        Solve(model, SOLVER_TYPE_GLOP));
//   ASSERT_THAT(result, TerminatesWithOneOf(
//       TerminationReason::kUnbounded,
//       TerminationReason::kInfeasibleOrUnbounded));
//   if(!result.primal_rays.empty()) {
//     EXPECT_THAT(result.primal_rays[0], PrimalRayIsNear({{x, 1,}, {y, 0}}));
//   }
//
//
// Tips on writing good tests:
//   * Use ASSERT_OK_AND_ASSIGN(const SolveResult result, Solve(...)) to ensure
//     the test terminates immediately if Solve() does not return OK.
//   * If you ASSERT_THAT(result, IsOptimal()), you can assume that you have a
//     feasible primal solution afterwards. Otherwise, make no assumptions on
//     the contents of result (e.g. do not assume result contains a primal ray
//     just because the termination reason was UNBOUNDED).
//   * For problems that are infeasible, the termination reasons INFEASIBLE and
//     DUAL_INFEASIBLE are both possible. Likewise, for unbounded problems, you
//     can get both UNBOUNDED and DUAL_INFEASIBLE. See TerminatesWithOneOf()
//     below to make assertions in this case. Note also that some solvers have
//     solver specific parameters to ensure that DUAL_INFEASIBLE will not be
//     returned (e.g. for Gurobi, use DualReductions or InfUnbdInfo).
//   * The objective value and variable values should always be compared up to
//     a tolerance, even if your decision variables are integer. The matchers
//     defined have a configurable tolerance with default value 1e-5.
//   * Primal and dual rays are unique only up to a constant scaling. The
//     matchers provided rescale both expected and actual before comparing.
//   * Take care on problems with multiple optimal solutions. Do not rely on a
//     particular solution being returned in your test, as the test will break
//     when we upgrade the solver.
//
// This file also defines functions to let gunit print various MathOpt types.
//
// To see the error messages these matchers generate, run
//   blaze test experimental/users/rander/math_opt:matchers_error_messages
// which is a fork of matchers_test.cc where the assertions are all negated
// (note that every test should fail).
#ifndef OR_TOOLS_MATH_OPT_CPP_MATCHERS_H_
#define OR_TOOLS_MATH_OPT_CPP_MATCHERS_H_

#include <optional>
#include <ostream>
#include <sstream>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

constexpr double kMatcherDefaultTolerance = 1e-5;

////////////////////////////////////////////////////////////////////////////////
// Matchers for IdMap
////////////////////////////////////////////////////////////////////////////////

// Checks that the maps have identical keys and values within tolerance. This
// factory will CHECK-fail if expected contains any NaN values.
testing::Matcher<VariableMap<double>> IsNear(
    VariableMap<double> expected, double tolerance = kMatcherDefaultTolerance);

// Checks that the keys of actual are a subset of the keys of expected, and that
// for all shared keys, the values are within tolerance. This factory will
// CHECK-fail if expected contains any NaN values, and any NaN values in the
// expression compared against will result in the matcher failing.
testing::Matcher<VariableMap<double>> IsNearlySubsetOf(
    VariableMap<double> expected, double tolerance = kMatcherDefaultTolerance);

// Checks that the maps have identical keys and values within tolerance. This
// factory will CHECK-fail if expected contains any NaN values, and any NaN
// values in the expression compared against will result in the matcher failing.
testing::Matcher<LinearConstraintMap<double>> IsNear(
    LinearConstraintMap<double> expected,
    double tolerance = kMatcherDefaultTolerance);

// Checks that the keys of actual are a subset of the keys of expected, and that
// for all shared keys, the values are within tolerance. This factory will
// CHECK-fail if expected contains any NaN values, and any NaN values in the
// expression compared against will result in the matcher failing.
testing::Matcher<LinearConstraintMap<double>> IsNearlySubsetOf(
    LinearConstraintMap<double> expected,
    double tolerance = kMatcherDefaultTolerance);

// Checks that the maps have identical keys and values within tolerance. Works
// for VariableMap, LinearConstraintMap, among other realizations of IdMap. This
// factory will CHECK-fail if expected contains any NaN values, and any NaN
// values in the expression compared against will result in the matcher failing.
template <typename K>
testing::Matcher<IdMap<K, double>> IsNear(
    IdMap<K, double> expected,
    const double tolerance = kMatcherDefaultTolerance);

// Checks that the keys of actual are a subset of the keys of expected, and that
// for all shared keys, the values are within tolerance. Works for VariableMap,
// LinearConstraintMap, among other realizations of IdMap. This factory will
// CHECK-fail if expected contains any NaN values, and any NaN values in the
// expression compared against will result in the matcher failing.
template <typename K>
testing::Matcher<IdMap<K, double>> IsNearlySubsetOf(
    IdMap<K, double> expected,
    const double tolerance = kMatcherDefaultTolerance);

////////////////////////////////////////////////////////////////////////////////
// Matchers for various Variable expressions (e.g. LinearExpression)
////////////////////////////////////////////////////////////////////////////////

// Checks that the expressions are structurally identical (i.e., internal maps
// have the same keys and storage, coefficients are exactly equal). This factory
// will CHECK-fail if expected contains any NaN values, and any NaN values in
// the expression compared against will result in the matcher failing.
testing::Matcher<LinearExpression> IsIdentical(LinearExpression expected);

testing::Matcher<LinearExpression> LinearExpressionIsNear(
    LinearExpression expected, double tolerance = kMatcherDefaultTolerance);

// Checks that the bounded linear expression is equivalent to expected, where
// equivalence is maintained by:
//  * adding alpha to the lower bound, the linear expression and upper bound
//  * multiplying the lower bound, linear expression, by -1 (and flipping the
//    inequalities).
// Note that, as implemented, we do not allow for arbitrary multiplicative
// rescalings (this makes additive tolerance complicated).
testing::Matcher<BoundedLinearExpression> IsNearlyEquivalent(
    const BoundedLinearExpression& expected,
    double tolerance = kMatcherDefaultTolerance);

// Checks that the expressions are structurally identical (i.e., internal maps
// have the same keys and storage, coefficients are exactly equal). This factory
// will CHECK-fail if expected contains any NaN values, and any NaN values in
// the expression compared against will result in the matcher failing.
testing::Matcher<QuadraticExpression> IsIdentical(QuadraticExpression expected);

////////////////////////////////////////////////////////////////////////////////
// Matchers for solutions
////////////////////////////////////////////////////////////////////////////////

// Options for IsNear(Solution).
struct SolutionMatcherOptions {
  double tolerance = kMatcherDefaultTolerance;
  bool check_primal = true;
  bool check_dual = true;
  bool check_basis = true;
};

testing::Matcher<Solution> IsNear(Solution expected,
                                  SolutionMatcherOptions options = {});

// Checks variables match and variable/objective values are within tolerance and
// feasibility statuses are identical.
testing::Matcher<PrimalSolution> IsNear(
    PrimalSolution expected, double tolerance = kMatcherDefaultTolerance);

// Checks dual variables, reduced costs and objective are within tolerance and
// feasibility statuses are identical.
testing::Matcher<DualSolution> IsNear(
    DualSolution expected, double tolerance = kMatcherDefaultTolerance);

testing::Matcher<Basis> BasisIs(const Basis& expected);

////////////////////////////////////////////////////////////////////////////////
// Matchers for a Rays
////////////////////////////////////////////////////////////////////////////////

// Checks variables match and that after rescaling, variable values are within
// tolerance.
testing::Matcher<PrimalRay> IsNear(PrimalRay expected,
                                   double tolerance = kMatcherDefaultTolerance);

// Checks variables match and that after rescaling, variable values are within
// tolerance.
testing::Matcher<PrimalRay> PrimalRayIsNear(
    VariableMap<double> expected_var_values,
    double tolerance = kMatcherDefaultTolerance);

// Checks that dual variables and reduced costs are defined for the same
// set of Variables/LinearConstraints, and that their rescaled values are within
// tolerance.
testing::Matcher<DualRay> IsNear(DualRay expected,
                                 double tolerance = kMatcherDefaultTolerance);

////////////////////////////////////////////////////////////////////////////////
// Matchers for a Termination
////////////////////////////////////////////////////////////////////////////////

testing::Matcher<Termination> ReasonIs(TerminationReason reason);

testing::Matcher<Termination> ReasonIsOptimal();

////////////////////////////////////////////////////////////////////////////////
// Matchers for a SolveResult
////////////////////////////////////////////////////////////////////////////////

// Checks the following:
//  * The termination reason is optimal.
//  * If expected_objective contains a value, there is at least one feasible
//    solution and that solution has an objective value within tolerance of
//    expected_objective.
testing::Matcher<SolveResult> IsOptimal(
    std::optional<double> expected_objective = std::nullopt,
    double tolerance = kMatcherDefaultTolerance);

testing::Matcher<SolveResult> IsOptimalWithSolution(
    double expected_objective, VariableMap<double> expected_variable_values,
    double tolerance = kMatcherDefaultTolerance);

testing::Matcher<SolveResult> IsOptimalWithDualSolution(
    double expected_objective, LinearConstraintMap<double> expected_dual_values,
    VariableMap<double> expected_reduced_costs,
    double tolerance = kMatcherDefaultTolerance);

// Checks the following:
//  * The result has the expected termination reason.
testing::Matcher<SolveResult> TerminatesWith(TerminationReason expected);

// Checks that the result has one of the allowed termination reasons.
testing::Matcher<SolveResult> TerminatesWithOneOf(
    const std::vector<TerminationReason>& allowed);

// Checks the following:
//  * The result has termination reason kFeasible or kNoSolutionFound.
//  * The limit is expected, or is kUndetermined if allow_limit_undetermined.
testing::Matcher<SolveResult> TerminatesWithLimit(
    Limit expected, bool allow_limit_undetermined = false);

// Checks the following:
//  * The result has termination reason kFeasible.
//  * The limit is expected, or is kUndetermined if allow_limit_undetermined.
testing::Matcher<SolveResult> TerminatesWithReasonFeasible(
    Limit expected, bool allow_limit_undetermined = false);

// Checks the following:
//  * The result has termination reason kNoSolutionFound.
//  * The limit is expected, or is kUndetermined if allow_limit_undetermined.
testing::Matcher<SolveResult> TerminatesWithReasonNoSolutionFound(
    Limit expected, bool allow_limit_undetermined = false);

// SolveResult has a primal solution matching expected within tolerance.
testing::Matcher<SolveResult> HasSolution(
    PrimalSolution expected, double tolerance = kMatcherDefaultTolerance);

// SolveResult has a dual solution matching expected within
// tolerance.
testing::Matcher<SolveResult> HasDualSolution(
    DualSolution expected, double tolerance = kMatcherDefaultTolerance);

// Actual SolveResult contains a primal ray that matches expected within
// tolerance.
testing::Matcher<SolveResult> HasPrimalRay(
    PrimalRay expected, double tolerance = kMatcherDefaultTolerance);

// Actual SolveResult contains a primal ray with variable values equivalent to
// (under L_inf scaling) expected_vars up to tolerance.
testing::Matcher<SolveResult> HasPrimalRay(
    VariableMap<double> expected_vars,
    double tolerance = kMatcherDefaultTolerance);

// Actual SolveResult contains a dual ray that matches expected within
// tolerance.
testing::Matcher<SolveResult> HasDualRay(
    DualRay expected, double tolerance = kMatcherDefaultTolerance);

// Configures SolveResult matcher IsConsistentWith() below.
struct SolveResultMatcherOptions {
  double tolerance = 1e-5;
  bool first_solution_only = true;
  bool check_dual = true;
  bool check_rays = true;

  // If the expected result has termination reason kInfeasible, kUnbounded, or
  // kDualInfeasasible, the primal solution, dual solution, and basis are
  // ignored unless check_solutions_if_inf_or_unbounded is true.
  //
  // TODO(b/201099290): this is perhaps not a good default. Gurobi as
  //  implemented is returning primal solutions for both unbounded and
  //  infeasible problems. We need to add unit tests that inspect this value
  //  and turn them on one solver at a time with a new parameter on
  //  SimpleLpTestParameters.
  bool check_solutions_if_inf_or_unbounded = false;
  bool check_basis = false;

  // In linear programming, the following outcomes are all possible
  //
  //    Primal LP    | Dual LP    | Possible MathOpt Termination Reasons
  //    -----------------------------------------------------------------
  // 1. Infeasible   | Unbounded  | kInfeasible
  // 2. Optimal      | Optimal    | kOptimal
  // 3. Unbounded    | Infeasible | kUnbounded, kInfeasibleOrUnbounded
  // 4. Infeasible   | Infeasible | kInfeasible, kInfeasibleOrUnbounded
  //
  // (Above "Optimal" means that an optimal solution exists. This is a statement
  // about the existence of optimal solutions and certificates of
  // infeasibility/unboundedness, not about the outcome of applying any
  // particular algorithm.)
  //
  // When writing your unit test, you can typically tell which case of 1-4 you
  // are in, but in cases 3-4 you do not know which termination reason will be
  // returned. In some situations, it may not be clear if you are in case 1 or
  // case 4 as well.
  //
  // When inf_or_unb_soft_match=false, the matcher must exactly specify the
  // status returned by the solver. For cases 3-4, this is implementation
  // dependent and we do not recommend this. When
  // inf_or_unb_soft_match=true:
  //   * kInfeasible can also match kInfeasibleOrUnbounded
  //   * kUnbounded can also match kInfeasibleOrUnbounded
  //   * kInfeasibleOrUnbounded can also match kInfeasible and kUnbounded.
  // For case 2, inf_or_unb_soft_match has no effect.
  //
  // To build the strongest possible matcher (accepting the minimal set of
  // termination reasons):
  //   * If you know you are in case 1, se inf_or_unb_soft_match=false
  //     (soft_match=true over-matches)
  //   * For case 3, use inf_or_unb_soft_match=false and
  //     termination_reason=kUnbounded (kInfeasibleOrUnbounded over-matches).
  //   * For case 4 (or if you are unsure of case 1 vs case 4), use
  //     inf_or_unb_soft_match=true and
  //     termination_reason=kInfeasible (kInfeasibleOrUnbounded over-matches).
  //   * If you cannot tell if you are in case 3 or case 4, use
  //     inf_or_unb_soft_match=true and termination reason
  //     kInfeasibleOrUnbounded.
  //
  // If the above is too complicated, always setting
  // inf_or_unb_soft_match=true and using any of the expected MathOpt
  // termination reasons from the above table will give a matcher that is
  // slightly too lenient.
  bool inf_or_unb_soft_match = true;
};

// Tests that two SolveResults are equivalent. Basic use:
//
// SolveResult expected;
// // Fill in expected...
// ASSERT_OK_AND_ASSIGN(SolveResult actual, Solve(model, solver_type));
// EXPECT_THAT(actual, IsConsistentWith(expected));
//
// Equivalence is defined as follows:
//   * The termination reasons are the same.
//       - For infeasible and unbounded problems, see
//         options.inf_or_unb_soft_match.
//   * The solve stats are ignored.
//   * For both primal and dual solutions, either expected and actual are
//     both empty, or their first entries satisfy IsNear() at options.tolerance.
//       - Not checked if options.check_solutions_if_inf_or_unbounded and the
//         problem is infeasible or unbounded (default).
//       - If options.first_solution_only is false, check the entire list of
//         solutions matches in the same order.
//       - Dual solution is not checked if options.check_dual=false
//   * For both the primal and dual rays, either expected and actual are both
//     empty, or any ray in expected IsNear() any ray in actual (which is up
//     to a rescaling) at options.tolerance.
//       - Not checked if options.check_rays=false
//       - If options.first_solution_only is false, check the entire list of
//         solutions matches in the same order.
//   * The basis is not checked by default. If enabled, checked with BasisIs().
//       - Enable with options.check_basis
//
// This function is symmetric in that:
//   EXPECT_THAT(actual, IsConsistentWith(expected));
//   EXPECT_THAT(expected, IsConsistentWith(actual));
// agree on matching, they only differ in strings produced. Per gmock
// conventions, prefer the former.
//
// For problems with either primal or dual infeasibility, see
// SolveResultMatcherOptions::inf_or_unb_soft_match for guidance on how to
// best set the termination reason and inf_or_unb_soft_match.
testing::Matcher<SolveResult> IsConsistentWith(
    const SolveResult& expected, const SolveResultMatcherOptions& options = {});

////////////////////////////////////////////////////////////////////////////////
// Rarely used
////////////////////////////////////////////////////////////////////////////////

// Actual UpdateResult.did_update is true.
testing::Matcher<UpdateResult> DidUpdate();

////////////////////////////////////////////////////////////////////////////////
// Implementation details
////////////////////////////////////////////////////////////////////////////////

// TODO(b/200835670): use the << operator on Termination instead once it
//  supports quoting/escaping on termination.detail.
void PrintTo(const Termination& termination, std::ostream* os);
void PrintTo(const PrimalSolution& primal_solution, std::ostream* os);
void PrintTo(const DualSolution& dual_solution, std::ostream* os);
void PrintTo(const PrimalRay& primal_ray, std::ostream* os);
void PrintTo(const DualRay& dual_ray, std::ostream* os);
void PrintTo(const Basis& basis, std::ostream* os);
void PrintTo(const Solution& solution, std::ostream* os);
void PrintTo(const SolveResult& result, std::ostream* os);

// We do not want to rely on ::testing::internal::ContainerPrinter because we
// want to sort the keys.
template <typename K, typename V>
void PrintTo(const IdMap<K, V>& id_map, std::ostream* const os) {
  constexpr int kMaxPrint = 10;
  int num_added = 0;
  *os << "{";
  for (const K k : id_map.SortedKeys()) {
    if (num_added > 0) {
      *os << ", ";
    }
    if (num_added >= kMaxPrint) {
      *os << "...(size=" << id_map.size() << ")";
      break;
    }
    *os << "{" << k << ", " << ::testing::PrintToString(id_map.at(k)) << "}";
    ++num_added;
  }
  *os << "}";
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_MATCHERS_H_
